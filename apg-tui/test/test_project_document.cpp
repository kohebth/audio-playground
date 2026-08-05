#include "apg_terminal/application/editor.hpp"
#include "apg_terminal/domain/project_document.hpp"

#include <nlohmann/json.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using apg::terminal::ApgPackageDocument;
using apg::terminal::Route;
using apg::terminal::TopologyElement;

namespace {

std::string read_file(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    assert(input);
    return {(std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>()};
}

nlohmann::ordered_json make_package(const std::string &entry_project, const std::vector<std::string> &unit_files) {
    nlohmann::ordered_json files = nlohmann::ordered_json::array();
    files.push_back({
        {   "path",                               entry_project},
        {   "role",                                   "project"},
        {"content", read_file("test/fixtures/" + entry_project)},
    });
    for (const auto &path : unit_files) {
        files.push_back({
            {   "path",                               path},
            {   "role",                             "unit"},
            {"content", read_file("test/fixtures/" + path)},
        });
    }
    return {
        {          "schema","apg.project.package.v1"                            },
        {         "version",                        1},
        {        "manifest",
         {
         {"id", "terminal-test"},
         {"name", "Terminal Test"},
         {"description", "Package contract test"},
         {"createdAt", "2026-07-25T00:00:00.000Z"},
         {"updatedAt", "2026-07-25T00:00:00.000Z"},
         {"lastMode", "pro"},
         }                                           },
        {       "workspace",
         {
         {"schema", "apg.ui.workspace.v2"},
         {"version", 2},
         {"entryProject", entry_project},
         {"files", std::move(files)},
         }                                           },
        {           "audio",      nlohmann::ordered_json::array({
      {
      {"id", "input-audio"},
      {"name", "input.wav"},
      {"mimeType", "audio/wav"},
      {"channels", 1},
      {"sampleRate", 48000},
      {"durationSeconds", 0.25},
      {"encoding", "base64"},
      {"data", "AAECAwQFBgc="},
      },
      })                 },
        {       "readiness",
         {
         {"checkedAt", nullptr},
         {"validation", "unknown"},
         {"preview", "unknown"},
         {"targets", nlohmann::ordered_json::object()},
         {"diagnostics", nlohmann::ordered_json::array()},
         }                                           },
        {"unknownExtension",     {{"preserve", true}}},
    };
}

template <typename Function> bool throws(Function &&function) {
    try {
        function();
        return false;
    } catch (const std::exception &) { return true; }
}

std::filesystem::path unique_temp_package() {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("apg-terminal-project-" + std::to_string(nonce) + ".apg");
}

} // namespace

int main() {
    auto package = make_package("projects-v2/simple-gain-board.project.v2.yaml", {"units-v2/simple_gain.unit.v2.yaml"});
    auto document = ApgPackageDocument::parse(package.dump(), "memory.apg");
    assert(document.name() == "simple-gain-board");
    assert(document.nodes().size() == 1);
    assert(document.routes().size() == 2);
    assert(document.scenes().size() == 2);
    assert(document.validate_core().ok());
    assert(!document.dirty());

    const auto *gain = document.find_node("gain1");
    assert(gain != nullptr);
    assert(gain->parameter_specs.size() == 1);
    assert(gain->parameter_specs.front().label == "Gain");
    assert(gain->parameter_specs.front().max == 4.0);

    auto unchanged = document;
    unchanged.set_param("gain1", "gain", 2.0);
    assert(!unchanged.dirty());

    document.set_param("gain1", "gain", 100.0);
    assert(document.find_node("gain1")->parameter_specs.front().value == 4.0);
    assert(document.dirty());

    const auto inserted = document.insert_on_route(Route{"gain1.output", "system.output"}, "gain_unit");
    assert(inserted == "simple_gain");
    assert(document.find_node(inserted) != nullptr);
    document.move_to_route(inserted, Route{"system.input", "gain1.input"});
    assert((document.routes().front() == Route{"system.input", "simple_gain.input"}));
    document.remove_node(inserted);
    assert(document.find_node(inserted) == nullptr);
    assert(document.validate_core().ok());
    assert(throws([&] { (void)document.serialize_for_save(" \t "); }));

    document.upsert_scene(
        "Current", {
                       {"gain1", false}
    }
    );
    assert(document.find_scene("Current") != nullptr);
    assert(throws([&] {
        document.upsert_scene(
            "Current", {
                           {"gain1", false}
        }
        );
    }));
    document.upsert_scene(
        "Current",
        {
            {"gain1", true}
    },
        true
    );
    document.rename_scene("Current", "Current Sound");
    assert(document.find_scene("Current Sound") != nullptr);
    const auto recall = document.recall_scene("Boost");
    assert(recall.bypass.at("gain1"));
    assert(document.find_node("gain1")->parameter_specs.front().value == 3.0);
    document.remove_scene("Current Sound");
    assert(document.find_scene("Current Sound") == nullptr);

    auto no_scenes = ApgPackageDocument::parse(package.dump(), "no-scenes.apg");
    no_scenes.remove_scene("Unity");
    no_scenes.remove_scene("Boost");
    assert(no_scenes.scenes().empty());
    assert(no_scenes.project_content().find("scenes: []") != std::string::npos);
    assert(no_scenes.validate_core().ok());

    const auto output = unique_temp_package();
    {
        std::ofstream file(output, std::ios::binary);
        file << package.dump(2);
    }
    auto saved = ApgPackageDocument::load(output);
    saved.set_param("gain1", "gain", 2.5);
    saved.save_atomic("2026-07-27T12:34:56.789Z");
    assert(!saved.dirty());
    const auto persisted = nlohmann::ordered_json::parse(read_file(output));
    assert(persisted["manifest"]["updatedAt"] == "2026-07-27T12:34:56.789Z");
    assert(persisted["readiness"]["validation"] == "ready");
    assert(persisted["readiness"]["preview"] == "ready");
    assert(persisted["audio"][0]["data"] == "AAECAwQFBgc=");
    assert(persisted["unknownExtension"]["preserve"] == true);
    auto round_trip = ApgPackageDocument::load(output);
    assert(round_trip.find_node("gain1")->parameter_specs.front().value == 2.5);
    std::filesystem::remove(output);

    const auto history_output = unique_temp_package();
    {
        std::ofstream file(history_output, std::ios::binary);
        file << package.dump(2);
    }
    apg::terminal::ProjectEditor editor(ApgPackageDocument::load(history_output));
    editor.set_param("gain1", "gain", 2.5);
    assert(editor.dirty());
    editor.save("2026-07-27T12:34:56.789Z");
    assert(!editor.dirty());
    assert(editor.undo());
    assert(editor.dirty());
    assert(editor.redo());
    assert(!editor.dirty());
    assert(editor.undo());
    editor.save("2026-07-27T12:35:56.789Z");
    assert(!editor.dirty());
    assert(editor.redo());
    assert(editor.dirty());
    editor.save_scene("  Trimmed Scene  ");
    assert(editor.active_scene() && *editor.active_scene() == "Trimmed Scene");
    assert(editor.document().find_scene("Trimmed Scene"));
    std::filesystem::remove(history_output);

    auto invalid_path                             = package;
    invalid_path["workspace"]["files"][0]["path"] = "../escape.yaml";
    assert(throws([&] { (void)ApgPackageDocument::parse(invalid_path.dump(), "memory.apg"); }));
    auto whitespace_name                = package;
    whitespace_name["manifest"]["name"] = " \t ";
    assert(throws([&] { (void)ApgPackageDocument::parse(whitespace_name.dump(), "memory.apg"); }));
    assert(throws([&] { (void)ApgPackageDocument::parse("{}", "memory.apg"); }));

    auto web_fixture = ApgPackageDocument::load("test/fixtures/packages-v1/simple-gain.apg");
    web_fixture.set_param("gain1", "gain", 1.25);
    const auto web_round_trip =
        nlohmann::ordered_json::parse(web_fixture.serialize_for_save("2026-07-27T12:34:56.789Z"));
    const auto web_project = web_round_trip["workspace"]["files"][0]["content"].get<std::string>();
    assert(web_project.find("ui:") != std::string::npos);
    assert(web_project.find("position:") != std::string::npos);
    assert(web_round_trip["audio"][0]["data"] == "UklGRg==");

    auto catalog_only = ApgPackageDocument::parse(
        make_package("projects-v2/catalog-pass-through.project.v2.yaml", {}).dump(), "catalog-only.apg"
    );
    assert(catalog_only.units().size() == 3);
    assert(!catalog_only.units().front().user_placeable());
    assert(catalog_only.nodes().empty());
    assert(catalog_only.validate_core().ok());

    auto invalid_unused_package = make_package("projects-v2/catalog-pass-through.project.v2.yaml", {});
    invalid_unused_package["workspace"]["files"].push_back({
        {   "path", "projects-v2/missing-catalog-only.unit.v2.yaml"},
        {   "role",                                          "unit"},
        {"content",                                   "not: [valid"},
    });
    auto invalid_unused = ApgPackageDocument::parse(invalid_unused_package.dump(), "invalid-unused.apg");
    assert(invalid_unused.units().size() == 3);
    assert(!invalid_unused.units().front().user_placeable());
    assert(invalid_unused.validate_core().ok());

    auto active_catalog = ApgPackageDocument::parse(
        make_package("projects-v2/catalog-active.project.v2.yaml", {"units-v2/simple_gain.unit.v2.yaml"}).dump(),
        "active-catalog.apg"
    );
    assert(active_catalog.units().size() == 4);
    assert(active_catalog.nodes().size() == 1);
    assert(active_catalog.validate_core().ok());

    auto invalid_active_package                                = package;
    invalid_active_package["workspace"]["files"][1]["content"] = "not: [valid";
    assert(throws([&] { (void)ApgPackageDocument::parse(invalid_active_package.dump(), "invalid-active.apg"); }));

    auto parallel_package = make_package(
        "projects-v2/parallel-gain.project.v2.yaml", {
                                                         "units-v2/simple_gain.unit.v2.yaml",
                                                         "units-v2/path_panner_2.unit.v2.yaml",
                                                         "units-v2/path_mixer_2.unit.v2.yaml",
                                                     }
    );
    auto parallel = ApgPackageDocument::parse(parallel_package.dump(), "parallel.apg");
    auto topology = parallel.topology();
    assert(topology.elements.size() == 1);
    assert(topology.elements.front().kind == TopologyElement::Kind::Parallel);
    assert(topology.elements.front().parallel->paths.size() == 2);

    const Route direct{
        "parallel_pan.path_1",
        "parallel_mix.path_1",
    };
    const auto nested_effect = parallel.add_parallel_on_route(direct, "gain_unit");
    assert(parallel.find_node(nested_effect) != nullptr);
    topology = parallel.topology();
    assert(topology.elements.front().parallel->paths.front().sequence->elements.size() == 1);
    assert(
        topology.elements.front().parallel->paths.front().sequence->elements.front().kind ==
        TopologyElement::Kind::Parallel
    );
    assert(parallel.validate_core().ok());

    auto collapsible = ApgPackageDocument::parse(parallel_package.dump(), "collapsible.apg");
    collapsible.remove_node("boost");
    collapsible.collapse_parallel("parallel_1");
    assert(collapsible.nodes().empty());
    assert(collapsible.routes().size() == 1);
    assert((collapsible.routes().front() == Route{"system.input", "system.output"}));
    assert(collapsible.validate_core().ok());

    auto collision_package = parallel_package;
    for (auto &file : collision_package["workspace"]["files"]) {
        if (file["path"] != "units-v2/simple_gain.unit.v2.yaml")
            continue;
        auto       content = file["content"].get<std::string>();
        const auto at      = content.find("name: simple_gain");
        assert(at != std::string::npos);
        content.replace(at, std::string("name: simple_gain").size(), "name: parallel_2_pan");
        file["content"] = std::move(content);
    }
    auto                  collision        = ApgPackageDocument::parse(collision_package.dump(), "collision.apg");
    const auto            collision_effect = collision.add_parallel_on_route(direct, "gain_unit");
    std::set<std::string> collision_ids;
    for (const auto &node : collision.nodes())
        assert(collision_ids.insert(node.id).second);
    assert(collision_effect != "parallel_2_pan");

    auto empty_split_package = make_package(
        "projects-v2/simple-gain-board.project.v2.yaml",
        {"units-v2/simple_gain.unit.v2.yaml", "units-v2/path_panner_2.unit.v2.yaml", "units-v2/path_mixer_2.unit.v2.yaml"}
    );
    auto empty_split_doc     = ApgPackageDocument::parse(empty_split_package.dump(), "empty_split.apg");
    const auto empty_route   = empty_split_doc.routes().front();
    const auto panner_id     = empty_split_doc.add_parallel_on_route(empty_route, "");
    assert(!panner_id.empty());
    assert(empty_split_doc.find_node(panner_id) != nullptr);
    assert(empty_split_doc.validate_core().ok());

    auto wrap_doc = ApgPackageDocument::parse(empty_split_package.dump(), "wrap.apg");
    const auto target_node_id = wrap_doc.nodes().front().id;
    const auto wrap_panner_id = wrap_doc.wrap_node_in_parallel(target_node_id);
    assert(!wrap_panner_id.empty());
    assert(wrap_doc.find_node(wrap_panner_id) != nullptr);
    const auto wrap_topo = wrap_doc.topology();
    assert(wrap_topo.elements.front().kind == TopologyElement::Kind::Parallel);
    auto pedalboard_doc = ApgPackageDocument::load("test/fixtures/packages-v1/guitar-pedalboard.apg");
    const auto pb_route = pedalboard_doc.routes().front();
    const auto pb_panner = pedalboard_doc.add_parallel_on_route(pb_route, "");
    assert(!pb_panner.empty());
    assert(pedalboard_doc.find_node(pb_panner) != nullptr);
    assert(pedalboard_doc.validate_core().ok());

    {
        auto route_doc = ApgPackageDocument::load("test/fixtures/packages-v1/guitar-pedalboard.apg");
        const auto guitar_order = route_doc.node_ids_in_route_order();
        const std::vector<std::string> expected_guitar = {
            "gate1", "phaser", "drive1", "tone1", "trem1", "chorus1", "delay1", "reverb1",
        };
        assert(guitar_order == expected_guitar);
        std::vector<std::string> doc_order;
        for (const auto &node : route_doc.nodes())
            doc_order.push_back(node.id);
        assert(guitar_order != doc_order);
    }
    {
        auto nested_doc = ApgPackageDocument::load("test/fixtures/packages-v1/parallel-nested-chain.apg");
        const auto nested_order = nested_doc.node_ids_in_route_order();
        const std::vector<std::string> expected_nested = {
            "parallel_pan", "drive",      "chorus",       "preamp",         "parallel_mix",
            "parallel_2_pan", "chorus_3", "parallel_3_pan", "tone_stack",   "parallel_3_mix",
            "parallel_2_mix",
        };
        assert(nested_order == expected_nested);
    }
    return 0;
}
