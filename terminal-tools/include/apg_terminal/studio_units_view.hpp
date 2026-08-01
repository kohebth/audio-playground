#ifndef APG_TERMINAL_STUDIO_UNITS_VIEW_HPP
#define APG_TERMINAL_STUDIO_UNITS_VIEW_HPP

#include "apg_terminal/editor.hpp"
#include "apg_terminal/studio_types.hpp"

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace apg::terminal {

ftxui::Element render_units_view(
    const std::vector<const UnitReference *> &units,
    bool                                      searching,
    const std::string                        &unit_search,
    bool                                      wide_layout,
    std::size_t                               selected_unit,
    Pane                                      active_pane,
    const std::optional<std::string>         &dragged_unit,
    const std::optional<std::string>         &carried_unit,
    std::deque<UnitHit>                      &unit_hits
);

bool handle_units_event(
    const ftxui::Event                        &event,
    const std::vector<const UnitReference *>  &units,
    bool                                      &searching,
    std::size_t                               &selected_unit,
    const std::optional<Route>                &selected_route,
    std::string                               &transient_status,
    ProjectEditor                             &editor,
    std::string                               &selected_node,
    std::function<void(std::function<void()>)> act_fn
);

} // namespace apg::terminal

#endif // APG_TERMINAL_STUDIO_UNITS_VIEW_HPP
