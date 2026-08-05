#include "apg_terminal/application/editor.hpp"
#include "apg_terminal/domain/project_document.hpp"
#include "apg_terminal/io/session.hpp"
#include "apg_terminal/ui/studio.hpp"

#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Rendered {
    ftxui::Screen screen;
    std::string   text;
};

Rendered render(const ftxui::Component &component, int width, int height, std::pair<int, int> &terminal_size) {
    terminal_size = {width, height};
    ftxui::Screen screen(width, height);
    ftxui::Render(screen, component->Render());
    auto text = screen.ToString();
    assert(std::count(text.begin(), text.end(), '\n') + 1 == static_cast<std::size_t>(height));
    assert(text.find("\xEF\xBF\xBD") == std::string::npos);
    return {std::move(screen), std::move(text)};
}

std::optional<std::pair<int, int>>
locate_ascii(const ftxui::Screen &screen, const std::string &needle, bool last = false) {
    std::optional<std::pair<int, int>> result;
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x + static_cast<int>(needle.size()) <= screen.dimx(); ++x) {
            bool matches = true;
            for (std::size_t index = 0; index < needle.size(); ++index) {
                if (screen.CellAt(x + static_cast<int>(index), y).character != needle.substr(index, 1)) {
                    matches = false;
                    break;
                }
            }
            if (matches) {
                result = std::pair{x, y};
                if (!last)
                    return result;
            }
        }
    }
    return result;
}

std::optional<std::pair<int, int>> locate_character(const ftxui::Screen &screen, const std::string &character) {
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x < screen.dimx(); ++x) {
            if (screen.CellAt(x, y).character == character)
                return std::pair{x, y};
        }
    }
    return std::nullopt;
}

std::vector<std::pair<int, int>> signal_cells(const ftxui::Screen &screen) {
    std::vector<std::pair<int, int>> result;
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x < screen.dimx(); ++x) {
            const auto &character = screen.CellAt(x, y).character;
            if (character == ">")
                result.emplace_back(x, y);
        }
    }
    return result;
}

ftxui::Event left_mouse(int x, int y, ftxui::Mouse::Motion motion) {
    ftxui::Mouse mouse;
    mouse.button = ftxui::Mouse::Left;
    mouse.motion = motion;
    mouse.x      = x;
    mouse.y      = y;
    return ftxui::Event::Mouse("", mouse);
}

ftxui::Event wheel_event(int x, int y, ftxui::Mouse::Button button) {
    ftxui::Mouse mouse;
    mouse.button = button;
    mouse.motion = ftxui::Mouse::Pressed;
    mouse.x      = x;
    mouse.y      = y;
    return ftxui::Event::Mouse("", mouse);
}

double gain(const apg::terminal::ProjectEditor &editor) {
    const auto *node = editor.document().find_node("gain1");
    assert(node);
    const auto found = std::find_if(
        node->parameter_specs.begin(), node->parameter_specs.end(),
        [](const apg::terminal::Parameter &parameter) { return parameter.name == "gain"; }
    );
    assert(found != node->parameter_specs.end());
    return found->value;
}

std::string read_file(const std::string &path) {
    std::ifstream input(path, std::ios::binary);
    assert(input);
    return {(std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>()};
}

apg::terminal::ApgPackageDocument parallel_document() {
    auto package                = nlohmann::ordered_json::parse(read_file("test/fixtures/packages-v1/simple-gain.apg"));
    package["manifest"]["id"]   = "parallel-layout";
    package["manifest"]["name"] = "Parallel Layout";
    package["workspace"]["entryProject"] = "projects-v2/parallel-gain.project.v2.yaml";
    package["workspace"]["files"]        = nlohmann::ordered_json::array({
        {
         {"path", "projects-v2/parallel-gain.project.v2.yaml"},
         {"role", "project"},
         {"content", read_file("test/fixtures/projects-v2/parallel-gain.project.v2.yaml")},
         },
        {
         {"path", "units-v2/simple_gain.unit.v2.yaml"},
         {"role", "unit"},
         {"content", read_file("test/fixtures/units-v2/simple_gain.unit.v2.yaml")},
         },
        {
         {"path", "units-v2/path_panner_2.unit.v2.yaml"},
         {"role", "unit"},
         {"content", read_file("test/fixtures/units-v2/path_panner_2.unit.v2.yaml")},
         },
        {
         {"path", "units-v2/path_mixer_2.unit.v2.yaml"},
         {"role", "unit"},
         {"content", read_file("test/fixtures/units-v2/path_mixer_2.unit.v2.yaml")},
         },
    });
    return apg::terminal::ApgPackageDocument::parse(package.dump(), "parallel-layout.apg");
}

std::optional<std::pair<int, int>> locate_graph_ascii(const ftxui::Screen &screen, const std::string &needle) {
    const int max_x = screen.dimx() >= 120 ? 120 : screen.dimx();
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x + static_cast<int>(needle.size()) <= max_x; ++x) {
            bool matches = true;
            for (std::size_t index = 0; index < needle.size(); ++index) {
                if (screen.CellAt(x + static_cast<int>(index), y).character != needle.substr(index, 1)) {
                    matches = false;
                    break;
                }
            }
            if (matches)
                return std::pair{x, y};
        }
    }
    return std::nullopt;
}

