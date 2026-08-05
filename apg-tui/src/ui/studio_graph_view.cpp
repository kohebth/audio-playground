#include "apg_terminal/ui/studio_graph_view.hpp"

#include <ftxui/screen/screen.hpp>

namespace apg::terminal {
namespace {

class BoxCapture : public ftxui::Node {
public:
    BoxCapture(ftxui::Element child, ftxui::Box &box) : ftxui::Node(ftxui::unpack(std::move(child))), box_(box) {}

    void ComputeRequirement() final {
        ftxui::Node::ComputeRequirement();
        requirement_ = children_[0]->requirement();
    }

    void SetBox(ftxui::Box box) final {
        box_ = box;
        ftxui::Node::SetBox(box);
        children_[0]->SetBox(box);
    }

    void Render(ftxui::Screen &screen) final { ftxui::Node::Render(screen); }

private:
    ftxui::Box &box_;
};

ftxui::Decorator capture(ftxui::Box &box) {
    return [&](ftxui::Element child) -> ftxui::Element { return std::make_shared<BoxCapture>(std::move(child), box); };
}

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
    const auto *node        = editor.document().find_node(node_id);
    const auto *unit        = node ? editor.document().find_unit(node->unit) : nullptr;
    const auto  label       = unit && !unit->title.empty() ? unit->title : node_id;
    const bool  is_bypassed = !helper && editor.bypassed(node_id);
    const auto bot_str     = node_id;
    Element    bot_line;
    if (helper) {
        bot_line = text(bot_str) | dim;
    } else if (is_bypassed) {
        bot_line = hbox({
            text(bot_str) | dim,
            filler(),
            text(" ○ ") | color(Color::Yellow),
        });
    } else {
        bot_line = hbox({
            text(bot_str) | dim,
            filler(),
            text(" ● ") | color(Color::Green),
        });
    }

    const bool is_selected = (selected_node == node_id);
    auto       content     = vbox({
        text(label) | bold,
        filler(),
        bot_line,
    });

    if (is_bypassed) {
        content = content | color(Color::GrayLight) | dim;
    }

    auto element = content | (is_selected ? borderStyled(DOUBLE) : border) |
                   size(HEIGHT, EQUAL, kGraphNodeHeight);
    if (is_selected) {
        element = element | color(Color::Cyan);
    }

    if (is_selected && active_pane == Pane::Graph)
        element = element | focus;

