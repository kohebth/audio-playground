#include "apg_terminal/pipeline_unit_card.hpp"

#include <ftxui/dom/elements.hpp>

namespace apg::terminal {

ftxui::Element PipelineUnitCard::Render(
    const PipelineItem &item,
    bool                is_selected,
    bool                is_dragged,
    bool                is_hovered,
    DropPosition        hover_position,
    ftxui::Box         &box,
    bool                fixed_height
) const {
    return Render(item.id, is_selected, is_dragged, is_hovered, hover_position, box, fixed_height);
}

ftxui::Element PipelineUnitCard::Render(
    const std::string &text,
    bool               is_selected,
    bool               is_dragged,
    bool               is_hovered,
    DropPosition       hover_position,
    ftxui::Box        &box,
    bool               fixed_height
) const {
    using namespace ftxui;

    auto card = ftxui::text(text) | border;
    if (fixed_height)
        card = card | size(HEIGHT, EQUAL, 3);
    if (is_selected)
        card = card | bold | color(Color::Cyan);
    if (is_hovered)
        card = card | inverted | color(hover_position == DropPosition::Before ? Color::Yellow : Color::Magenta);
    if (is_dragged)
        card = card | dim;
    return card | reflect(box);
}

} // namespace apg::terminal
