#include "apg_terminal/pipeline_component.hpp"
#include "apg_terminal/project_document.hpp"

#include <ftxui/screen/screen.hpp>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <vector>

namespace {

void assert_full_card_shapes(const ftxui::Screen &screen, std::size_t expected_cards) {
    const auto width  = screen.dimx();
    const auto height = screen.dimy();
    int         detected_cards = 0;

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

void validate_fixture(const std::filesystem::path &fixture_path) {
    const auto document = apg::terminal::ProjectDocument::load(fixture_path.string());
    std::vector<apg::terminal::PipelineItem> items;
    items.reserve(document.nodes().size());
    for (const auto &node : document.nodes())
        items.push_back({node.id, node.unit});

    int selected = 0;
    auto pipeline = apg::terminal::draggable_pipeline(
        [items] { return items; },
        &selected,
        [](std::size_t) {},
        [](const std::string &, const std::string &, apg::terminal::DropPosition) {},
        [](const std::string &, const std::string &, apg::terminal::DropPosition) {},
        nullptr
    );

    const std::vector widths = {40, 80};
    for (const auto width : widths) {
        const int height = std::max(12, static_cast<int>(items.size() * 3 + 12));
        ftxui::Screen screen(width, height);
        ftxui::Render(screen, pipeline->Render());
        assert_full_card_shapes(screen, items.size());
    }
}

} // namespace

int main() {
    std::vector<std::filesystem::path> fixture_paths;
    for (const auto &entry : std::filesystem::recursive_directory_iterator("test/fixtures/projects-v2")) {
        if (!entry.is_regular_file())
            continue;
        if (entry.path().extension() == ".yaml" && entry.path().string().find(".project.v2.yaml") != std::string::npos)
            fixture_paths.push_back(entry.path());
    }
    std::sort(fixture_paths.begin(), fixture_paths.end());
    assert(!fixture_paths.empty());

    for (const auto &fixture_path : fixture_paths)
        validate_fixture(fixture_path);

    return 0;
}
