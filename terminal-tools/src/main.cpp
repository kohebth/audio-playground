#include "apg_terminal/pipeline_component.hpp"
#include "apg_terminal/project_document.hpp"
#include "apg_terminal/session.hpp"
#include "apg_terminal/ui.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace ftxui;
using apg::terminal::NullAudioSession;
using apg::terminal::ProjectDocument;

namespace {

int self_test() {
    NullAudioSession audio;
    if (!audio.start() || !audio.running())
        return 1;
    audio.set_mute(true);
    audio.stop();
    return audio.running() ? 1 : 0;
}

} // namespace

int main(int argc, char **argv) {
    if (argc > 1 && std::string(argv[1]) == "--version") {
        std::cout << "apg-tui 0.1.0\n";
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "--self-test")
        return self_test();

    const std::filesystem::path project_path =
        argc > 1 ? argv[1] : "test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml";
    ProjectDocument document;
    try {
        document = ProjectDocument::load(project_path);
    } catch (const std::exception &error) {
        std::cerr << "apg-tui: " << error.what() << '\n';
        return 1;
    }

    auto                         audio    = std::make_shared<NullAudioSession>();
    int                          selected = 0;
    std::string                  status   = "Loaded " + project_path.string();
    std::vector<ProjectDocument> undo;
    std::vector<ProjectDocument> redo;
    apg::terminal::DragState     drag_state;
    auto                         normalize_selection = [&] {
        if (document.nodes().empty())
            selected = 0;
        else
            selected = std::clamp(selected, 0, static_cast<int>(document.nodes().size()) - 1);
    };
    auto select_node = [&](const std::string &id) {
        for (std::size_t index = 0; index < document.nodes().size(); ++index) {
            if (document.nodes()[index].id == id) {
                selected = static_cast<int>(index);
                return;
            }
        }
        normalize_selection();
    };
    auto pipeline = apg::terminal::draggable_pipeline(
        [&] {
            std::vector<apg::terminal::PipelineItem> items;
            items.reserve(document.nodes().size());
            for (const auto &node : document.nodes())
                items.push_back({node.id, node.unit});
            return items;
        },
        &selected, [&](std::size_t index) { selected = static_cast<int>(index); },
        [&](const std::string &dragged, const std::string &target, apg::terminal::DropPosition position) {
            const auto before = document;
            const bool moved  = position == apg::terminal::DropPosition::Before ? document.move_before(dragged, target)
                                                                                : document.move_after(dragged, target);
            if (moved) {
                undo.push_back(before);
                redo.clear();
                select_node(dragged);
                status = "Moved " + dragged +
                         (position == apg::terminal::DropPosition::Before ? " before " : " after ") + target;
            } else {
                status = "Unable to move effect to that route";
            }
        },
        [&](const std::string &unit, const std::string &target, apg::terminal::DropPosition position) {
            const auto                before = document;
            const auto                id     = unit + "_" + std::to_string(document.nodes().size() + 1);
            const apg::terminal::Node node{id, unit, {}, {}, {}};
            const bool added = position == apg::terminal::DropPosition::Before ? document.add_before(target, node)
                                                                               : document.add_after(target, node);
            if (added) {
                undo.push_back(before);
                redo.clear();
                select_node(id);
                status = "Added " + unit + (position == apg::terminal::DropPosition::Before ? " before " : " after ") +
                         target;
            } else {
                status = "Unable to drop " + unit + " at that route";
            }
        },
        &drag_state
    );
    auto parameters = apg::terminal::parameter_panel(
        [&] {
            std::vector<apg::terminal::ParameterItem> items;
            if (document.nodes().empty())
                return items;
            const auto &node = document.nodes()[static_cast<std::size_t>(selected)];
            for (const auto &parameter : node.parameter_specs)
                items.push_back({parameter.label, parameter.unit, parameter.value, parameter.min, parameter.max});
            return items;
        },
        [&](std::size_t index, double delta) {
            if (document.nodes().empty())
                return;
            auto &node = document.nodes()[static_cast<std::size_t>(selected)];
            if (index >= node.parameter_specs.size())
                return;
            const auto  before    = document;
            const auto &parameter = node.parameter_specs[index];
            if (document.set_param(node.id, parameter.name, parameter.value + delta)) {
                undo.push_back(before);
                redo.clear();
                status = "Adjusted " + node.id + "." + parameter.name;
            }
        }
    );
    auto tray = apg::terminal::unit_tray(
        [&] {
            std::vector<apg::terminal::UnitTrayItem> items;
            for (const auto &unit : document.units())
                items.push_back({unit.id});
            return items;
        },
        [&](const std::string &unit_id) {
            if (document.nodes().empty())
                return;
            const auto before  = document;
            const auto after   = document.nodes()[static_cast<std::size_t>(selected)].id;
            const auto node_id = unit_id + "_" + std::to_string(document.nodes().size() + 1);
            if (document.add_after(after, apg::terminal::Node{node_id, unit_id, {}, {}, {}})) {
                undo.push_back(before);
                redo.clear();
                select_node(node_id);
                status = "Added " + unit_id;
            } else {
                status = "Unable to add " + unit_id + " at this route";
            }
        },
        &drag_state
    );

    auto add_button          = Button(" + ", [&] {
        if (document.units().empty()) {
            status = "No unit references available";
            return;
        }
        apg::terminal::Node node{
            "effect" + std::to_string(document.nodes().size() + 1), document.units().front().id, {}, {}, {}
        };
        const std::string after =
            document.nodes().empty() ? "" : document.nodes()[static_cast<std::size_t>(selected)].id;
        const auto before = document;
        status = document.add_after(after, std::move(node)) ? "Effect added" : "Unable to add effect at this route";
        if (status == "Effect added") {
            undo.push_back(before);
            redo.clear();
        }
        normalize_selection();
    });
    auto remove_button       = Button(" − ", [&] {
        if (document.nodes().empty())
            return;
        const auto id     = document.nodes()[static_cast<std::size_t>(selected)].id;
        const auto before = document;
        status            = document.remove(id) ? "Effect removed" : "Unable to remove effect";
        if (status == "Effect removed") {
            undo.push_back(before);
            redo.clear();
        }
        normalize_selection();
    });
    auto branch_button       = Button(" ∥ ", [&] {
        if (document.nodes().empty())
            return;
        const auto before = document;
        const auto id     = document.nodes()[static_cast<std::size_t>(selected)].id;
        if (document.add_parallel_branch(id)) {
            undo.push_back(before);
            redo.clear();
            status = "Added parallel branch around " + id;
        } else {
            status = "Parallel branch requires a linear project and Pan 2/Mix 2 definitions";
        }
    });
    auto move_up_button      = Button(" ↑ ", [&] {
        if (selected <= 0 || document.nodes().empty())
            return;
        const auto before = document;
        const auto id     = document.nodes()[static_cast<std::size_t>(selected)].id;
        const auto target = document.nodes()[static_cast<std::size_t>(selected - 1)].id;
        if (document.move_before(id, target)) {
            undo.push_back(before);
            redo.clear();
            status = "Effect moved";
        } else
            status = "Unable to move effect";
        select_node(id);
    });
    auto adjust_param_button = Button(" \u2699 ", [&] {
        if (document.nodes().empty())
            return;
        auto &node = document.nodes()[static_cast<std::size_t>(selected)];
        if (node.params.empty()) {
            status = "Selected effect has no parameters";
            return;
        }
        const auto before    = document;
        auto       parameter = node.params.begin();
        parameter->second += 0.05;
        undo.push_back(before);
        redo.clear();
        status = "Adjusted " + node.id + "." + parameter->first;
    });
    auto undo_button         = Button(" \u21a9 ", [&] {
        if (undo.empty()) {
            status = "Nothing to undo";
            return;
        }
        redo.push_back(document);
        document = undo.back();
        undo.pop_back();
        status = "Undid last edit";
        normalize_selection();
    });
    auto redo_button         = Button(" \u21aa ", [&] {
        if (redo.empty()) {
            status = "Nothing to redo";
            return;
        }
        undo.push_back(document);
        document = redo.back();
        redo.pop_back();
        status = "Redid last edit";
        normalize_selection();
    });
    auto save_button         = Button(" \u2b07 ", [&] {
        try {
            document.save_atomic(project_path);
            status = "Saved " + project_path.string();
        } catch (const std::exception &error) { status = error.what(); }
    });
    auto transport_button    = Button(" \u25b6 ", [&] {
        if (audio->running()) {
            audio->stop();
            status = "Audio stopped";
        } else {
            audio->start();
            status = "Audio started (null backend)";
        }
    });

    auto controls = Container::Horizontal({
        Container::Vertical({add_button, undo_button, save_button}),
        Container::Vertical({remove_button, redo_button, transport_button}),
        Container::Vertical({branch_button, move_up_button, adjust_param_button}),
    });
    auto select_next_node = [&] {
        if (document.nodes().empty()) {
            selected = 0;
            status   = "No nodes to select";
            return;
        }
        const auto count = static_cast<int>(document.nodes().size());
        selected        = (selected + 1 + count) % count;
        status          = "Selected " + document.nodes()[static_cast<std::size_t>(selected)].id;
    };
    auto select_previous_node = [&] {
        if (document.nodes().empty()) {
            selected = 0;
            status   = "No nodes to select";
            return;
        }
        const auto count = static_cast<int>(document.nodes().size());
        selected        = (selected - 1 + count) % count;
        status          = "Selected " + document.nodes()[static_cast<std::size_t>(selected)].id;
    };
    auto root = Renderer(Container::Vertical({pipeline, parameters, tray, controls}), [&] {
        const auto errors = document.validate();
        return apg::terminal::render_pipeline_ui(
            pipeline, controls, parameters, tray,
            document.nodes().empty() ? "none" : document.nodes()[static_cast<std::size_t>(selected)].id,
            audio->diagnostic(), status, errors.empty() ? "OK" : errors.front()
        );
    });

    auto screen = ScreenInteractive::Fullscreen();
    root |= CatchEvent([&](const Event &event) {
        if (event == Event::Tab) {
            select_next_node();
            return true;
        }
        if (event == Event::TabReverse) {
            select_previous_node();
            return true;
        }
        if (event == Event::Return) {
            if (audio->running()) {
                audio->stop();
                status = "Audio stopped";
            } else {
                audio->start();
                status = "Audio started (null backend)";
            }
            return true;
        }
        if (event == Event::Character("q") || event == Event::Escape) {
            screen.Exit();
            return true;
        }
        return false;
    });
    screen.Loop(root);
    audio->stop();
    return 0;
}