    return (element | capture(node_hits.back().box)) | vcenter;
}

class HorizontalRailNode : public ftxui::Node {
public:
    HorizontalRailNode(ftxui::Color color, bool show_arrow) : color_(color), show_arrow_(show_arrow) {}
    void ComputeRequirement() override {
        requirement_.min_x = 5;
        requirement_.min_y = 1;
        requirement_.flex_grow_x = 1;
    }
    void Render(ftxui::Screen &screen) override {
        for (int x = box_.x_min; x <= box_.x_max; ++x) {
            auto &pixel            = screen.PixelAt(x, box_.y_min);
            pixel.character        = "─";
            pixel.foreground_color = color_;
        }
        if (show_arrow_ && box_.x_max >= box_.x_min) {
            int   mid_x            = box_.x_min + (box_.x_max - box_.x_min) / 2;
            auto &pixel            = screen.PixelAt(mid_x, box_.y_min);
            pixel.character        = ">";
            pixel.foreground_color = color_;
        }
    }

private:
    ftxui::Color color_;
    bool         show_arrow_;
};

ftxui::Element horizontal_rail(ftxui::Color color = ftxui::Color::GrayDark, bool show_arrow = true) {
    return std::make_shared<HorizontalRailNode>(color, show_arrow);
}

ftxui::Element render_route_item(
    const Route                &route,
    Pane                        active_pane,
    const std::optional<Route> &selected_route,
    const std::optional<Route> &hovered_route,
    bool                        route_drop_active,
    std::deque<RouteHit>       &route_hits,
    ftxui::Element              custom_element = nullptr
) {
    using namespace ftxui;
    route_hits.push_back({route, {}});
    const bool hovered  = hovered_route && *hovered_route == route;
    const bool selected = selected_route && *selected_route == route;

    ftxui::Color color_to_use = Color::GrayDark;
    if (hovered) {
        color_to_use = Color::Green;
    } else if (route_drop_active) {
        color_to_use = Color::Cyan;
    } else if (selected) {
        color_to_use = Color::Yellow;
    }

    Element element;
    if (custom_element) {
        element = custom_element;
    } else {
        element = text("──>──");
    }

    element = element | color(color_to_use);
    if (hovered || selected)
        element = element | bold;
    if (selected && active_pane == Pane::Graph)
        element = element | focus;

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

    const bool is_selected = (selected_node == node_id);
    auto element = vbox({
                       text(" " + label + " ") | bold,
                   }) |
                   (is_selected ? borderStyled(DOUBLE) : border);
    if (is_selected) {
        element = element | color(Color::Cyan);
    }
    if (is_selected && active_pane == Pane::Graph)
        element = element | focus;
    return (element | capture(node_hits.back().box));
}

constexpr int kCenterRowHeight = 3;
constexpr int kCenterRowPort   = 1;
constexpr int kCenterRowBotOffset = kCenterRowHeight - kCenterRowPort;

struct LayoutMetrics {
    int total_height = 5;
    int port_row     = 2;
};

LayoutMetrics compute_element_metrics(const TopologyElement &el);
LayoutMetrics compute_sequence_metrics(const TopologySequence &sequence);

LayoutMetrics compute_element_metrics(const TopologyElement &el) {
    LayoutMetrics m{5, 2};
    if (el.kind == TopologyElement::Kind::Parallel && el.parallel) {
        const auto &paths = el.parallel->paths;
        if (paths.size() >= 2) {
            auto top_m = compute_sequence_metrics(*paths.front().sequence);
            auto bot_m = compute_sequence_metrics(*paths.back().sequence);
            int mid_h = 0;
            for (std::size_t j = 1; j + 1 < paths.size(); ++j) {
                mid_h += compute_sequence_metrics(*paths[j].sequence).total_height;
            }
            m.port_row = top_m.total_height + mid_h + kCenterRowPort;
            m.total_height = top_m.total_height + mid_h + kCenterRowHeight + bot_m.total_height;
        }
    }
    return m;
}

LayoutMetrics compute_sequence_metrics(const TopologySequence &sequence) {
    if (sequence.elements.empty()) {
        return {3, 0};
    }
    int max_height = 1;
    int max_port   = 0;
    for (std::size_t i = 0; i < sequence.elements.size(); ++i) {
        auto m = compute_element_metrics(sequence.elements[i]);
        max_port   = std::max(max_port, m.port_row);
        max_height = std::max(max_height, m.total_height);
    }
    return {max_height, max_port};
}

ftxui::Element pad_top(ftxui::Element el, int count) {
    using namespace ftxui;
    if (count <= 0) return el;
    Elements stack;
    for (int r = 0; r < count; ++r) {
        stack.push_back(text(""));
    }
    stack.push_back(std::move(el));
    return vbox(std::move(stack));
}

ftxui::Element make_top_left_connector(int total_height, int port_row) {
    using namespace ftxui;
    Elements lines;
    for (int r = 0; r < total_height; ++r) {
        if (r < port_row) {
            lines.push_back(text("    "));
        } else if (r == port_row) {
            lines.push_back(text("    ┌") | color(Color::GrayDark));
        } else {
            lines.push_back(text("    │") | color(Color::GrayDark));
        }
    }
    return vbox(std::move(lines));
}

ftxui::Element make_top_right_connector(int total_height, int port_row) {
    using namespace ftxui;
    Elements lines;
    for (int r = 0; r < total_height; ++r) {
        if (r < port_row) {
            lines.push_back(text("    "));
        } else if (r == port_row) {
            lines.push_back(text("┐    ") | color(Color::GrayDark));
        } else {
            lines.push_back(text("│    ") | color(Color::GrayDark));
        }
    }
    return vbox(std::move(lines));
}

ftxui::Element make_bot_left_connector(int bot_total_height, int bot_port_row) {
    using namespace ftxui;
    Elements lines;
    int target_r = bot_port_row;
    int max_r    = bot_total_height;
    for (int r = 0; r < max_r; ++r) {
        if (r < target_r) {
            lines.push_back(text("    │") | color(Color::GrayDark));
        } else if (r == target_r) {
            lines.push_back(text("    └") | color(Color::GrayDark));
        } else {
            lines.push_back(text("    "));
        }
    }
    return vbox(std::move(lines));
}

ftxui::Element make_bot_right_connector(int bot_total_height, int bot_port_row) {
    using namespace ftxui;
    Elements lines;
    int target_r = bot_port_row;
    int max_r    = bot_total_height;
    for (int r = 0; r < max_r; ++r) {
        if (r < target_r) {
            lines.push_back(text("│    ") | color(Color::GrayDark));
        } else if (r == target_r) {
            lines.push_back(text("┘    ") | color(Color::GrayDark));
        } else {
            lines.push_back(text("    "));
        }
    }
    return vbox(std::move(lines));
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

    auto seq_m       = compute_sequence_metrics(sequence);
    int  target_port = seq_m.port_row;

    if (depth == 0) {
        auto in_box = vbox({text(" IN ") | bold}) | border | color(Color::GrayLight);
        items.push_back(pad_top(in_box, target_port - 1));
    }
    for (std::size_t index = 0; index < sequence.elements.size(); ++index) {
        const auto &element = sequence.elements[index];
        if (depth > 0) {
            auto rail_el = render_route_item(
                sequence.routes[index], active_pane, selected_route, hovered_route, route_drop_active, route_hits,
                horizontal_rail()
            );
            items.push_back(pad_top(rail_el, target_port) | flex);
        } else {
            auto route_el = render_route_item(
                sequence.routes[index], active_pane, selected_route, hovered_route, route_drop_active, route_hits
            );
            items.push_back(pad_top(route_el, target_port));
        }

        if (element.kind == TopologyElement::Kind::Effect) {
            auto node_el = render_node_item(editor, element.node_id, false, active_pane, selected_node, node_hits);
            items.push_back(pad_top(node_el, target_port - 2));
        } else if (element.parallel) {
            const auto       &panner_id = element.parallel->panner_id;
            const auto       &mixer_id  = element.parallel->mixer_id;
            const auto       &paths_vec = element.parallel->paths;
            const std::size_t num_paths = paths_vec.size();

            auto pan_card = render_compact_helper_node(editor, panner_id, "Pan", active_pane, selected_node, node_hits);
            auto mix_card = render_compact_helper_node(editor, mixer_id, "Mix", active_pane, selected_node, node_hits);

            if (num_paths < 2) {
                items.push_back(
                    hbox(
                        pan_card,
                        text("───") | vcenter | color(Color::GrayDark),
                        paths_vec.empty() || !paths_vec[0].sequence
                            ? text("disconnected") | dim | vcenter
                            : render_sequence_item(
                                  editor, *paths_vec[0].sequence, depth + 1, active_pane, selected_node, selected_route,
                                  hovered_route, route_drop_active, route_hits, node_hits
                              ),
                        text("───") | vcenter | color(Color::GrayDark),
                        mix_card
                    ) |
                    vcenter
                );
                continue;
            }

            // Top branch (path 0)
            const auto &top_path     = paths_vec[0];
            const bool  top_is_empty = !top_path.sequence || (top_path.sequence->elements.empty() && top_path.sequence->routes.size() == 1);

            Element top_seq;
            if (top_is_empty) {
                if (top_path.sequence && !top_path.sequence->routes.empty()) {
                    auto rail_item = render_route_item(
                        top_path.sequence->routes[0], active_pane, selected_route, hovered_route, route_drop_active, route_hits,
                        horizontal_rail()
                    );
                    top_seq = vbox(
                        rail_item,
                        filler(),
                        filler()
                    );
                } else {
                    top_seq = vbox(
                        horizontal_rail(),
                        filler(),
                        filler()
                    );
                }
            } else {
                top_seq = render_sequence_item(
                    editor, *top_path.sequence, depth + 1, active_pane, selected_node, selected_route,
                    hovered_route, route_drop_active, route_hits, node_hits
                );
            }
            auto top_m = top_path.sequence ? compute_sequence_metrics(*top_path.sequence) : LayoutMetrics{3, 0};
            auto top_left_conn  = make_top_left_connector(top_m.total_height, top_m.port_row);
            auto top_right_conn = make_top_right_connector(top_m.total_height, top_m.port_row);

            auto top_row = hbox(
                top_left_conn,
                top_seq | flex,
                top_right_conn
            );

            // Bottom branch (path num_paths - 1)
            const auto &bot_path     = paths_vec[num_paths - 1];
            const bool  bot_is_empty = !bot_path.sequence || (bot_path.sequence->elements.empty() && bot_path.sequence->routes.size() == 1);

            Element bot_seq;
            if (bot_is_empty) {
                if (bot_path.sequence && !bot_path.sequence->routes.empty()) {
                    auto rail_item = render_route_item(
                        bot_path.sequence->routes[0], active_pane, selected_route, hovered_route, route_drop_active, route_hits,
                        horizontal_rail()
                    );
                    bot_seq = vbox(
                        filler(),
                        filler(),
                        rail_item
                    );
                } else {
                    bot_seq = vbox(
                        filler(),
                        filler(),
                        horizontal_rail()
                    );
                }
            } else {
                bot_seq = render_sequence_item(
                    editor, *bot_path.sequence, depth + 1, active_pane, selected_node, selected_route,
                    hovered_route, route_drop_active, route_hits, node_hits
                );
            }
            auto bot_m = (bot_is_empty || !bot_path.sequence) ? LayoutMetrics{3, 2} : compute_sequence_metrics(*bot_path.sequence);
            auto bot_left_conn  = make_bot_left_connector(bot_m.total_height, bot_m.port_row);
            auto bot_right_conn = make_bot_right_connector(bot_m.total_height, bot_m.port_row);

            auto bot_row = hbox(
                bot_left_conn,
                bot_seq | flex,
                bot_right_conn
            );

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

                mid_rows.push_back(hbox(
                    m_left_conn,
                    m_seq | vcenter,
                    m_right_conn
                ));
            }

            // Center Panner / Mixer row
            auto center_row = hbox(
                                  pan_card,
                                  filler(),
                                  mix_card
                              ) |
                              vcenter;

            Elements all_rows;
            all_rows.push_back(top_row);
            for (auto &r : mid_rows)
                all_rows.push_back(std::move(r));
            all_rows.push_back(center_row);
            all_rows.push_back(bot_row);

            auto parallel_box = vbox(std::move(all_rows));
            auto el_m = compute_element_metrics(element);
            items.push_back(pad_top(parallel_box, target_port - el_m.port_row));
        }
    }

