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
    case Modal::Help: {
        title   = "Help";
        auto col1 = vbox({
            hbox({ text("Tab/S-Tab") | bold | color(Color::Cyan), text(":Switch pane") }),
            hbox({ text("h/j/k/l") | bold | color(Color::Cyan), text(":Navigate pane") }),
            hbox({ text("u") | bold | color(Color::Cyan), text(":Undo action") }),
            hbox({ text("Ctrl+R") | bold | color(Color::Cyan), text(":Redo action") }),
            hbox({ text("r") | bold | color(Color::Cyan), text(":Cycle routes") }),
            hbox({ text("x") | bold | color(Color::Cyan), text(":Move effect") }),
            hbox({ text("b") | bold | color(Color::Cyan), text(":Bypass effect") }),
            hbox({ text("d") | bold | color(Color::Cyan), text(":Remove item") }),
            hbox({ text("p") | bold | color(Color::Cyan), text(":Add parallel branch") }),
        });
        auto col2 = vbox({
            hbox({ text("Space") | bold | color(Color::Cyan), text(":Audio transport") }),
            hbox({ text("m") | bold | color(Color::Cyan), text(":Mute audio") }),
            hbox({ text("Ctrl+S") | bold | color(Color::Cyan), text(":Save project") }),
            hbox({ text("q") | bold | color(Color::Cyan), text(":Guarded quit") }),
            hbox({ text("n") | bold | color(Color::Cyan), text(":New scene") }),
            hbox({ text("e") | bold | color(Color::Cyan), text(":Rename scene") }),
            hbox({ text("c") | bold | color(Color::Cyan), text(":Collapse parallel") }),
            hbox({ text("Ctrl+D") | bold | color(Color::Cyan), text(":Debug snapshot") }),
        });
        content = vbox({
            hbox({ col1 | flex, text("   "), col2 | flex }),
            separator(),
            text("Press Escape, Enter, or ? to close.") | dim,
        });
        break;
    }
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
    case Modal::Debug:
        title   = "Debug Snapshot";
        content = vbox({
            text("Copied to OS Clipboard & written to apg-tui-debug.txt") | bold | color(Color::Green),
            separator(),
            paragraph(modal_text) | size(HEIGHT, LESS_THAN, 18),
            separator(),
            text("Press Escape, Enter, or Ctrl+D to close.") | dim,
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
    if (modal == Modal::Debug) {
        if (event == Event::Escape || event == Event::Return || event == Event::Character("d") ||
            event == Event::Special("\x04")) {
            modal = Modal::None;
            modal_text.clear();
        }
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
