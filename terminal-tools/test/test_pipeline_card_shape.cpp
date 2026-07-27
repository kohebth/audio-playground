#include "apg_terminal/pipeline_component.hpp"

#include <ftxui/screen/screen.hpp>

#include <algorithm>
#include <cassert>
#include <vector>

namespace {

void assert_full_card_shapes(const ftxui::Screen &screen, std::size_t expected_cards) {
    const auto width          = screen.dimx();
    const auto height         = screen.dimy();
    int        detected_cards = 0;

    for (int y = 0; y + 2 < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (screen.CellAt(x, y).character != "╭")
                continue;
            ++detected_cards;

            int right = -1;
            for (int right_x = x + 1; right_x < width; ++right_x) {
                if (screen.CellAt(right_x, y).character == "╮") {
                    right = right_x;
                    break;
                }
            }
            assert(right > x);
            assert(screen.CellAt(x, y + 1).character == "│");
            assert(screen.CellAt(right, y + 1).character == "│");
            assert(screen.CellAt(x, y + 2).character == "╰");
            assert(screen.CellAt(right, y + 2).character == "╯");
        }
    }

    assert(static_cast<std::size_t>(detected_cards) == expected_cards);
}

void validate_count(std::size_t item_count) {
    std::vector<apg::terminal::PipelineItem> items;
    for (std::size_t index = 0; index < item_count; ++index)
        items.push_back({"node_" + std::to_string(index), "unit"});
    int  selected = 0;
    auto pipeline = apg::terminal::draggable_pipeline(
        [items] { return items; }, &selected, [](std::size_t) {},
        [](const std::string &, const std::string &, apg::terminal::DropPosition) {},
        [](const std::string &, const std::string &, apg::terminal::DropPosition) {}, nullptr
    );

    const std::vector widths = {24, 40, 80};
    for (const auto width : widths) {
        const int     height = std::max(12, static_cast<int>(items.size() * 3 + 3));
        ftxui::Screen screen(width, height);
        ftxui::Render(screen, pipeline->Render());
        assert_full_card_shapes(screen, items.size());
    }
}

} // namespace

int main() {
    validate_count(1);
    validate_count(3);
    validate_count(8);
    return 0;
}
