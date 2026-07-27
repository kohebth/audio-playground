#include "apg_terminal/parameter_row.hpp"
#include "apg_terminal/pipeline_component.hpp"
#include "apg_terminal/ui.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>

#include <cassert>
#include <limits>

namespace {

ftxui::Event mouse_event(int x, int y, ftxui::Mouse::Motion motion) {
    ftxui::Mouse mouse;
    mouse.button = ftxui::Mouse::Left;
    mouse.motion = motion;
    mouse.x      = x;
    mouse.y      = y;
    return ftxui::Event::Mouse("", mouse);
}

ftxui::Event mouse_pressed(int x, int y) { return mouse_event(x, y, ftxui::Mouse::Pressed); }
ftxui::Event mouse_moved(int x, int y) { return mouse_event(x, y, ftxui::Mouse::Moved); }
ftxui::Event mouse_released(int x, int y) { return mouse_event(x, y, ftxui::Mouse::Released); }

} // namespace

int main() {
    using namespace ftxui;

    int  node_clicks   = 0;
    int  action_clicks = 0;
    auto node          = Button("drive1", [&] { ++node_clicks; });
    auto action        = Button("Save", [&] { ++action_clicks; });
    auto nodes         = Container::Horizontal({node});
    auto actions       = Container::Vertical({action});
    auto parameters    = Container::Vertical({});
    auto tray          = Container::Horizontal({});
    auto layout        = Container::Vertical({nodes, actions});
    auto root          = Renderer(layout, [&] {
        return apg::terminal::render_pipeline_ui(nodes, actions, parameters, tray, "drive1", "null", "ready", "OK");
    });

    Screen screen(80, 20);
    Render(screen, root->Render());

    for (int y = 0; y < 20 && node_clicks == 0; ++y) {
        for (int x = 0; x < 80 && node_clicks == 0; ++x) {
            root->OnEvent(mouse_pressed(x, y));
        }
    }
    const int node_click_count_after_first_find = node_clicks;
    for (int y = 0; y < 20 && action_clicks == 0; ++y) {
        for (int x = 0; x < 80 && action_clicks == 0; ++x) {
            root->OnEvent(mouse_pressed(x, y));
        }
    }

    assert(node_clicks >= node_click_count_after_first_find);
    assert(action_clicks == 1);

    int         selected       = 0;
    int         selected_event = -1;
    std::string dragged;
    std::string target;
    auto        dropped_position = apg::terminal::DropPosition::After;
    auto        pipeline         = apg::terminal::draggable_pipeline(
        [] {
            return std::vector<apg::terminal::PipelineItem>{
                { "gate1",  "gate"},
                {"drive1", "drive"},
                {"delay1", "delay"}
            };
        },
        &selected, [&](std::size_t index) { selected_event = static_cast<int>(index); },
        [&](const std::string &from, const std::string &to, apg::terminal::DropPosition position) {
            dragged          = from;
            target           = to;
            dropped_position = position;
        },
        [](const std::string &, const std::string &, apg::terminal::DropPosition) {}, nullptr
    );
    Screen pipeline_screen(100, 5);
    Render(pipeline_screen, pipeline->Render());

    auto locate = [&](int wanted, bool rightmost = false) {
        std::pair result{-1, -1};
        for (int y = 0; y < 5; ++y) {
            for (int x = 0; x < 100; ++x) {
                selected_event = -1;
                pipeline->OnEvent(mouse_pressed(x, y));
                pipeline->OnEvent(mouse_released(x, y));
                if (selected_event == wanted) {
                    result = {x, y};
                    if (!rightmost)
                        return result;
                }
            }
        }
        return result;
    };

    const auto source      = locate(0);
    const auto destination = locate(2);
    assert(source.first >= 0);
    assert(destination.first >= 0);
    pipeline->OnEvent(mouse_pressed(source.first, source.second));
    pipeline->OnEvent(mouse_moved(destination.first, destination.second));
    pipeline->OnEvent(mouse_released(destination.first, destination.second));
    assert(dragged == "gate1");
    assert(target == "delay1");
    assert(dropped_position == apg::terminal::DropPosition::Before);

    dragged.clear();
    target.clear();
    const auto destination_right = locate(2, true);
    pipeline->OnEvent(mouse_pressed(source.first, source.second));
    pipeline->OnEvent(mouse_moved(destination_right.first, destination_right.second));
    pipeline->OnEvent(mouse_released(destination_right.first, destination_right.second));
    assert(dragged == "gate1");
    assert(target == "delay1");
    assert(dropped_position == apg::terminal::DropPosition::After);

    selected       = 0;
    selected_event = -1;
    pipeline->OnEvent(Event::Tab);
    assert(selected_event == 1);
    pipeline->OnEvent(Event::TabReverse);
    assert(selected_event == 0);

    int    parameter_changes = 0;
    double parameter_delta   = 0.0;
    auto   knobs             = apg::terminal::parameter_panel(
        [] {
            return std::vector<apg::terminal::ParameterItem>{
                {"Drive", "x", 2.0, 1.0, 8.0}
            };
        },
        [&](std::size_t, double delta) {
            ++parameter_changes;
            parameter_delta = delta;
        }
    );
    Screen knob_screen(60, 4);
    Render(knob_screen, knobs->Render());
    for (int y = 0; y < 4 && parameter_changes == 0; ++y) {
        for (int x = 0; x < 60 && parameter_changes == 0; ++x)
            knobs->OnEvent(mouse_pressed(x, y));
    }
    assert(parameter_changes == 1);
    assert(parameter_delta != 0.0);

    bool                     unit_drop_called = false;
    int                      unit_index       = 0;
    apg::terminal::DragState unit_drag_state;
    unit_drag_state.active  = true;
    unit_drag_state.unit_id = "drive_unit";
    auto unit_drop_pipeline = apg::terminal::draggable_pipeline(
        [] {
            return std::vector<apg::terminal::PipelineItem>{
                { "gate1",  "gate"},
                {"drive1", "drive"},
                {"delay1", "delay"},
            };
        },
        &unit_index, [](std::size_t) {}, [](const std::string &, const std::string &, apg::terminal::DropPosition) {},
        [&](const std::string &, const std::string &, apg::terminal::DropPosition) { unit_drop_called = true; },
        &unit_drag_state
    );
    Screen unit_drop_screen(100, 5);
    Render(unit_drop_screen, unit_drop_pipeline->Render());
    unit_drop_pipeline->OnEvent(mouse_released(99, 4));
    assert(!unit_drag_state.active);
    assert(!unit_drop_called);
    assert(apg::terminal::ParameterRow::KnobIndex(-1.0) == 0);
    assert(apg::terminal::ParameterRow::KnobIndex(2.0) == 7);
    assert(apg::terminal::ParameterRow::KnobIndex(std::numeric_limits<double>::quiet_NaN()) == 0);
    assert(apg::terminal::ParameterRow::KnobSymbol(1.0) == "◵");
    return 0;
}
