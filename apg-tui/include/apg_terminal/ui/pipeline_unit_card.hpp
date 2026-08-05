#ifndef APG_TERMINAL_PIPELINE_UNIT_CARD_HPP
#define APG_TERMINAL_PIPELINE_UNIT_CARD_HPP

#include "pipeline_component.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

namespace apg::terminal {

class PipelineUnitCard final {
  public:
    ftxui::Element Render(
        const PipelineItem &item,
        bool                is_selected,
        bool                is_dragged,
        bool                is_hovered,
        DropPosition        hover_position,
        ftxui::Box         &box,
        bool                fixed_height = true
    ) const;

    ftxui::Element Render(
        const std::string &text,
        bool               is_selected,
        bool               is_dragged,
        bool               is_hovered,
        DropPosition       hover_position,
        ftxui::Box        &box,
        bool               fixed_height = true
    ) const;
};

} // namespace apg::terminal

#endif
