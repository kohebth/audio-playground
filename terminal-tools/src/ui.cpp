#include "apg_terminal/ui.hpp"

#include <ftxui/component/captured_mouse.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace apg::terminal {

namespace {

const char *knob_chars = "○◴◑◷●◶◐◵";

std::size_t knob_index(double ratio) {
    return std::clamp(static_cast<std::size_t>(ratio * 7.999), std::size_t{0}, std::size_t{7});
}

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
            const auto &parameter = values_[index];
            const auto  ratio =
                parameter.max > parameter.min
                     ? std::clamp((parameter.value - parameter.min) / (parameter.max - parameter.min), 0.0, 1.0)
                     : 0.0;
            const auto  knob = std::string(1, knob_chars[knob_index(ratio)]);
            auto        row  = text(" " + parameter.label + " " + knob + " " + format_value(parameter.value) +
                               (parameter.unit.empty() ? "" : " " + parameter.unit)) |
                           reflect(boxes_[index]);
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
                const auto dx = mouse.x - last_x_;
                last_x_ = mouse.x;
                const auto values = parameters_();
                if (*drag_index_ < values.size()) {
                    const auto &p = values[*drag_index_];
                    const auto range = std::max(p.max - p.min, 0.0001);
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
            const auto step = std::max((p.max - p.min) / 40.0, 0.0001);
            on_adjust_(index, mouse.x < (box.x_min + box.x_max) / 2 ? -step : step);
            return true;
        }
        return false;
    }

  private:
    static std::string format_value(double value) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.3g", value);
        return buffer;
    }

    std::function<std::vector<ParameterItem>()>      parameters_;
    std::function<void(std::size_t, double)>         on_adjust_;
    std::vector<ParameterItem>                       values_;
    std::vector<ftxui::Box>                          boxes_;
    ftxui::CapturedMouse                             captured_mouse_;
    std::optional<std::size_t>                       drag_index_;
    int                                              last_x_ = 0;
};

class UnitTray final : public ftxui::ComponentBase {
  public:
    UnitTray(
        std::function<std::vector<UnitTrayItem>()> units,
        std::function<void(const std::string &)>   on_add,
        DragState                                 *drag_state
    )
        : units_(std::move(units)), on_add_(std::move(on_add)), drag_state_(drag_state) {}

    ftxui::Element OnRender() override {
        using namespace ftxui;
        values_ = units_();
        boxes_.assign(values_.size(), {});
        if (values_.empty())
            return text("No unit definitions") | dim;
        const auto visible = std::min<std::size_t>(values_.size(), 6);
        if (offset_ + visible > values_.size())
            offset_ = values_.size() - visible;
        Elements cards;
        for (std::size_t index = offset_; index < offset_ + visible; ++index) {
            auto card = text("+ " + values_[index].id) | border | reflect(boxes_[index]);
            cards.push_back(std::move(card));
            if (index + 1 < offset_ + visible)
                cards.push_back(text(" "));
        }
        return hbox(std::move(cards));
    }

    bool OnEvent(ftxui::Event event) override {
        using namespace ftxui;
        if (event.is_mouse()) {
            const auto &mouse = event.mouse();
            if (mouse.button == Mouse::WheelDown) {
                if (offset_ + 6 < values_.size())
                    ++offset_;
                return true;
            }
            if (mouse.button == Mouse::WheelUp) {
                if (offset_ > 0)
                    --offset_;
                return true;
            }
            if (mouse.button != Mouse::Left)
                return false;
            if (mouse.motion == Mouse::Released && drag_state_ && drag_state_->active) {
                drag_state_->active = false;
                drag_state_->unit_id.clear();
                if (pressed_index_ && boxes_[*pressed_index_].Contain(mouse.x, mouse.y))
                    on_add_(values_[*pressed_index_].id);
                pressed_index_.reset();
                return true;
            }
            if (mouse.motion != Mouse::Pressed)
                return false;
            for (std::size_t index = offset_; index < boxes_.size() && index < offset_ + 6; ++index) {
                if (boxes_[index].Contain(mouse.x, mouse.y)) {
                    if (drag_state_) {
                        drag_state_->active  = true;
                        drag_state_->unit_id = values_[index].id;
                    }
                    pressed_index_ = index;
                    return true;
                }
            }
        }
        return false;
    }

  private:
    std::function<std::vector<UnitTrayItem>()> units_;
    std::function<void(const std::string &)>   on_add_;
    std::vector<UnitTrayItem>                  values_;
    std::vector<ftxui::Box>                    boxes_;
    std::size_t                                offset_ = 0;
    DragState                                 *drag_state_;
    std::optional<std::size_t>                 pressed_index_;
};

} // namespace

ftxui::Component parameter_panel(
    std::function<std::vector<ParameterItem>()> parameters, std::function<void(std::size_t, double)> on_adjust
) {
    return std::make_shared<ParameterPanel>(std::move(parameters), std::move(on_adjust));
}

ftxui::Component unit_tray(
    std::function<std::vector<UnitTrayItem>()> units,
    std::function<void(const std::string &)>   on_add,
    DragState                                 *drag_state
) {
    return std::make_shared<UnitTray>(std::move(units), std::move(on_add), drag_state);
}

ftxui::Element render_pipeline_ui(
    const ftxui::Component &pipeline,
    const ftxui::Component &controls,
    const ftxui::Component &parameters,
    const ftxui::Component &tray,
    const std::string      &selected,
    const std::string      &audio,
    const std::string      &status,
    const std::string      &validation
) {
    using namespace ftxui;

    return vbox({
        text("APG Terminal Pipeline") | bold | color(Color::Cyan),
        hbox({
            vbox({text("Chain") | bold, pipeline->Render()}) | border | flex,
            vbox(
                {text("Unit controls") | bold, parameters->Render() | frame | flex, separator(),
                 text("Actions") | bold, controls->Render()}
            ) | border,
        }) | flex,
        separator(),
        vbox({text("Unit definitions") | bold, tray->Render() | frame}) | border,
        separator(),
        text("Selected: " + selected),
        text("Mouse: drag effect; yellow inserts before, magenta after. "
             "Keyboard: Tab/arrows, Enter to toggle transport, q to quit."),
        text("Audio: " + audio),
        text("Status: " + status),
        text("Validation: " + validation) | color(validation == "OK" ? Color::Green : Color::Red),
    }) |
    border;
}

} // namespace apg::terminal