std::optional<std::pair<int, int>>
locate_node_card(const ftxui::Screen &screen, const std::string &title, const std::string &id) {
    const int  max_x    = screen.dimx() >= 120 ? 120 : screen.dimx();
    const auto check_id = id.substr(0, std::min<std::size_t>(id.size(), 4));
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x + static_cast<int>(title.size()) <= max_x; ++x) {
            bool matches_title = true;
            for (std::size_t index = 0; index < title.size(); ++index) {
                if (screen.CellAt(x + static_cast<int>(index), y).character != title.substr(index, 1)) {
                    matches_title = false;
                    break;
                }
            }
            if (!matches_title)
                continue;
            for (int dy = 1; dy <= 3 && y + dy < screen.dimy(); ++dy) {
                for (int x2 = std::max(0, x - 5); x2 + static_cast<int>(check_id.size()) <= max_x && x2 <= x + 5;
                     ++x2) {
                    bool matches_id = true;
                    for (std::size_t index = 0; index < check_id.size(); ++index) {
                        if (screen.CellAt(x2 + static_cast<int>(index), y + dy).character !=
                            check_id.substr(index, 1)) {
                            matches_id = false;
                            break;
                        }
                    }
                    if (matches_id)
                        return std::pair{x, y};
                }
            }
        }
    }
    return std::nullopt;
}

int assert_five_row_card(const ftxui::Screen &screen, const std::string &title, const std::string &id) {
    auto card_position = locate_node_card(screen, title, id);
    if (!card_position)
        card_position = locate_graph_ascii(screen, title);
    assert(card_position);
    for (int dy = 0; dy <= 2; ++dy) {
        const int min_x = std::max(0, card_position->first - 8);
        const int max_x = std::min(screen.dimx(), card_position->first + static_cast<int>(title.size()) + 8);
        for (int x = min_x; x < max_x; ++x) {
            const auto &c = screen.CellAt(x, card_position->second + dy).character;
            if (c == ">") {
                return card_position->second + dy;
            }
        }
    }
    return card_position->second;
}

void assert_compact_graph_shape(const Rendered &rendered) {
    const auto title = locate_ascii(rendered.screen, "Simple Gain");
    const auto id    = locate_ascii(rendered.screen, "gain1");
    assert(title);
    assert(id);
    const int center = assert_five_row_card(rendered.screen, "Simple Gain", "gain1");
    for (const auto &[x, y] : signal_cells(rendered.screen)) {
        (void)x;
        assert(y == center);
    }
}

void assert_parallel_alignment() {
    auto                            document = parallel_document();
    apg::terminal::ProjectEditor    editor(std::move(document));
    apg::terminal::FakeAudioSession audio;
    std::pair                       terminal_size{180, 42};
    auto       studio   = apg::terminal::studio_component(editor, audio, [] {}, [&] { return terminal_size; });
    const auto rendered = render(studio, 180, 42, terminal_size);

    const int  pan_center   = assert_five_row_card(rendered.screen, "Pan 2", "parallel_pan");
    const int  mix_center   = assert_five_row_card(rendered.screen, "Mix 2", "parallel_mix");
    const int  boost_center = assert_five_row_card(rendered.screen, "Simple Gain", "boost");
    const auto pan          = locate_ascii(rendered.screen, "Pan 2");
    const auto mix          = locate_ascii(rendered.screen, "Mix 2");
    const auto boost        = locate_ascii(rendered.screen, "boost");
    assert(pan);
    assert(mix);
    assert(boost);

    const auto signals = signal_cells(rendered.screen);
    assert(!signals.empty());
    const auto before_pan = std::min_element(signals.begin(), signals.end(), [&](const auto &left, const auto &right) {
        const int left_distance  = left.first < pan->first ? pan->first - left.first : rendered.screen.dimx();
        const int right_distance = right.first < pan->first ? pan->first - right.first : rendered.screen.dimx();
        return left_distance < right_distance;
    });
    const auto after_mix  = std::min_element(signals.begin(), signals.end(), [&](const auto &left, const auto &right) {
        const int left_distance  = left.first > mix->first ? left.first - mix->first : rendered.screen.dimx();
        const int right_distance = right.first > mix->first ? right.first - mix->first : rendered.screen.dimx();
        return left_distance < right_distance;
    });
    assert(before_pan != signals.end());
    assert(after_mix != signals.end());
    assert(before_pan->first < pan->first);
    assert(after_mix->first > mix->first);
    assert(before_pan->second == pan_center);
    assert(after_mix->second == mix_center);
    assert(std::any_of(signals.begin(), signals.end(), [&](const auto &signal) {
        return signal.first < boost->first && signal.second == boost_center;
    }));
    assert(std::any_of(signals.begin(), signals.end(), [&](const auto &signal) {
        return signal.first > boost->first && signal.second == boost_center;
    }));
}

