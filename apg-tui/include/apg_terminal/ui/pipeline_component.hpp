#ifndef APG_TERMINAL_PIPELINE_COMPONENT_HPP
#define APG_TERMINAL_PIPELINE_COMPONENT_HPP

#include <ftxui/component/component_base.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace apg::terminal {

struct PipelineItem {
    std::string id;
    std::string unit;
};

struct DragState {
    bool        active = false;
    std::string unit_id;
};

enum class DropPosition {
    Before,
    After,
};

ftxui::Component draggable_pipeline(
    std::function<std::vector<PipelineItem>()>                                  items,
    int                                                                        *selected,
    std::function<void(std::size_t)>                                            on_select,
    std::function<void(const std::string &, const std::string &, DropPosition)> on_drop,
    std::function<void(const std::string &, const std::string &, DropPosition)> on_unit_drop,
    DragState                                                                  *drag_state
);

} // namespace apg::terminal

#endif
