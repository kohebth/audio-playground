#include "apg_terminal/editor.hpp"
#include "apg_terminal/project_document.hpp"
#include "apg_terminal/session.hpp"
#include "apg_terminal/studio.hpp"

#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
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

void assert_compact_graph_shape(const Rendered &rendered) {
    const auto title = locate_ascii(rendered.screen, "Simple Gain");
    const auto id    = locate_ascii(rendered.screen, "gain1");
    assert(title);
    assert(id);
    assert(id->second == title->second + 1);
    bool closes_on_next_row = false;
    for (int x = 0; x < rendered.screen.dimx(); ++x) {
        if (rendered.screen.CellAt(x, id->second + 1).character == "╰") {
            closes_on_next_row = true;
            break;
        }
    }
    assert(closes_on_next_row);
}

} // namespace

int main() {
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