void assert_wide_unit_drag() {
    auto document = apg::terminal::ApgPackageDocument::load("test/fixtures/packages-v1/simple-gain.apg");
    apg::terminal::ProjectEditor    editor(std::move(document));
    apg::terminal::FakeAudioSession audio;
    std::pair                       terminal_size{120, 32};
    auto       studio  = apg::terminal::studio_component(editor, audio, [] {}, [&] { return terminal_size; });
    const auto initial = render(studio, 120, 32, terminal_size);
    const auto unit    = locate_ascii(initial.screen, "gain_unit");
    const auto route   = locate_character(initial.screen, ">");
    assert(unit);
    assert(route);

    const auto initial_nodes = editor.document().nodes().size();
    assert(studio->OnEvent(left_mouse(unit->first, unit->second, ftxui::Mouse::Pressed)));
    render(studio, 120, 32, terminal_size);
    assert(studio->OnEvent(left_mouse(route->first, route->second, ftxui::Mouse::Moved)));
    const auto hovered = render(studio, 120, 32, terminal_size);
    assert(hovered.screen.CellAt(route->first, route->second).character == ">");
    assert(studio->OnEvent(left_mouse(route->first, route->second, ftxui::Mouse::Released)));
    assert(editor.document().nodes().size() == initial_nodes + 1);
    assert(editor.can_undo());
    assert(editor.undo());
    assert(editor.document().nodes().size() == initial_nodes);
}

void assert_compact_unit_pickup() {
    auto document = apg::terminal::ApgPackageDocument::load("test/fixtures/packages-v1/simple-gain.apg");
    apg::terminal::ProjectEditor    editor(std::move(document));
    apg::terminal::FakeAudioSession audio;
    std::pair                       terminal_size{80, 24};
    bool                            exit_requested = false;
    auto                            studio =
        apg::terminal::studio_component(editor, audio, [&] { exit_requested = true; }, [&] { return terminal_size; });

    const auto initial_nodes = editor.document().nodes().size();
    assert(studio->OnEvent(ftxui::Event::TabReverse));
    const auto units = render(studio, 80, 24, terminal_size);
    const auto unit  = locate_ascii(units.screen, "gain_unit");
    assert(unit);
    assert(studio->OnEvent(left_mouse(unit->first, unit->second, ftxui::Mouse::Pressed)));
    assert(studio->OnEvent(left_mouse(unit->first, unit->second, ftxui::Mouse::Released)));

    auto graph = render(studio, 80, 24, terminal_size);
    assert(graph.text.find("Placing Simple Gain") != std::string::npos);
    const auto route = locate_character(graph.screen, ">");
    assert(route);
    assert(studio->OnEvent(left_mouse(route->first, route->second + 1, ftxui::Mouse::Pressed)));
    assert(studio->OnEvent(left_mouse(route->first, route->second + 1, ftxui::Mouse::Released)));
    assert(editor.document().nodes().size() == initial_nodes);
    graph = render(studio, 80, 24, terminal_size);
    assert(graph.text.find("Placing Simple Gain") != std::string::npos);
    assert(studio->OnEvent(left_mouse(route->first, route->second - 1, ftxui::Mouse::Pressed)));
    assert(studio->OnEvent(left_mouse(route->first, route->second - 1, ftxui::Mouse::Released)));
    assert(editor.document().nodes().size() == initial_nodes);
    graph = render(studio, 80, 24, terminal_size);
    assert(graph.text.find("Placing Simple Gain") != std::string::npos);
    assert(studio->OnEvent(left_mouse(route->first + 3, route->second, ftxui::Mouse::Pressed)));
    assert(studio->OnEvent(left_mouse(route->first + 3, route->second, ftxui::Mouse::Released)));
    assert(editor.document().nodes().size() == initial_nodes);
    graph = render(studio, 80, 24, terminal_size);
    assert(graph.text.find("Placing Simple Gain") != std::string::npos);

    assert(studio->OnEvent(left_mouse(route->first, route->second, ftxui::Mouse::Pressed)));
    assert(studio->OnEvent(left_mouse(route->first, route->second, ftxui::Mouse::Released)));
    assert(editor.document().nodes().size() == initial_nodes + 1);
    assert(editor.undo());
    assert(editor.document().nodes().size() == initial_nodes);

    assert(studio->OnEvent(ftxui::Event::TabReverse));
    const auto units_again = render(studio, 80, 24, terminal_size);
    const auto unit_again  = locate_ascii(units_again.screen, "gain_unit");
    assert(unit_again);
    assert(studio->OnEvent(left_mouse(unit_again->first, unit_again->second, ftxui::Mouse::Pressed)));
    assert(studio->OnEvent(left_mouse(unit_again->first, unit_again->second, ftxui::Mouse::Released)));
    assert(studio->OnEvent(ftxui::Event::Escape));
    assert(!exit_requested);
    const auto cancelled = render(studio, 80, 24, terminal_size);
    assert(
        cancelled.text.find("Unit placement cancelled") != std::string::npos ||
        cancelled.text.find("placement cancelled") != std::string::npos ||
        cancelled.text.find("Placement cancelled") != std::string::npos
    );
}

