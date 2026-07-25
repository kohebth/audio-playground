#include "apg_terminal/pipeline_component.hpp"
#include "apg_terminal/pipeline_unit_card.hpp"

#include <ftxui/component/captured_mouse.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

namespace apg::terminal {
namespace {

class DraggablePipeline final : public ftxui::ComponentBase {
  public:
    DraggablePipeline(
        std::function<std::vector<PipelineItem>()>                                  items,
        int                                                                        *selected,
        std::function<void(std::size_t)>                                            on_select,
        std::function<void(const std::string &, const std::string &, DropPosition)> on_drop,
        std::function<void(const std::string &, const std::string &, DropPosition)> on_unit_drop,
        DragState                                                                  *drag_state
    )
        : items_provider_(std::move(items)), selected_(selected), on_select_(std::move(on_select)),
          on_drop_(std::move(on_drop)), on_unit_drop_(std::move(on_unit_drop)), drag_state_(drag_state) {}

    ftxui::Element OnRender() override {
        using namespace ftxui;

        items_ = items_provider_();
        boxes_.assign(items_.size(), {});
        if (items_.empty())
            return text("Input -> Output") | dim;

        normalize_selection();
        Elements elements;
        for (std::size_t index = 0; index < items_.size(); ++index) {
            const bool is_hovered = dragging_ && hover_index_ == index && items_[index].id != dragged_id_;
            const auto card_renderer = PipelineUnitCard{};
            const auto card = card_renderer.Render(
                items_[index],
                selected_ && *selected_ == static_cast<int>(index),
                dragging_ && items_[index].id == dragged_id_,
                is_hovered,
                drop_position_,
                boxes_[index]
            );
            elements.push_back(std::move(card));

            if (index + 1 < items_.size())
                elements.push_back(text("→"));
        }
        return hflow(std::move(elements));
    }

    bool OnEvent(ftxui::Event event) override {
        using namespace ftxui;

        if (event.is_mouse())
            return on_mouse(event);
        if (items_.empty() || !selected_)
            return false;
        const auto item_count = static_cast<int>(items_.size());
        const auto normalize = [&] {
            *selected_ = std::clamp(*selected_, 0, item_count - 1);
        };
        const auto set_selected = [&](int next) {
            *selected_ = (next + item_count) % item_count;
            on_select_(static_cast<std::size_t>(*selected_));
        };
        if (event == Event::ArrowLeft || event == Event::Character("h")) {
            normalize();
            *selected_ = std::max(0, *selected_ - 1);
            on_select_(static_cast<std::size_t>(*selected_));
            return true;
        }
        if (event == Event::ArrowRight || event == Event::Character("l")) {
            normalize();
            *selected_ = std::min(item_count - 1, *selected_ + 1);
            on_select_(static_cast<std::size_t>(*selected_));
            return true;
        }
        if (event == Event::Tab) {
            normalize();
            set_selected(*selected_ + 1);
            return true;
        }
        if (event == Event::TabReverse) {
            normalize();
            set_selected(*selected_ - 1);
            return true;
        }
        return false;
    }

    bool Focusable() const override { return true; }

  private:
    std::optional<std::size_t> hit(int x, int y) const {
        for (std::size_t index = 0; index < boxes_.size(); ++index) {
            if (!boxes_[index].IsEmpty() && boxes_[index].Contain(x, y))
                return index;
        }
        return std::nullopt;
    }

    DropPosition drop_position(std::size_t index, int x) const {
        const auto &box = boxes_[index];
        return x > box.x_min + (box.x_max - box.x_min) / 2 ? DropPosition::After : DropPosition::Before;
    }

    bool on_mouse(ftxui::Event event) {
        using namespace ftxui;

        const auto target = hit(event.mouse().x, event.mouse().y);
        if (captured_mouse_) {
            hover_index_ = target;
            if (target)
                drop_position_ = drop_position(*target, event.mouse().x);
            if (event.mouse().motion == Mouse::Released) {
                const auto dragged  = dragged_id_;
                const auto dropped  = target ? items_[*target].id : std::string{};
                const auto position = drop_position_;
                dragging_           = false;
                dragged_id_.clear();
                hover_index_.reset();
                captured_mouse_.reset();
                if (!dropped.empty() && dropped != dragged)
                    on_drop_(dragged, dropped, position);
            }
            return true;
        }
        if (drag_state_ && drag_state_->active && event.mouse().motion == Mouse::Released) {
            const auto unit     = drag_state_->unit_id;
            drag_state_->active = false;
            drag_state_->unit_id.clear();
            if (!target)
                return true;
            const auto position = drop_position(*target, event.mouse().x);
            on_unit_drop_(unit, items_[*target].id, position);
            return true;
        }
        if (!target || event.mouse().button != Mouse::Left || event.mouse().motion != Mouse::Pressed)
            return false;

        captured_mouse_ = CaptureMouse(event);
        if (!captured_mouse_)
            return false;
        TakeFocus();
        dragging_      = true;
        dragged_id_    = items_[*target].id;
        hover_index_   = target;
        drop_position_ = drop_position(*target, event.mouse().x);
        if (selected_)
            *selected_ = static_cast<int>(*target);
        on_select_(*target);
        return true;
    }

    void normalize_selection() {
        if (!selected_)
            return;
        *selected_ = std::clamp(*selected_, 0, static_cast<int>(items_.size()) - 1);
    }

    std::function<std::vector<PipelineItem>()>                                  items_provider_;
    int                                                                        *selected_;
    std::function<void(std::size_t)>                                            on_select_;
    std::function<void(const std::string &, const std::string &, DropPosition)> on_drop_;
    std::function<void(const std::string &, const std::string &, DropPosition)> on_unit_drop_;
    DragState                                                                  *drag_state_;
    std::vector<PipelineItem>                                                   items_;
    std::vector<ftxui::Box>                                                     boxes_;
    ftxui::CapturedMouse                                                        captured_mouse_;
    bool                                                                        dragging_ = false;
    std::string                                                                 dragged_id_;
    std::optional<std::size_t>                                                  hover_index_;
    DropPosition                                                                drop_position_ = DropPosition::Before;
};

} // namespace

ftxui::Component draggable_pipeline(
    std::function<std::vector<PipelineItem>()>                                  items,
    int                                                                        *selected,
    std::function<void(std::size_t)>                                            on_select,
    std::function<void(const std::string &, const std::string &, DropPosition)> on_drop,
    std::function<void(const std::string &, const std::string &, DropPosition)> on_unit_drop,
    DragState                                                                  *drag_state
) {
    return std::make_shared<DraggablePipeline>(
        std::move(items), selected, std::move(on_select), std::move(on_drop), std::move(on_unit_drop), drag_state
    );
}

} // namespace apg::terminal
