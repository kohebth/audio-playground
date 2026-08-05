#include "apg_terminal/ui/studio_graph_view.hpp"

namespace apg::terminal {
namespace {

ftxui::Element render_node_item(
    const ProjectEditor &editor,
    const std::string   &node_id,
    bool                 helper,
    Pane                 active_pane,
    const std::string   &selected_node,
    std::deque<NodeHit> &node_hits
) {
    using namespace ftxui;
    node_hits.push_back({node_id, {}});
    const auto *node    = editor.document().find_node(node_id);
    const auto *unit    = node ? editor.document().find_unit(node->unit) : nullptr;
    const auto  label   = unit && !unit->title.empty() ? unit->title : node_id;
    auto        element = vbox({
                       text(label) | bold,
                       filler(),
                       text(
                           node_id + (helper                     ? " · routing"
                                             : editor.bypassed(node_id) ? " · BYPASS"
                                                                        : "")
                       ) | dim,
                   }) |
                   border | size(HEIGHT, EQUAL, kGraphNodeHeight);
    if (selected_node == node_id) {
        element = element | color(Color::Cyan);
        if (active_pane == Pane::Graph)
            element = element | focus;
    }
    if (!helper && editor.bypassed(node_id))
        element = element | dim;
    return (element | reflect(node_hits.back().box)) | vcenter;
}

ftxui::Element render_route_item(
    const Route                &route,
    Pane                        active_pane,
    const std::optional<Route> &selected_route,
    const std::optional<Route> &hovered_route,
    bool                        route_drop_active,
    std::deque<RouteHit>       &route_hits
) {
    using namespace ftxui;
    route_hits.push_back({route, {}});
    const bool hovered  = hovered_route && *hovered_route == route;
    const bool selected = selected_route && *selected_route == route;
    auto       element  = text(hovered || selected ? "──>──" : "──>──");
    if (hovered) {
        element = element | color(Color::Green) | bold;
    } else if (route_drop_active) {
        element = element | color(Color::Cyan);
    } else if (selected) {
        element = element | color(Color::Yellow) | bold;
        if (active_pane == Pane::Graph)
            element = element | focus;
    } else {
        element = element | color(Color::GrayDark);
    }
    return (element | reflect(route_hits.back().box)) | vcenter;
}

ftxui::Element render_compact_helper_node(
    const ProjectEditor &editor,
    const std::string   &node_id,
    const std::string   &default_label,
    Pane                 active_pane,
    const std::string   &selected_node,
    std::deque<NodeHit> &node_hits
) {
    using namespace ftxui;
    node_hits.push_back({node_id, {}});
    const auto *node  = editor.document().find_node(node_id);
    const auto *unit  = node ? editor.document().find_unit(node->unit) : nullptr;
    const auto  label = (unit && !unit->title.empty()) ? unit->title : (!node_id.empty() ? node_id : default_label);

    auto element = vbox({
                       text(" " + label + " ") | bold,
                   }) |
                   border;
    if (selected_node == node_id) {
        element = element | color(Color::Cyan);
        if (active_pane == Pane::Graph)
            element = element | focus;
    }
    return (element | reflect(node_hits.back().box)) | vcenter;
}

ftxui::Element render_sequence_item(
    const ProjectEditor        &editor,
    const TopologySequence     &sequence,
    int                         depth,
    Pane                        active_pane,
    const std::string          &selected_node,
    const std::optional<Route> &selected_route,
    const std::optional<Route> &hovered_route,
    bool                        route_drop_active,
    std::deque<RouteHit>       &route_hits,
    std::deque<NodeHit>        &node_hits
) {
    using namespace ftxui;
    if (depth > 32)
        return text("nesting limit") | color(Color::Red);
    Elements items;
    for (std::size_t index = 0; index < sequence.routes.size(); ++index) {
        items.push_back(render_route_item(
            sequence.routes[index], active_pane, selected_route, hovered_route, route_drop_active, route_hits
        ));
        if (index >= sequence.elements.size())
            continue;
        const auto &element = sequence.elements[index];
        if (element.kind == TopologyElement::Kind::Effect) {
            items.push_back(render_node_item(editor, element.node_id, false, active_pane, selected_node, node_hits));
        } else if (element.parallel) {
            const auto       &panner_id = element.parallel->panner_id;
            const auto       &mixer_id  = element.parallel->mixer_id;
            const auto       &paths_vec = element.parallel->paths;
            const std::size_t num_paths = paths_vec.size();

            auto pan_card = render_compact_helper_node(editor, panner_id, "Pan", active_pane, selected_node, node_hits);
            auto mix_card = render_compact_helper_node(editor, mixer_id, "Mix", active_pane, selected_node, node_hits);

            if (num_paths < 2) {
                items.push_back(
                    hbox({
                        pan_card,
                        text("───") | vcenter | color(Color::GrayDark),
                        paths_vec.empty() || !paths_vec[0].sequence
                            ? text("disconnected") | dim | vcenter
                            : render_sequence_item(
                                  editor, *paths_vec[0].sequence, depth + 1, active_pane, selected_node, selected_route,
                                  hovered_route, route_drop_active, route_hits, node_hits
                              ),
                        text("───") | vcenter | color(Color::GrayDark),
                        mix_card,
                    }) |
                    vcenter
                );
                continue;
            }

            // Top branch (path 0)
            const auto &top_path = paths_vec[0];
            auto        top_seq  = top_path.sequence
                                       ? render_sequence_item(
                                     editor, *top_path.sequence, depth + 1, active_pane, selected_node, selected_route,
                                     hovered_route, route_drop_active, route_hits, node_hits
                                 )
                                       : text("──────") | dim | vcenter;

            auto top_left_conn  = vbox({
                filler(),
                text("    ┌") | color(Color::GrayDark),
                text("    │") | color(Color::GrayDark),
                text("    │") | color(Color::GrayDark),
            });
            auto top_right_conn = vbox({
                filler(),
                text("┐    ") | color(Color::GrayDark),
                text("│    ") | color(Color::GrayDark),
                text("│    ") | color(Color::GrayDark),
            });

            auto top_row = hbox({
                top_left_conn,
                top_seq | vcenter,
                top_right_conn,
            });

            // Bottom branch (path N-1)
            const auto &bot_path = paths_vec[num_paths - 1];
            auto        bot_seq  = bot_path.sequence
                                       ? render_sequence_item(
                                     editor, *bot_path.sequence, depth + 1, active_pane, selected_node, selected_route,
                                     hovered_route, route_drop_active, route_hits, node_hits
                                 )
                                       : text("──────") | dim | vcenter;

            auto bot_left_conn  = vbox({
                text("    │") | color(Color::GrayDark),
                text("    │") | color(Color::GrayDark),
                text("    └") | color(Color::GrayDark),
                filler(),
            });
            auto bot_right_conn = vbox({
                text("│    ") | color(Color::GrayDark),
                text("│    ") | color(Color::GrayDark),
                text("┘    ") | color(Color::GrayDark),
                filler(),
            });

            auto bot_row = hbox({
                bot_left_conn,
                bot_seq | vcenter,
                bot_right_conn,
            });

            // Middle paths (if any)
            Elements mid_rows;
            for (std::size_t i = 1; i < num_paths - 1; ++i) {
                const auto &m_path = paths_vec[i];
                auto        m_seq  = m_path.sequence
                                         ? render_sequence_item(
                                       editor, *m_path.sequence, depth + 1, active_pane, selected_node, selected_route,
                                       hovered_route, route_drop_active, route_hits, node_hits
                                   )
                                         : text("──────") | dim | vcenter;

                auto m_left_conn = vbox({
                                       text("     │     ") | color(Color::GrayDark),
                                       text("     ├──>──") | color(Color::GrayDark),
                                       text("     │     ") | color(Color::GrayDark),
                                   }) |
                                   vcenter;

                auto m_right_conn = vbox({
                                        text("     │     ") | color(Color::GrayDark),
                                        text("──>──┤     ") | color(Color::GrayDark),
                                        text("     │     ") | color(Color::GrayDark),
                                    }) |
                                    vcenter;

                mid_rows.push_back(hbox({
                    m_left_conn,
                    m_seq | vcenter,
                    m_right_conn,
                }));
            }

            // Center Panner / Mixer row
            auto center_row = hbox({
                                  pan_card,
                                  filler(),
                                  mix_card,
                              }) |
                              vcenter;

            Elements all_rows;
            all_rows.push_back(top_row);
            for (auto &r : mid_rows)
                all_rows.push_back(std::move(r));
            all_rows.push_back(center_row);
            all_rows.push_back(bot_row);

            items.push_back(vbox(std::move(all_rows)) | vcenter);
        }
    }
    return hbox(std::move(items));
}

} // namespace

ftxui::Element render_graph_view(
    ProjectEditor            &editor,
    const GraphRenderOptions &options,
    std::deque<RouteHit>     &route_hits,
    std::deque<NodeHit>      &node_hits,
    std::string              &render_error
) {
    using namespace ftxui;
    render_error.clear();
    try {
        const auto topology = editor.document().topology();
        return vbox({
                   render_sequence_item(
                       editor, topology, 0, options.active_pane, options.selected_node, options.selected_route,
                       options.hovered_route, options.route_drop_active, route_hits, node_hits
                   ),
                   filler(),
               }) |
               focusPosition(options.scroll_x.offset, options.scroll_y.offset) | hscroll_indicator | vscroll_indicator |
               xframe | yframe | flex;
    } catch (const std::exception &error) {
        render_error = error.what();
        return vbox({
                   text("Unable to render route topology") | bold | color(Color::Red),
                   paragraph(error.what()),
               }) |
               flex;
    }
}

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
) {
    using namespace ftxui;
    const auto &nodes  = editor.document().nodes();
    const auto &routes = editor.document().routes();
    if ((event == Event::ArrowLeft || event == Event::Character("h")) && !nodes.empty()) {
        cycle_node_fn(-1);
        return true;
    }
    if ((event == Event::ArrowRight || event == Event::Character("l")) && !nodes.empty()) {
        cycle_node_fn(1);
        return true;
    }
    if ((event == Event::Character("r") || event == Event::ArrowDown) && !routes.empty()) {
        cycle_route_fn(1);
        return true;
    }
    if (event == Event::ArrowUp && !routes.empty()) {
        cycle_route_fn(-1);
        return true;
    }
    if (event == Event::Return) {
        active_pane = Pane::Inspector;
        return true;
    }
    if (event == Event::Character("x") && selected_route && !selected_node.empty()) {
        const auto node        = selected_node;
        const auto destination = *selected_route;
        act_fn([&] { editor.move_to_route(node, destination); });
        return true;
    }
    if (event == Event::Character("c") && !selected_node.empty()) {
        const auto *node = editor.document().find_node(selected_node);
        if (!node || node->routing_section.empty()) {
            transient_status = "Error: select a routing helper from an empty parallel section";
        } else {
            const auto section = node->routing_section;
            act_fn([&] { editor.collapse_parallel(section); });
        }
        return true;
    }
    if (event == Event::Character("d") && !selected_node.empty()) {
        const auto *node = editor.document().find_node(selected_node);
        if (!node) {
            transient_status = "Error: selected node not found";
        } else if (node->routing_helper()) {
            transient_status = "Error: cannot delete a routing helper directly; collapse the parallel section first";
        } else {
            const auto node_id = selected_node;
            selected_node.clear();
            act_fn([&] { editor.remove_node(node_id); });
        }
        return true;
    }
    return false;
}

} // namespace apg::terminal