std::string strip_ansi_codes(const std::string &input) {
    std::string result;
    bool        in_escape = false;
    for (char character : input) {
        if (character == '\033') {
            in_escape = true;
        } else if (in_escape) {
            if ((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z')) {
                in_escape = false;
            }
        } else {
            result += character;
        }
    }
    return result;
}

void assert_screen_dump() {
    {
        auto document = apg::terminal::ApgPackageDocument::load("test/fixtures/packages-v1/simple-gain.apg");
        apg::terminal::ProjectEditor    editor(std::move(document));
        apg::terminal::FakeAudioSession audio;
        std::pair                       terminal_size{132, 43};
        auto       studio   = apg::terminal::studio_component(editor, audio, [] {}, [&] { return terminal_size; });
        const auto rendered = render(studio, 132, 43, terminal_size);
        const auto clean    = strip_ansi_codes(rendered.screen.ToString());

        assert(clean.find("APG Studio simple-gain-board v2.0.0") != std::string::npos);
        assert(clean.find("Simple Gain") != std::string::npos);
        assert(clean.find("gain1") != std::string::npos);
        assert(clean.find("simple_gain") != std::string::npos);
        assert(clean.find("Unity") != std::string::npos);
        assert(clean.find("Boost") != std::string::npos);
    }
    {
        auto document = apg::terminal::ApgPackageDocument::load("test/fixtures/packages-v1/parallel-setup.apg");
        apg::terminal::ProjectEditor    editor(std::move(document));
        apg::terminal::FakeAudioSession audio;
        std::pair                       terminal_size{132, 43};
        auto studio = apg::terminal::studio_component(editor, audio, [] {}, [&] { return terminal_size; });

        studio->OnEvent(ftxui::Event::ArrowRight);
        studio->OnEvent(ftxui::Event::ArrowRight);
        studio->OnEvent(ftxui::Event::ArrowRight);

        const auto rendered = render(studio, 132, 43, terminal_size);
        const auto clean    = strip_ansi_codes(rendered.screen.ToString());

        assert(clean.find("Pan 2") != std::string::npos);
        assert(clean.find("Plexi Tone Stage") != std::string::npos);
        assert(clean.find("tone1") != std::string::npos);
        assert(clean.find("tone_stack") != std::string::npos);
    }
}

void assert_debug_snapshot() {
    auto document = apg::terminal::ApgPackageDocument::load("test/fixtures/packages-v1/parallel-setup.apg");
    apg::terminal::ProjectEditor    editor(std::move(document));
    apg::terminal::FakeAudioSession audio;
    std::pair                       terminal_size{120, 40};
    auto studio = apg::terminal::studio_component(editor, audio, [] {}, [&] { return terminal_size; }, true);

    studio->OnEvent(ftxui::Event::Special("\x04"));
    const auto debug_rendered = render(studio, 120, 40, terminal_size);
    const auto clean          = strip_ansi_codes(debug_rendered.screen.ToString());

    assert(clean.find("Debug Snapshot") != std::string::npos);
    assert(clean.find("APG-TUI DEBUG SNAPSHOT") != std::string::npos);
}

std::string snapshot_selected_node_id(const ftxui::Component &studio, std::pair<int, int> &terminal_size) {
    assert(studio->OnEvent(ftxui::Event::Special("\x04")));
    const auto debug  = render(studio, terminal_size.first, terminal_size.second, terminal_size);
    const auto clean  = strip_ansi_codes(debug.screen.ToString());
    const auto marker = std::string("Selected Node ID: ");
    const auto pos    = clean.find(marker);
    assert(pos != std::string::npos);
    std::string value;
    for (std::size_t i = pos + marker.size(); i < clean.size(); ++i) {
        const char character = clean[i];
        if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '_' || character == '-') {
            value += character;
        } else {
            break;
        }
    }
    assert(!value.empty());
    assert(studio->OnEvent(ftxui::Event::Escape));
    return value;
}

void assert_graph_wheel_cycles_route_order() {
    auto document = apg::terminal::ApgPackageDocument::load("test/fixtures/packages-v1/guitar-pedalboard.apg");
    apg::terminal::ProjectEditor    editor(std::move(document));
    apg::terminal::FakeAudioSession audio;
    std::pair                       terminal_size{132, 43};
    auto studio = apg::terminal::studio_component(editor, audio, [] {}, [&] { return terminal_size; }, true);

    render(studio, 132, 43, terminal_size);
    assert(snapshot_selected_node_id(studio, terminal_size) == "gate1");

    assert(studio->OnEvent(wheel_event(60, 20, ftxui::Mouse::WheelDown)));
    assert(snapshot_selected_node_id(studio, terminal_size) == "phaser");
    assert(studio->OnEvent(wheel_event(60, 20, ftxui::Mouse::WheelDown)));
    assert(snapshot_selected_node_id(studio, terminal_size) == "drive1");

    for (int i = 0; i < 5; ++i)
        assert(studio->OnEvent(wheel_event(60, 20, ftxui::Mouse::WheelDown)));
    assert(snapshot_selected_node_id(studio, terminal_size) == "reverb1");

    assert(studio->OnEvent(wheel_event(60, 20, ftxui::Mouse::WheelDown)));
    assert(snapshot_selected_node_id(studio, terminal_size) == "reverb1");
    assert(studio->OnEvent(wheel_event(60, 20, ftxui::Mouse::WheelUp)));
    assert(snapshot_selected_node_id(studio, terminal_size) == "delay1");
}

