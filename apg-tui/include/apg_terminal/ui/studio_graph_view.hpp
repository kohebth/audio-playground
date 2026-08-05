#ifndef APG_TERMINAL_STUDIO_GRAPH_VIEW_HPP
#define APG_TERMINAL_STUDIO_GRAPH_VIEW_HPP

#include "apg_terminal/application/editor.hpp"
#include "apg_terminal/ui/studio_types.hpp"

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <deque>
#include <functional>
#include <optional>
#include <string>

namespace apg::terminal {

struct GraphRenderOptions {
    Pane                 active_pane;
    std::string          selected_node;
    std::optional<Route> selected_route;
    std::optional<Route> hovered_route;
    bool                 route_drop_active;
    ScrollState          scroll_x;
    ScrollState          scroll_y;
    bool                 wide_layout;
    int                  width;
    int                  height;
};

ftxui::Element render_graph_view(
    ProjectEditor            &editor,
    const GraphRenderOptions &options,
    std::deque<RouteHit>     &route_hits,
    std::deque<NodeHit>      &node_hits,
    std::string              &render_error,
    ftxui::Box               &content_box
);

bool handle_graph_event(
    const ftxui::Event                        &event,
    ProjectEditor                             &editor,
    std::string                               &selected_node,
    std::optional<Route>                      &selected_route,
    Pane                                      &active_pane,
    std::string                               &transient_status,
    std::function<void(std::function<void()>)> act_fn,
    std::function<void(int)>                   cycle_node_fn,
    std::function<void(int)>                   cycle_route_fn
);

} // namespace apg::terminal

#endif // APG_TERMINAL_STUDIO_GRAPH_VIEW_HPP
