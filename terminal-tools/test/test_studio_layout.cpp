#include "apg_terminal/editor.hpp"
#include "apg_terminal/project_document.hpp"
#include "apg_terminal/session.hpp"
#include "apg_terminal/studio.hpp"

#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
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
            if (character == "◇" || character == "◆")
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

int assert_five_row_card(const ftxui::Screen &screen, const std::string &title, const std::string &id) {
    const auto title_position = locate_ascii(screen, title, true);
    const auto id_position    = locate_ascii(screen, id, true);
    assert(title_position);
    assert(id_position);
    assert(id_position->first == title_position->first);
    assert(id_position->second == title_position->second + 2);
    assert(title_position->first > 0);
    assert(screen.CellAt(title_position->first - 1, title_position->second - 1).character == "╭");
    assert(screen.CellAt(id_position->first - 1, id_position->second + 1).character == "╰");
    return title_position->second + 1;
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
    const auto pan          = locate_ascii(rendered.screen, "parallel_pan");
    const auto mix          = locate_ascii(rendered.screen, "parallel_mix");
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
    const auto route   = locate_character(initial.screen, "◇");
    assert(unit);
    assert(route);

    assert(studio->OnEvent(left_mouse(unit->first, unit->second, ftxui::Mouse::Pressed)));
    assert(studio->OnEvent(left_mouse(route->first, route->second, ftxui::Mouse::Moved)));
    const auto hovered = render(studio, 120, 32, terminal_size);
    assert(hovered.screen.CellAt(route->first, route->second).character == "◆");
    assert(studio->OnEvent(left_mouse(route->first, route->second, ftxui::Mouse::Released)));
    assert(editor.document().nodes().size() == 2);
    assert(editor.can_undo());
    assert(editor.undo());
    assert(editor.document().nodes().size() == 1);
}

void assert_compact_unit_pickup() {
    auto document = apg::terminal::ApgPackageDocument::load("test/fixtures/packages-v1/simple-gain.apg");
    apg::terminal::ProjectEditor    editor(std::move(document));
    apg::terminal::FakeAudioSession audio;
    std::pair                       terminal_size{80, 24};
    bool                            exit_requested = false;
    auto                            studio =
        apg::terminal::studio_component(editor, audio, [&] { exit_requested = true; }, [&] { return terminal_size; });

    assert(studio->OnEvent(ftxui::Event::TabReverse));
    const auto units = render(studio, 80, 24, terminal_size);
    const auto unit  = locate_ascii(units.screen, "gain_unit");
    assert(unit);
    assert(studio->OnEvent(left_mouse(unit->first, unit->second, ftxui::Mouse::Pressed)));
    assert(studio->OnEvent(left_mouse(unit->first, unit->second, ftxui::Mouse::Released)));

    auto graph = render(studio, 80, 24, terminal_size);
    assert(graph.text.find("Placing Simple Gain") != std::string::npos);
    const auto route = locate_character(graph.screen, "◇");
    assert(route);
    assert(studio->OnEvent(left_mouse(route->first, route->second + 1, ftxui::Mouse::Pressed)));
    assert(studio->OnEvent(left_mouse(route->first, route->second + 1, ftxui::Mouse::Released)));
    assert(editor.document().nodes().size() == 1);
    graph = render(studio, 80, 24, terminal_size);
    assert(graph.text.find("Placing Simple Gain") != std::string::npos);
    assert(studio->OnEvent(left_mouse(route->first, route->second - 1, ftxui::Mouse::Pressed)));
    assert(studio->OnEvent(left_mouse(route->first, route->second - 1, ftxui::Mouse::Released)));
    assert(editor.document().nodes().size() == 1);
    graph = render(studio, 80, 24, terminal_size);
    assert(graph.text.find("Placing Simple Gain") != std::string::npos);
    assert(studio->OnEvent(left_mouse(route->first + 3, route->second, ftxui::Mouse::Pressed)));
    assert(studio->OnEvent(left_mouse(route->first + 3, route->second, ftxui::Mouse::Released)));
    assert(editor.document().nodes().size() == 1);
    graph = render(studio, 80, 24, terminal_size);
    assert(graph.text.find("Placing Simple Gain") != std::string::npos);

    assert(studio->OnEvent(left_mouse(route->first, route->second, ftxui::Mouse::Pressed)));
    assert(studio->OnEvent(left_mouse(route->first, route->second, ftxui::Mouse::Released)));
    assert(editor.document().nodes().size() == 2);
    assert(editor.undo());
    assert(editor.document().nodes().size() == 1);

    assert(studio->OnEvent(ftxui::Event::TabReverse));
    const auto units_again = render(studio, 80, 24, terminal_size);
    const auto unit_again  = locate_ascii(units_again.screen, "gain_unit");
    assert(unit_again);
    assert(studio->OnEvent(left_mouse(unit_again->first, unit_again->second, ftxui::Mouse::Pressed)));
    assert(studio->OnEvent(left_mouse(unit_again->first, unit_again->second, ftxui::Mouse::Released)));
    assert(studio->OnEvent(ftxui::Event::Escape));
    assert(!exit_requested);
    const auto cancelled = render(studio, 80, 24, terminal_size);
    assert(cancelled.text.find("Placement cancelled") != std::string::npos);
}

} // namespace

int main() {
    assert_parallel_alignment();
    assert_wide_unit_drag();
    assert_compact_unit_pickup();

    auto document = apg::terminal::ApgPackageDocument::load("test/fixtures/packages-v1/simple-gain.apg");
    apg::terminal::ProjectEditor    editor(std::move(document));
    apg::terminal::FakeAudioSession audio;
    std::pair                       terminal_size{80, 24};
    bool                            exit_requested = false;
    auto                            studio =
        apg::terminal::studio_component(editor, audio, [&] { exit_requested = true; }, [&] { return terminal_size; });

    const auto compact = render(studio, 80, 24, terminal_size);
    assert(compact.text.find("APG TUI") != std::string::npos);
    assert(compact.text.find("Graph") != std::string::npos);
    assert(compact.text.find("Inspector") != std::string::npos);
    assert(compact.text.find("Ctrl+S save") != std::string::npos);
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
    assert(editor.document().nodes().size() == 2);
    assert(editor.document().find_node("simple_gain"));
    assert(studio->OnEvent(ftxui::Event::Tab));
    assert(studio->OnEvent(ftxui::Event::Character("r")));
    assert(studio->OnEvent(ftxui::Event::Character("r")));
    assert(studio->OnEvent(ftxui::Event::Character("x")));
    assert(
        std::find(
            editor.document().routes().begin(), editor.document().routes().end(),
            apg::terminal::Route{"gain1.output", "simple_gain.input"}
        ) != editor.document().routes().end()
    );
    assert(studio->OnEvent(ftxui::Event::CtrlZ));
    assert(studio->OnEvent(ftxui::Event::CtrlZ));
    assert(editor.document().nodes().size() == 1);
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
    assert(editor.document().find_scene("T"));

    assert(studio->OnEvent(ftxui::Event::Character("?")));
    const auto help = render(studio, 80, 24, terminal_size);
    assert(help.text.find("Help") != std::string::npos);
    assert(help.text.find("Ctrl+Z/Y history") != std::string::npos);
    assert(studio->OnEvent(ftxui::Event::Escape));

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