void assert_both_channels_empty_alignment() {
    auto document = apg::terminal::ApgPackageDocument::load("test/fixtures/packages-v1/parallel-empty-empty.apg");
    apg::terminal::ProjectEditor    editor(std::move(document));
    apg::terminal::FakeAudioSession audio;
    std::pair                       terminal_size{120, 36};
    auto studio = apg::terminal::studio_component(editor, audio, [] {}, [&] { return terminal_size; });
    const auto rendered = render(studio, 120, 36, terminal_size);
    const auto clean    = strip_ansi_codes(rendered.screen.ToString());

    assert(clean.find("APG Studio parallel-empty-empty v2.0.0") != std::string::npos);
    assert(clean.find("│ IN │") != std::string::npos);
    assert(clean.find("│ OUT │") != std::string::npos);
    assert(clean.find("┌───>────┐") != std::string::npos);
    assert(clean.find("│ IN │──>──║ Pan 2 ║") != std::string::npos);
    assert(clean.find("│ Mix 2 │──>──│ OUT │") != std::string::npos);
    assert(clean.find("└───>────┘") != std::string::npos);
    assert(clean.find("Pan 2") != std::string::npos);
    assert(clean.find("Mix 2") != std::string::npos);
}

void assert_one_channel_empty_alignment() {
    auto document = apg::terminal::ApgPackageDocument::load("test/fixtures/packages-v1/parallel-empty-preamp.apg");
    apg::terminal::ProjectEditor    editor(std::move(document));
    apg::terminal::FakeAudioSession audio;
    std::pair                       terminal_size{120, 36};
    auto studio = apg::terminal::studio_component(editor, audio, [] {}, [&] { return terminal_size; });
    const auto rendered = render(studio, 120, 36, terminal_size);
    const auto clean    = strip_ansi_codes(rendered.screen.ToString());

    assert(clean.find("APG Studio parallel-empty-preamp v2.0.0") != std::string::npos);
    assert(clean.find("┌─────────────>──────────────┐") != std::string::npos || clean.find("┌──────────────>─────────────┐") != std::string::npos);
    assert(clean.find("║ Pan 2 ║                    │ Mix 2 │") != std::string::npos);
    assert(clean.find("└──>──│") != std::string::npos);
    assert(clean.find("│──>──┘") != std::string::npos);
    assert(clean.find("Plexi Tone Stage") != std::string::npos);
}

void assert_drive_chorus_preamp_alignment() {
    auto document = apg::terminal::ApgPackageDocument::load("test/fixtures/packages-v1/parallel-drive-chorus-preamp.apg");
    apg::terminal::ProjectEditor    editor(std::move(document));
    apg::terminal::FakeAudioSession audio;
    std::pair                       terminal_size{128, 36};
    auto studio = apg::terminal::studio_component(editor, audio, [] {}, [&] { return terminal_size; });
    const auto rendered = render(studio, 128, 36, terminal_size);
    const auto clean    = strip_ansi_codes(rendered.screen.ToString());
    std::cout << clean;
    assert(clean.find("APG Studio parallel-drive-chorus-preamp v2.0.0") != std::string::npos);
    assert(clean.find("┌──>──│         │──>──│         │──>──┐") != std::string::npos);
    assert(clean.find("│ IN │──>──║ Pan 2 ║") != std::string::npos);
    assert(clean.find("│ Mix 2 │") != std::string::npos);
    assert(clean.find("└────>────│") != std::string::npos);
    assert(clean.find("│────>─────┘") != std::string::npos);
    assert(clean.find("Overdrive") != std::string::npos);
    assert(clean.find("Chorus") != std::string::npos);
    assert(clean.find("Plexi Tone Stage") != std::string::npos);
}

void assert_parallel_followed_by_node_alignment() {
    auto document = apg::terminal::ApgPackageDocument::load("test/fixtures/packages-v1/simple-gain.apg");
    apg::terminal::ProjectEditor    editor(std::move(document));
    editor.add_parallel_on_route(editor.document().routes().front(), "");
    apg::terminal::FakeAudioSession audio;
    std::pair                       terminal_size{140, 36};
    auto studio = apg::terminal::studio_component(editor, audio, [] {}, [&] { return terminal_size; });
    const auto rendered = render(studio, 140, 36, terminal_size);
    const auto clean    = strip_ansi_codes(rendered.screen.ToString());

    assert(clean.find("│ IN │──>──│ Pan 2 │") != std::string::npos);
    assert(clean.find("│ Mix 2 │──>──║") != std::string::npos);
    assert(clean.find("Gain") != std::string::npos);
}

void assert_parallel_nested_chain_alignment() {
    auto document = apg::terminal::ApgPackageDocument::load("test/fixtures/packages-v1/parallel-nested-chain.apg");
    apg::terminal::ProjectEditor    editor(std::move(document));
    apg::terminal::FakeAudioSession audio;
    std::pair                       terminal_size{184, 45};
    auto studio = apg::terminal::studio_component(editor, audio, [] {}, [&] { return terminal_size; });
    const auto rendered = render(studio, 184, 45, terminal_size);
    const auto clean    = strip_ansi_codes(rendered.screen.ToString());

    assert(clean.find("APG Studio parallel-drive-chorus-preamp v2.0.0") != std::string::npos);
    assert(clean.find("│ IN │──>──") != std::string::npos);
    assert(clean.find("Pan 2") != std::string::npos);
    assert(clean.find("Overdrive") != std::string::npos);
    assert(clean.find("Chorus") != std::string::npos);
    assert(clean.find("Plexi Tone Stage") != std::string::npos);
    assert(clean.find("Mix 2") != std::string::npos);
    assert(clean.find("└──>──│ Pan 2 │") != std::string::npos);
}

