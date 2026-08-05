#ifndef APG_TERMINAL_STUDIO_SCENES_VIEW_HPP
#define APG_TERMINAL_STUDIO_SCENES_VIEW_HPP

#include "apg_terminal/application/editor.hpp"
#include "apg_terminal/ui/studio_types.hpp"

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <deque>
#include <functional>
#include <string>

namespace apg::terminal {

ftxui::Element render_scenes_view(
    const ProjectEditor &editor, std::size_t selected_scene, Pane active_pane, std::deque<SceneHit> &scene_hits
);

bool handle_scenes_event(
    const ftxui::Event                        &event,
    ProjectEditor                             &editor,
    std::size_t                               &selected_scene,
    Modal                                     &modal,
    std::string                               &modal_text,
    std::string                               &transient_status,
    std::function<void(std::function<void()>)> act_fn
);

} // namespace apg::terminal

#endif // APG_TERMINAL_STUDIO_SCENES_VIEW_HPP
