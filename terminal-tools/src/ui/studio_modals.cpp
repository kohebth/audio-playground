#include "apg_terminal/ui/studio_modals.hpp"

namespace apg::terminal {

ftxui::Element render_modal_dialog(
    Modal              modal,
    const std::string &modal_text,
    const std::string &scene_name,
    const std::string &node_id,
    ftxui::Element     page
) {
    using namespace ftxui;
    if (modal == Modal::None)
        return page;
    Element     content;
    std::string title;
    switch (modal) {
    case Modal::Help:
        title   = "Help";
        content = vbox({
            text("Tab/Shift-Tab switch panes; arrows navigate the active pane."),
            text("Graph: r cycles routes, x moves the effect, c collapses an empty parallel section."),
            text("Units: drag onto a signal line; compact clicks carry a unit to Graph. Enter inserts."),
            text("Inspector: arrows adjust, Page keys coarse, Home/End bounds, b bypass, d remove."),
            text("Scenes: Enter recall, n create, u update, e rename, d delete."),
            text("Audio: Space transport, m mute. Ctrl+S save, Ctrl+Z/Y history, q guarded quit."),
            separator(),
            text("Press Escape or Enter to close.") | dim,
        });
        break;
    case Modal::Quit:
        title   = "Unsaved changes";
        content = vbox({
            text("Save before leaving?"),
            text("[s] Save and quit   [d] Discard   [c/Esc] Cancel") | bold,
        });
        break;
    case Modal::NewScene:
        title   = "Capture scene";
        content = vbox({
            text("Name: " + modal_text + "▌"),
            text("Enter saves · Escape cancels") | dim,
        });
        break;
    case Modal::RenameScene:
        title   = "Rename scene";
        content = vbox({
            text("Name: " + modal_text + "▌"),
            text("Enter renames · Escape cancels") | dim,
        });
        break;
    case Modal::DeleteScene:
        title   = "Delete scene";
        content = vbox({
            text("Delete \"" + scene_name + "\"?"),
            text("[y] Delete   [n/Esc] Cancel") | bold,
        });
        break;
    case Modal::DeleteNode:
        title   = "Remove effect";
        content = vbox({
            text("Remove \"" + node_id + "\" and bridge its route?"),
            text("[y] Remove   [n/Esc] Cancel") | bold,
        });
        break;
    case Modal::None:
        break;
    }
    auto dialog =
        window(text(" " + title + " ") | bold, std::move(content)) | size(WIDTH, LESS_THAN, 72) | clear_under | center;
    return dbox({std::move(page), dialog});
}

bool handle_modal_event(
    const ftxui::Event                        &event,
    Modal                                     &modal,
    std::string                               &modal_text,
    ProjectEditor                             &editor,
    const std::string                         &selected_scene_name,
    const std::string                         &selected_node,
    std::function<bool()>                      save_fn,
    std::function<void()>                      request_exit_fn,
    std::function<void(std::function<void()>)> act_fn
) {
    using namespace ftxui;
    if (modal == Modal::Help) {
        if (event == Event::Escape || event == Event::Return || event == Event::Character("?"))
            modal = Modal::None;
        return true;
    }
    if (modal == Modal::Quit) {
        if (event == Event::Character("s")) {
            if (save_fn()) {
                modal = Modal::None;
                request_exit_fn();
            }
        } else if (event == Event::Character("d")) {
            modal = Modal::None;
            request_exit_fn();
        } else if (event == Event::Character("c") || event == Event::Escape) {
            modal = Modal::None;
        }
        return true;
    }
    if (modal == Modal::DeleteScene || modal == Modal::DeleteNode) {
        if (event == Event::Character("y")) {
            if (modal == Modal::DeleteScene) {
                const auto name = selected_scene_name;
                act_fn([&] { editor.remove_scene(name); });
            } else {
                const auto id = selected_node;
                act_fn([&] { editor.remove_node(id); });
            }
            modal = Modal::None;
        } else if (event == Event::Character("n") || event == Event::Escape) {
            modal = Modal::None;
        }
        return true;
    }
    if (event == Event::Escape) {
        modal = Modal::None;
        modal_text.clear();
        return true;
    }
    if (event == Event::Backspace) {
        pop_utf8(modal_text);
        return true;
    }
    if (event == Event::Return) {
        const auto value = modal_text;
        if (modal == Modal::NewScene)
            act_fn([&] { editor.save_scene(value); });
        else if (modal == Modal::RenameScene) {
            const auto current = selected_scene_name;
            act_fn([&] { editor.rename_scene(current, value); });
        }
        modal = Modal::None;
        modal_text.clear();
        return true;
    }
    if (event.is_character() && event.character().size() <= 4 && modal_text.size() < 64) {
        modal_text += event.character();
        return true;
    }
    return true;
}

} // namespace apg::terminal