std::vector<int> snapshot_numbers(const std::string &clean, const std::string &key, std::size_t count) {
    const auto pos = clean.find(key);
    assert(pos != std::string::npos);
    std::vector<int> values;
    std::size_t      index = pos + key.size();
    while (index < clean.size() && values.size() < count) {
        while (index < clean.size() &&
               !std::isdigit(static_cast<unsigned char>(clean[index])) && clean[index] != '-')
            ++index;
        if (index >= clean.size())
            break;
        const std::size_t start = index;
        while (index < clean.size() &&
               (std::isdigit(static_cast<unsigned char>(clean[index])) || clean[index] == '-'))
            ++index;
        values.push_back(static_cast<int>(std::strtol(clean.substr(start, index - start).c_str(), nullptr, 10)));
    }
    assert(values.size() == count);
    return values;
}

struct GraphSnapshot {
    std::string id;
    int         scroll_x = 0;
    int         scroll_y = 0;
    int         vx_min   = 0;
    int         vy_min   = 0;
    int         vx_max   = 0;
    int         vy_max   = 0;
    int         px_min   = 0;
    int         py_min   = 0;
    int         px_max   = 0;
    int         py_max   = 0;
    int         cx_min   = 0;
    int         cy_min   = 0;
    int         cx_max   = 0;
    int         cy_max   = 0;
    int         nx_min   = 0;
    int         ny_min   = 0;
    int         nx_max   = 0;
    int         ny_max   = 0;
};

void assert_graph_scroll_centers_selected() {
    auto document = apg::terminal::ApgPackageDocument::load("test/fixtures/packages-v1/guitar-pedalboard.apg");
    apg::terminal::ProjectEditor    editor(std::move(document));
    apg::terminal::FakeAudioSession audio;
    std::pair                       terminal_size{120, 36};
    auto studio = apg::terminal::studio_component(editor, audio, [] {}, [&] { return terminal_size; }, true);

    render(studio, 120, 36, terminal_size);

    const std::vector<std::string> order = {
        "gate1", "phaser", "drive1", "tone1", "trem1", "chorus1", "delay1", "reverb1",
    };

    auto inspect = [&]() -> GraphSnapshot {
        assert(studio->OnEvent(ftxui::Event::Special("\x04")));
        const auto debug = render(studio, 120, 36, terminal_size);
        const auto clean = strip_ansi_codes(debug.screen.ToString());

        const std::string marker = "Selected Node ID: ";
        const auto        pos    = clean.find(marker);
        assert(pos != std::string::npos);
        std::string id;
        for (std::size_t i = pos + marker.size(); i < clean.size(); ++i) {
            const char character = clean[i];
            if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
                (character >= '0' && character <= '9') || character == '_' || character == '-') {
                id += character;
            } else {
                break;
            }
        }
        assert(!id.empty());

        const auto scroll   = snapshot_numbers(clean, "Graph Scroll: ", 2);
        const auto viewport = snapshot_numbers(clean, "Graph Viewport: ", 4);
        const auto pane     = snapshot_numbers(clean, "Graph Pane Box: ", 4);
        const auto content  = snapshot_numbers(clean, "Graph Content Box: ", 4);
        const auto node     = snapshot_numbers(clean, "Selected Node Box: ", 4);

        assert(studio->OnEvent(ftxui::Event::Escape));
        return {id,         scroll[0],   scroll[1],   viewport[0], viewport[1], viewport[2], viewport[3],
                pane[0],    pane[1],     pane[2],     pane[3],     content[0],  content[1],  content[2],
                content[3], node[0],     node[1],     node[2],     node[3]};
    };

    for (const std::string &expected : order) {
        const GraphSnapshot s = inspect();
        assert(s.id == expected);

        assert(s.nx_min >= s.px_min && s.nx_max <= s.px_max);
        assert(s.ny_min >= s.py_min && s.ny_max <= s.py_max);

        const int pane_width    = s.px_max - s.px_min + 1;
        const int pane_height   = s.py_max - s.py_min + 1;
        const int content_width = s.cx_max - s.cx_min + 1;
        const int content_height = s.cy_max - s.cy_min + 1;

        const int pane_center_x = s.px_min + (s.px_max - s.px_min) / 2;
        const int node_center_x = (s.nx_min + s.nx_max) / 2;
        const int pane_center_y = s.py_min + (s.py_max - s.py_min) / 2;
        const int node_center_y = (s.ny_min + s.ny_max) / 2;

        const int dx_actual = s.px_min - s.cx_min;
        const int dy_actual = s.py_min - s.cy_min;

        if (content_width > pane_width) {
            const int  max_dx = content_width - pane_width - 1;
            const bool clamped_left  = dx_actual <= 0;
            const bool clamped_right = dx_actual >= max_dx;
            if (!clamped_left && !clamped_right)
                assert(std::abs(node_center_x - pane_center_x) <= 1);
        }
        if (content_height > pane_height) {
            const int  max_dy = content_height - pane_height - 1;
            const bool clamped_top    = dy_actual <= 0;
            const bool clamped_bottom = dy_actual >= max_dy;
            if (!clamped_top && !clamped_bottom)
                assert(std::abs(node_center_y - pane_center_y) <= 1);
        }

        studio->OnEvent(ftxui::Event::ArrowRight);
    }
}

