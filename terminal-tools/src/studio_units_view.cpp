#include "apg_terminal/studio_units_view.hpp"

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
) {
    using namespace ftxui;
    Elements rows;
    rows.push_back(
        text(
            searching     ? "Search: " + unit_search + "▌"
            : wide_layout ? "/ search · drag to signal"
                          : "/ search · click to place"
        ) |
        dim
    );
    if (units.empty()) {
        rows.push_back(text("No matching package units") | dim);
    } else {
        for (std::size_t index = 0; index < units.size(); ++index) {
            const auto *unit = units[index];
            unit_hits.push_back({unit->id, {}});
            auto card = vbox({
                            text(unit->title.empty() ? unit->id : unit->title) | bold,
                            text(unit->id + " · " + unit->category) | dim,
                        }) |
                        border;
            if (index == selected_unit) {
                card = card | color(Color::Cyan);
                if (active_pane == Pane::Units)
                    card = card | focus;
            }
            if ((dragged_unit && *dragged_unit == unit->id) || (carried_unit && *carried_unit == unit->id))
                card = card | dim;
            rows.push_back(card | reflect(unit_hits.back().box));
        }
    }
    return vbox(std::move(rows)) | vscroll_indicator | yframe | flex;
}

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
) {
    using namespace ftxui;
    if (event == Event::Character("/")) {
        searching = true;
        return true;
    }
    if (units.empty())
        return false;
    if (event == Event::ArrowUp || event == Event::Character("k")) {
        selected_unit = selected_unit == 0 ? units.size() - 1 : selected_unit - 1;
        return true;
    }
    if (event == Event::ArrowDown || event == Event::Character("j")) {
        selected_unit = (selected_unit + 1) % units.size();
        return true;
    }
    if (event == Event::Return) {
        if (!selected_route) {
            transient_status = "Error: select a graph route first";
        } else {
            const auto route = *selected_route;
            const auto unit  = units[selected_unit]->id;
            act_fn([&] { selected_node = editor.insert_on_route(route, unit); });
        }
        return true;
    }
    if (event == Event::Character("p")) {
        if (!selected_route) {
            transient_status = "Error: select a graph route first";
        } else {
            const auto route = *selected_route;
            const auto unit  = units[selected_unit]->id;
            act_fn([&] { selected_node = editor.add_parallel_on_route(route, unit); });
        }
        return true;
    }
    return false;
}

} // namespace apg::terminal
