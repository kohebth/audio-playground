#ifndef APG_TERMINAL_UI_HPP
#define APG_TERMINAL_UI_HPP

#include "pipeline_component.hpp"
#include <ftxui/component/component_base.hpp>
#include <ftxui/dom/node.hpp>

#include <string>

namespace apg::terminal {

struct ParameterItem {
    std::string label;
    std::string unit;
    double      value;
    double      min;
    double      max;
};

struct UnitTrayItem {
    std::string id;
};

ftxui::Component parameter_panel(
    std::function<std::vector<ParameterItem>()> parameters, std::function<void(std::size_t, double)> on_adjust
);

ftxui::Component unit_tray(
    std::function<std::vector<UnitTrayItem>()> units,
    std::function<void(const std::string &)>   on_add,
    DragState                                 *drag_state
);

ftxui::Element render_pipeline_ui(
    const ftxui::Component &pipeline,
    const ftxui::Component &controls,
    const ftxui::Component &parameters,
    const ftxui::Component &tray,
    const std::string      &selected,
    const std::string      &audio,
    const std::string      &status,
    const std::string      &validation
);

} // namespace apg::terminal

#endif