void assert_selected_node_keeps_cyan_border() {
    auto                         document = parallel_document();
    apg::terminal::ProjectEditor editor(std::move(document));
    apg::terminal::FakeAudioSession audio;
    std::pair                       terminal_size{180, 42};
    auto studio = apg::terminal::studio_component(editor, audio, [] {}, [&] { return terminal_size; }, true);

    auto inspect = [&]() -> std::pair<std::string, ftxui::Box> {
        assert(studio->OnEvent(ftxui::Event::Special("\x04")));
        const auto debug = render(studio, terminal_size.first, terminal_size.second, terminal_size);
        const auto clean = strip_ansi_codes(debug.screen.ToString());

        const std::string marker = "Selected Node ID: ";
        const auto        pos    = clean.find(marker);
        assert(pos != std::string::npos);
        std::string id;
        for (std::size_t i = pos + marker.size(); i < clean.size(); ++i) {
            const char character = clean[i];
            if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
                (character >= '0' && character <= '9') || character == '_' || character == '-') {
                id += character;
            } else {
                break;
            }
        }
        assert(!id.empty());

        const auto node = snapshot_numbers(clean, "Selected Node Box: ", 4);
        assert(studio->OnEvent(ftxui::Event::Escape));
        return {id, {node[0], node[2], node[1], node[3]}};
    };

    auto assert_border_cyan = [&](const ftxui::Screen &screen, const ftxui::Box &b, bool expect_cyan) {
        assert(!b.IsEmpty());
        for (int x = b.x_min; x <= b.x_max; ++x) {
            assert((screen.CellAt(x, b.y_min).foreground_color == ftxui::Color::Cyan) == expect_cyan);
            assert((screen.CellAt(x, b.y_max).foreground_color == ftxui::Color::Cyan) == expect_cyan);
        }
        for (int y = b.y_min; y <= b.y_max; ++y) {
            assert((screen.CellAt(b.x_min, y).foreground_color == ftxui::Color::Cyan) == expect_cyan);
            assert((screen.CellAt(b.x_max, y).foreground_color == ftxui::Color::Cyan) == expect_cyan);
        }
    };

    render(studio, terminal_size.first, terminal_size.second, terminal_size);

    const auto [pan_id, pan_box] = inspect();
    assert(pan_id == "parallel_pan");
    assert(editor.document().find_node(pan_id)->routing_helper());
    const auto pan_screen = render(studio, terminal_size.first, terminal_size.second, terminal_size);
    assert_border_cyan(pan_screen.screen, pan_box, true);

    assert(studio->OnEvent(ftxui::Event::ArrowRight));
    const auto [boost_id, boost_box] = inspect();
    assert(boost_id == "boost");
    const auto boost_screen = render(studio, terminal_size.first, terminal_size.second, terminal_size);
    assert_border_cyan(boost_screen.screen, boost_box, true);
    assert_border_cyan(boost_screen.screen, pan_box, false);

    assert(studio->OnEvent(ftxui::Event::Character("b")));
    assert(editor.bypassed(boost_id));
    const auto [boost_id_after, boost_box_after] = inspect();
    assert(boost_id_after == boost_id);
    assert(boost_box_after == boost_box);
    const auto bypassed = render(studio, terminal_size.first, terminal_size.second, terminal_size);
    assert_border_cyan(bypassed.screen, boost_box, true);
    const int cx = (boost_box.x_min + boost_box.x_max) / 2;
    const int cy = (boost_box.y_min + boost_box.y_max) / 2;
    assert(bypassed.screen.CellAt(cx, cy).dim);
    assert(!bypassed.screen.CellAt(boost_box.x_min, cy).dim);

    assert(studio->OnEvent(ftxui::Event::Character("b")));
    assert(!editor.bypassed(boost_id));
    const auto reenabled = render(studio, terminal_size.first, terminal_size.second, terminal_size);
    assert_border_cyan(reenabled.screen, boost_box, true);
    assert(!reenabled.screen.CellAt(cx, cy).dim);
}

} // namespace

