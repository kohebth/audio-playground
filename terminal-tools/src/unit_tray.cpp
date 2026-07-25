#include "apg_terminal/pipeline_unit_card.hpp"
#include "apg_terminal/ui.hpp"

#include <ftxui/component/captured_mouse.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include <algorithm>
#include <functional>
#include <optional>
#include <vector>

namespace apg::terminal {

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
            auto card = PipelineUnitCard{}.Render("+ " + values_[index].id, false, false, false, DropPosition::Before, boxes_[index]);
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
                        drag_state_->active = true;
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
    std::vector<ftxui::Box>                   boxes_;
    std::size_t                                offset_ = 0;
    DragState                                  *drag_state_;
    std::optional<std::size_t>                  pressed_index_;
};

ftxui::Component unit_tray(
    std::function<std::vector<UnitTrayItem>()> units,
    std::function<void(const std::string &)>   on_add,
    DragState                                 *drag_state
) {
    return std::make_shared<UnitTray>(std::move(units), std::move(on_add), drag_state);
}

} // namespace apg::terminal
