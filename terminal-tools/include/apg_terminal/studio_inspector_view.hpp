#ifndef APG_TERMINAL_STUDIO_INSPECTOR_VIEW_HPP
#define APG_TERMINAL_STUDIO_INSPECTOR_VIEW_HPP

#include "apg_terminal/editor.hpp"
#include "apg_terminal/studio_types.hpp"

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <deque>
#include <functional>
#include <string>

namespace apg::terminal {

ftxui::Element render_inspector_view(
    const ProjectEditor      &editor,
    const std::string        &selected_node,
    std::size_t               selected_parameter,
    Pane                      active_pane,
    std::deque<ParameterHit> &parameter_hits
);

bool handle_inspector_event(
    const ftxui::Event                        &event,
    ProjectEditor                             &editor,
    const std::string                         &selected_node,
    std::size_t                               &selected_parameter,
    Modal                                     &modal,
    std::function<void(std::function<void()>)> act_fn
);

} // namespace apg::terminal

#endif // APG_TERMINAL_STUDIO_INSPECTOR_VIEW_HPP
