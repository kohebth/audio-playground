#include "apg_terminal/ui/studio_inspector_view.hpp"

namespace apg::terminal {

ftxui::Element render_inspector_view(
    const ProjectEditor      &editor,
    const std::string        &selected_node,
    std::size_t               selected_parameter,
    Pane                      active_pane,
    std::deque<ParameterHit> &parameter_hits
) {
    using namespace ftxui;
    const auto *node = editor.document().find_node(selected_node);
    if (!node)
        return text("Select a node in the graph") | dim;
    const auto *unit = editor.document().find_unit(node->unit);
    Elements    rows{
        text(node->id) | bold | color(Color::Cyan),
        text(unit ? unit->title : node->unit),
        text(
            node->routing_helper()      ? "Always active routing helper"
            : editor.bypassed(node->id) ? "Bypassed · b enables"
                                        : "Enabled · b bypasses"
        ) | dim,
        separator(),
    };
    if (node->parameter_specs.empty())
        rows.push_back(text("No public parameters") | dim);
    for (std::size_t index = 0; index < node->parameter_specs.size(); ++index) {
        const auto &parameter = node->parameter_specs[index];
        parameter_hits.push_back({node->id, parameter.name, parameter, {}});
        auto row = vbox({
            hbox({
                text(parameter.label) | flex,
                text(format_value(parameter)),
            }),
            gauge(static_cast<float>(parameter_ratio(parameter))) | color(Color::Green),
        });
        if (index == selected_parameter) {
            row = row | color(Color::Cyan);
            if (active_pane == Pane::Inspector)
                row = row | focus;
        }
        rows.push_back(row | reflect(parameter_hits.back().box));
    }
    if (!node->routing_helper())
        rows.push_back(text("b bypass · d remove · arrows/Page keys adjust") | dim);
    return vbox(std::move(rows)) | vscroll_indicator | yframe | flex;
}

bool handle_inspector_event(
    const ftxui::Event                        &event,
    ProjectEditor                             &editor,
    const std::string                         &selected_node,
    std::size_t                               &selected_parameter,
    Modal                                     &modal,
    std::function<void(std::function<void()>)> act_fn
) {
    using namespace ftxui;
    const auto *node = editor.document().find_node(selected_node);
    if (!node)
        return false;
    if (event == Event::Character("b")) {
        act_fn([&] { editor.toggle_bypass(node->id); });
        return true;
    }
    if (event == Event::Character("d") && !node->routing_helper()) {
        modal = Modal::DeleteNode;
        return true;
    }
    if (node->parameter_specs.empty())
        return false;
    if (event == Event::ArrowUp || event == Event::Character("k")) {
        selected_parameter = selected_parameter == 0 ? node->parameter_specs.size() - 1 : selected_parameter - 1;
        return true;
    }
    if (event == Event::ArrowDown || event == Event::Character("j")) {
        selected_parameter = (selected_parameter + 1) % node->parameter_specs.size();
        return true;
    }
    auto parameter = node->parameter_specs[selected_parameter];
    if (event == Event::Home) {
        act_fn([&] { editor.set_param(node->id, parameter.name, parameter.min); });
        return true;
    }
    if (event == Event::End) {
        act_fn([&] { editor.set_param(node->id, parameter.name, parameter.max); });
        return true;
    }
    double delta = 0.0;
    if (event == Event::ArrowLeft || event == Event::Character("h"))
        delta = -1.0;
    else if (event == Event::ArrowRight || event == Event::Character("l"))
        delta = 1.0;
    else if (event == Event::PageUp)
        delta = -5.0;
    else if (event == Event::PageDown)
        delta = 5.0;
    if (delta != 0.0) {
        const auto step = (parameter.max - parameter.min) / 40.0;
        act_fn([&] { editor.set_param(node->id, parameter.name, parameter.value + delta * step); });
        return true;
    }
    return false;
}

} // namespace apg::terminal
