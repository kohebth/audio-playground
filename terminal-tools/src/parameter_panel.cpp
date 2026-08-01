#include "apg_terminal/parameter_row.hpp"
#include "apg_terminal/ui.hpp"

#include <ftxui/component/captured_mouse.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include <algorithm>
#include <functional>
#include <optional>
#include <vector>

namespace apg::terminal {

class ParameterPanel final : public ftxui::ComponentBase {
  public:
    ParameterPanel(
        std::function<std::vector<ParameterItem>()> parameters, std::function<void(std::size_t, double)> on_adjust
    )
        : parameters_(std::move(parameters)), on_adjust_(std::move(on_adjust)) {}

    ftxui::Element OnRender() override {
        using namespace ftxui;
        values_ = parameters_();
        boxes_.assign(values_.size(), {});
        if (values_.empty())
            return text("Select a unit") | dim;
        Elements rows;
        for (std::size_t index = 0; index < values_.size(); ++index) {
            auto row = ParameterRow::Render(values_[index], boxes_[index]);
            rows.push_back(std::move(row));
        }
        return vbox(std::move(rows));
    }

    bool OnEvent(ftxui::Event event) override {
        if (!event.is_mouse())
            return false;
        const auto &mouse = event.mouse();
        if (captured_mouse_) {
            if (mouse.motion == ftxui::Mouse::Released) {
                captured_mouse_.reset();
                return true;
            }
            if (mouse.motion == ftxui::Mouse::Moved && drag_index_) {
                const auto dx     = mouse.x - last_x_;
                last_x_           = mouse.x;
                const auto values = parameters_();
                if (*drag_index_ < values.size()) {
                    const auto &p     = values[*drag_index_];
                    const auto  range = std::max(p.max - p.min, 0.0001);
                    on_adjust_(*drag_index_, dx / 40.0 * range);
                }
                return true;
            }
            return false;
        }
        if (mouse.button != ftxui::Mouse::Left || mouse.motion != ftxui::Mouse::Pressed)
            return false;
        for (std::size_t index = 0; index < boxes_.size(); ++index) {
            const auto &box = boxes_[index];
            if (!box.Contain(mouse.x, mouse.y))
                continue;
            const auto values = parameters_();
            if (index >= values.size())
                return true;
            drag_index_      = index;
            last_x_          = mouse.x;
            captured_mouse_  = CaptureMouse(event);
            const auto &p    = values[index];
            const auto  step = std::max((p.max - p.min) / 40.0, 0.0001);
            on_adjust_(index, mouse.x < (box.x_min + box.x_max) / 2 ? -step : step);
            return true;
        }
        return false;
    }

  private:
    std::function<std::vector<ParameterItem>()> parameters_;
    std::function<void(std::size_t, double)>    on_adjust_;
    std::vector<ParameterItem>                  values_;
    std::vector<ftxui::Box>                     boxes_;
    ftxui::CapturedMouse                        captured_mouse_;
    std::optional<std::size_t>                  drag_index_;
    int                                         last_x_ = 0;
};

ftxui::Component parameter_panel(
    std::function<std::vector<ParameterItem>()> parameters, std::function<void(std::size_t, double)> on_adjust
) {
    return std::make_shared<ParameterPanel>(std::move(parameters), std::move(on_adjust));
}

} // namespace apg::terminal