    if (!sequence.routes.empty()) {
        const auto &last_route = sequence.routes.back();
        if (depth > 0) {
            auto rail_el = render_route_item(
                last_route, active_pane, selected_route, hovered_route, route_drop_active, route_hits,
                horizontal_rail()
            );
            items.push_back(pad_top(rail_el, target_port) | flex);
        } else {
            auto route_el = render_route_item(
                last_route, active_pane, selected_route, hovered_route, route_drop_active, route_hits
            );
            items.push_back(pad_top(route_el, target_port));
        }
    }

    if (depth == 0) {
        auto out_box = vbox({
                           text(" OUT ") | bold,
                       }) |
                       border | color(Color::GrayLight);
        items.push_back(pad_top(out_box, target_port - 1));
    }
    return hbox(std::move(items));
}

} // namespace

ftxui::Element render_graph_view(
    ProjectEditor            &editor,
    const GraphRenderOptions &options,
    std::deque<RouteHit>     &route_hits,
    std::deque<NodeHit>      &node_hits,
    std::string              &render_error,
    ftxui::Box               &content_box
) {
    using namespace ftxui;
    render_error.clear();
    try {
        const auto topology = editor.document().topology();
        auto       content  = vbox({
            filler(),
            render_sequence_item(
                editor, topology, 0, options.active_pane, options.selected_node, options.selected_route,
                options.hovered_route, options.route_drop_active, route_hits, node_hits
            ),
            filler(),
        });
        return (content | capture(content_box) | focusPosition(options.scroll_x.offset, options.scroll_y.offset)) |
               hscroll_indicator | vscroll_indicator | xframe | yframe | flex;
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
    if (event == Event::Character("b") && !selected_node.empty()) {
        const auto *node = editor.document().find_node(selected_node);
        if (!node) {
            transient_status = "Error: selected node not found";
        } else if (node->routing_helper()) {
            transient_status = "Error: routing helpers cannot be bypassed";
        } else {
            const auto node_id = selected_node;
            act_fn([&] { editor.toggle_bypass(node_id); });
        }
        return true;
    }
    if (event == Event::Character("x") && selected_route && !selected_node.empty()) {
        const auto node        = selected_node;
        const auto destination = *selected_route;
        act_fn([&] { editor.move_to_route(node, destination); });
        return true;
    }
    if (event == Event::Character("p")) {
        if (!selected_node.empty()) {
            const auto *node = editor.document().find_node(selected_node);
            if (!node) {
                transient_status = "Error: selected node not found";
            } else if (node->routing_helper()) {
                transient_status = "Error: cannot wrap a routing helper in a parallel section";
            } else {
                const auto node_id = selected_node;
                act_fn([&] { editor.wrap_node_in_parallel(node_id); });
            }
            return true;
        }
        if (selected_route) {
            const auto route = *selected_route;
            act_fn([&] { editor.add_parallel_on_route(route, ""); });
            return true;
        }
        transient_status = "Error: select an effect unit or route in graph first";
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