int main() {
    assert_screen_dump();
    assert_parallel_alignment();
    assert_both_channels_empty_alignment();
    assert_one_channel_empty_alignment();
    assert_drive_chorus_preamp_alignment();
    assert_parallel_followed_by_node_alignment();
    assert_parallel_nested_chain_alignment();
    assert_wide_unit_drag();
    assert_compact_unit_pickup();
    assert_debug_snapshot();
    assert_graph_wheel_cycles_route_order();
    assert_graph_scroll_centers_selected();
    assert_selected_node_keeps_cyan_border();

    auto document = apg::terminal::ApgPackageDocument::load("test/fixtures/packages-v1/simple-gain.apg");
    apg::terminal::ProjectEditor    editor(std::move(document));
    apg::terminal::FakeAudioSession audio;
    std::pair                       terminal_size{80, 24};
    bool                            exit_requested = false;
    auto                            studio =
        apg::terminal::studio_component(editor, audio, [&] { exit_requested = true; }, [&] { return terminal_size; });

    const auto initial_nodes_main = editor.document().nodes().size();
    const auto compact            = render(studio, 80, 24, terminal_size);
    assert(compact.text.find("APG Studio") != std::string::npos || compact.text.find("APG TUI") != std::string::npos);
    assert(compact.text.find("Graph") != std::string::npos);
    assert(compact.text.find("Inspector") != std::string::npos);
    assert(
        compact.text.find("Ctrl+S save") != std::string::npos || compact.text.find("Saved") != std::string::npos ||
        compact.text.find("help") != std::string::npos
    );
    assert_compact_graph_shape(compact);

    const auto inspector_tab = locate_ascii(compact.screen, "Inspector");
    assert(inspector_tab);
    assert(studio->OnEvent(left_mouse(inspector_tab->first + 1, inspector_tab->second, ftxui::Mouse::Pressed)));
    auto inspector = render(studio, 80, 24, terminal_size);
    assert(inspector.text.find("2.00 x") != std::string::npos);

    const auto value = locate_ascii(inspector.screen, "2.00 x", true);
    assert(value);
    assert(studio->OnEvent(left_mouse(value->first, value->second, ftxui::Mouse::Pressed)));
    assert(studio->OnEvent(left_mouse(72, value->second, ftxui::Mouse::Released)));
    assert(gain(editor) > 3.0);
    assert(editor.can_undo());
    assert(studio->OnEvent(ftxui::Event::CtrlZ));
    assert(std::abs(gain(editor) - 2.0) < 1e-9);

    assert(studio->OnEvent(ftxui::Event::TabReverse));
    assert(studio->OnEvent(ftxui::Event::TabReverse));
    assert(studio->OnEvent(ftxui::Event::Return));
    assert(editor.document().nodes().size() == initial_nodes_main + 1);
    assert(editor.document().find_node("simple_gain"));
    assert(studio->OnEvent(ftxui::Event::Tab));
    assert(studio->OnEvent(ftxui::Event::Character("r")));
    assert(studio->OnEvent(ftxui::Event::Character("r")));
    assert(studio->OnEvent(ftxui::Event::Character("x")));
    assert(std::find_if(editor.document().routes().begin(), editor.document().routes().end(), [](const auto &r) {
               return r.from == "gain1.output";
           }) != editor.document().routes().end());
    assert(studio->OnEvent(ftxui::Event::CtrlZ));
    assert(studio->OnEvent(ftxui::Event::CtrlZ));
    assert(editor.document().nodes().size() == initial_nodes_main);
    assert(studio->OnEvent(ftxui::Event::Tab));

    assert(studio->OnEvent(ftxui::Event::Tab));
    auto scenes = render(studio, 80, 24, terminal_size);
    assert(scenes.text.find("Unity") != std::string::npos);
    assert(scenes.text.find("Boost") != std::string::npos);
    assert(studio->OnEvent(ftxui::Event::Return));
    assert(editor.active_scene() && *editor.active_scene() == "Unity");
    assert(std::abs(gain(editor) - 1.0) < 1e-9);

    assert(studio->OnEvent(ftxui::Event::Character("n")));
    assert(studio->OnEvent(ftxui::Event::Character("é")));
    assert(studio->OnEvent(ftxui::Event::Backspace));
    const auto empty_scene_name = render(studio, 80, 24, terminal_size);
    assert(empty_scene_name.text.find("\xEF\xBF\xBD") == std::string::npos);
    assert(studio->OnEvent(ftxui::Event::Character("T")));
    assert(studio->OnEvent(ftxui::Event::Return));
    assert(editor.document().find_scene("Scene 3T") || editor.document().find_scene("T"));

    assert(studio->OnEvent(ftxui::Event::Character("?")));
    const auto help       = render(studio, 80, 24, terminal_size);
    const auto clean_help = strip_ansi_codes(help.text);
    assert(clean_help.find("Help") != std::string::npos);
    assert(clean_help.find("u:Undo action") != std::string::npos);
    assert(clean_help.find("Ctrl+R:Redo action") != std::string::npos);
    assert(studio->OnEvent(ftxui::Event::Escape));

    assert(studio->OnEvent(ftxui::Event::TabReverse));
    assert(studio->OnEvent(ftxui::Event::TabReverse));
    assert(studio->OnEvent(ftxui::Event::Character(" ")));
    assert(audio.running());
    assert(audio.muted());
    assert(studio->OnEvent(ftxui::Event::Character("m")));
    assert(!audio.muted());
    assert(studio->OnEvent(ftxui::Event::Character(" ")));
    assert(!audio.running());

    const auto wide = render(studio, 120, 32, terminal_size);
    assert(wide.text.find("Units") != std::string::npos);
    assert(wide.text.find("Route graph") != std::string::npos);
    assert(wide.text.find("Inspector") != std::string::npos);
    assert(wide.text.find("Scenes") != std::string::npos);

    const auto large = render(studio, 180, 42, terminal_size);
    assert(large.text.find("Simple Gain") != std::string::npos);
    assert(large.text.find("APGCore") == std::string::npos);

    const auto undersized = render(studio, 79, 23, terminal_size);
    assert(undersized.text.find("Terminal too small") != std::string::npos);
    assert(undersized.text.find("80×24") != std::string::npos);

    assert(studio->OnEvent(ftxui::Event::Character("q")));
    const auto quit = render(studio, 80, 24, terminal_size);
    assert(quit.text.find("Unsaved changes") != std::string::npos);
    assert(!exit_requested);
    assert(studio->OnEvent(ftxui::Event::Character("c")));
    assert(!exit_requested);
    studio.reset();
    editor.set_param("gain1", "gain", 1.5);
    assert(std::abs(gain(editor) - 1.5) < 1e-9);
    return 0;
}
