#include "apg_terminal/project_document.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

using apg::terminal::Node;
using apg::terminal::ProjectDocument;

int main() {
    const auto fixture  = std::filesystem::path("test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml");
    auto       document = ProjectDocument::load(fixture);
    assert(document.validate().empty());
    assert(document.nodes().size() == 8);
    assert(document.routes().size() == 9);
    const auto *drive = document.selected_node("drive1");
    assert(drive != nullptr);
    assert(drive->parameter_specs.size() == 3);
    assert(drive->parameter_specs.front().label == "Drive");
    assert(document.set_param("drive1", "drive", 100.0));
    assert(document.selected_node("drive1")->parameter_specs.front().value == 8.0);
    const auto has_route = [&](const std::string &from, const std::string &to) {
        for (const auto &route : document.routes()) {
            if (route.from == from && route.to == to)
                return true;
        }
        return false;
    };

    assert(document.set_param("drive1", "drive", 3.0));
    assert(document.add_after("drive1", Node{"test_effect", document.units().front().id, {}, {}}));
    assert(document.nodes().size() == 9);
    assert(document.remove("test_effect"));
    assert(document.nodes().size() == 8);
    assert(document.move_before("reverb1", "gate1"));
    assert(document.nodes().front().id == "reverb1");
    assert(has_route("system.input", "reverb1.input"));
    assert(has_route("reverb1.output", "gate1.input"));
    assert(document.move_after("reverb1", "delay1"));
    assert(document.nodes().back().id == "reverb1");
    assert(has_route("delay1.output", "reverb1.input"));
    assert(has_route("reverb1.output", "system.output"));
    assert(document.validate().empty());
    assert(document.nodes().size() == 8);

    const auto output = std::filesystem::temp_directory_path() / "apg-terminal-project-test.yaml";
    document.save_atomic(output);
    auto round_trip = ProjectDocument::load(output);
    assert(round_trip.validate().empty());
    assert(round_trip.nodes().size() == 8);
    std::filesystem::remove(output);

    auto parallel = ProjectDocument::load("test/fixtures/projects-v2/parallel-gain.project.v2.yaml");
    assert(!parallel.move_before("boost", "parallel_pan"));
    assert(parallel.nodes()[1].id == "boost");

    auto branch = ProjectDocument::load(fixture);
    assert(branch.add_parallel_branch("drive1"));
    assert(branch.validate().empty());
    assert(branch.nodes().size() == 10);
    assert(branch.routes().size() == 12);
    return 0;
}
