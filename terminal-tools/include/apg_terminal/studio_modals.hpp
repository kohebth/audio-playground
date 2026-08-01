#ifndef APG_TERMINAL_STUDIO_MODALS_HPP
#define APG_TERMINAL_STUDIO_MODALS_HPP

#include "apg_terminal/editor.hpp"
#include "apg_terminal/studio_types.hpp"

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <functional>
#include <string>

namespace apg::terminal {

ftxui::Element render_modal_dialog(
    Modal              modal,
    const std::string &modal_text,
    const std::string &scene_name,
    const std::string &node_id,
    ftxui::Element     page
);

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
);

} // namespace apg::terminal

#endif // APG_TERMINAL_STUDIO_MODALS_HPP
