#include "apg_terminal/domain/project_document.hpp"
#include "apg_terminal/domain/project_document_internal.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace apg::terminal {

TopologySequence ApgPackageDocument::topology() const {
    struct Builder {
        const ApgPackageDocument       &document;
        std::unordered_set<std::string> visited_routes;

        [[nodiscard]] std::string route_key(const Route &route) const { return route.from + "\n" + route.to; }

        const Route &route_from(const std::string &source) {
            const Route *match = nullptr;
            for (const auto &route : document.routes()) {
                if (route.from != source)
                    continue;
                if (match)
                    fail("Topology source \"" + source + "\" has multiple routes.");
                match = &route;
            }
            if (!match)
                fail("Topology source \"" + source + "\" is disconnected.");
            if (!visited_routes.insert(route_key(*match)).second)
                fail("Topology contains a cycle at \"" + source + "\".");
            return *match;
        }

        TopologySequence trace(const std::string &start, const std::string &stop) {
            TopologySequence sequence;
            std::string      source = start;
            for (;;) {
                const auto &route = route_from(source);
                sequence.routes.push_back(route);
                if (route.to == stop)
                    return sequence;
                if (route.to == "system.output") {
                    if (stop == "system.output")
                        return sequence;
                    fail("A nested route exits before its mixer input.");
                }

                const auto  endpoint = parse_endpoint(route.to);
                const auto *node     = document.find_node(endpoint.instance);
                if (!node)
                    fail("Topology references missing instance \"" + endpoint.instance + "\".");
                const auto *unit = document.find_unit(node->unit);
                if (!unit)
                    fail("Topology references missing unit \"" + node->unit + "\".");

                if (unit->routing.role == RoutingRole::Panner) {
                    const Node          *mixer_node = nullptr;
                    const UnitReference *mixer      = nullptr;
                    for (const auto &candidate : document.nodes()) {
                        if (candidate.routing_section != node->routing_section)
                            continue;
                        const auto *candidate_unit = document.find_unit(candidate.unit);
                        if (candidate_unit && candidate_unit->routing.role == RoutingRole::Mixer) {
                            if (mixer_node)
                                fail("Routing section \"" + node->routing_section + "\" has multiple mixers.");
                            mixer_node = &candidate;
                            mixer      = candidate_unit;
                        }
                    }
                    if (!mixer_node || !mixer || !routing_contracts_match(*unit, *mixer))
                        fail("Routing section \"" + node->routing_section + "\" is incomplete or incompatible.");
                    auto parallel       = std::make_shared<ParallelTopology>();
                    parallel->section   = node->routing_section;
                    parallel->panner_id = node->id;
                    parallel->mixer_id  = mixer_node->id;
                    for (const auto &path : unit->routing.paths) {
                        parallel->paths.push_back({
                            path.port,
                            std::make_shared<TopologySequence>(
                                trace(node->id + "." + path.port, mixer_node->id + "." + path.port)
                            ),
                        });
                    }
                    sequence.elements.push_back({
                        TopologyElement::Kind::Parallel,
                        node->id,
                        std::move(parallel),
                    });
                    if (mixer->outputs.size() != 1)
                        fail("Routing mixer \"" + mixer_node->id + "\" must have one output.");
                    source = mixer_node->id + "." + mixer->outputs.front();
                    continue;
                }
                if (unit->routing.role == RoutingRole::Mixer)
                    fail("Routing mixer \"" + node->id + "\" was reached outside its paired path.");
                if (unit->outputs.size() != 1)
                    fail("Ordinary effect \"" + node->id + "\" must expose one output.");
                sequence.elements.push_back({TopologyElement::Kind::Effect, node->id, nullptr});
                source = node->id + "." + unit->outputs.front();
            }
        }
    };

    Builder builder{*this, {}};
    auto    result = builder.trace("system.input", "system.output");
    if (builder.visited_routes.size() != routes_.size())
        fail("Project contains routes that are not reachable from system.input.");
    return result;
}

std::vector<std::string> ApgPackageDocument::node_ids_in_route_order() const {
    std::vector<std::string> ordered;
    const auto              &root = topology();

    const auto visit = [&](const auto &self, const TopologySequence &sequence) -> void {
        for (const auto &element : sequence.elements) {
            if (element.kind == TopologyElement::Kind::Effect) {
                ordered.push_back(element.node_id);
                continue;
            }
            if (!element.parallel)
                continue;
            const auto &paths = element.parallel->paths;
            ordered.push_back(element.parallel->panner_id);
            for (const auto &path : paths)
                if (path.sequence)
                    self(self, *path.sequence);
            ordered.push_back(element.parallel->mixer_id);
        }
    };

    visit(visit, root);
    return ordered;
}

std::string ApgPackageDocument::serialize_for_save(const std::string &timestamp) const {
    if (trim(timestamp).empty())
        fail("Save timestamp must be a non-empty string.");
    const auto report = validate_core();
    if (!report.ok())
        fail("Refusing to save invalid project: " + report.summary());

    Json package                                                = *package_;
    package["workspace"]["files"][entry_file_index_]["content"] = project_content_;
    package["manifest"]["updatedAt"]                            = timestamp;

    bool has_panner_file = false;
    bool has_mixer_file  = false;
    for (const auto &file : package["workspace"]["files"]) {
        const auto p = file["path"].get<std::string>();
        if (p.find("path_panner_2") != std::string::npos)
            has_panner_file = true;
        if (p.find("path_mixer_2") != std::string::npos)
            has_mixer_file = true;
    }
    if (!has_panner_file) {
        package["workspace"]["files"].push_back({
            {"path", "units-v2/path_panner_2.unit.v2.yaml"},
            {"role", "unit"},
            {"content", kDefaultPathPanner2Yaml},
        });
    }
    if (!has_mixer_file) {
        package["workspace"]["files"].push_back({
            {"path", "units-v2/path_mixer_2.unit.v2.yaml"},
            {"role", "unit"},
            {"content", kDefaultPathMixer2Yaml},
        });
    }

    std::vector<std::string> targets;
    if (!default_target_.empty())
        targets.push_back(default_target_);
    for (const auto &target : export_targets_) {
        if (std::find(targets.begin(), targets.end(), target) == targets.end())
            targets.push_back(target);
    }
    std::set<std::string> active_units;
    for (const auto &node : nodes_)
        active_units.insert(node.unit);

    Json target_statuses = Json::object();
    Json diagnostics     = Json::array();
    for (const auto &target : targets) {
        bool supported = true;
        for (const auto &unit_id : active_units) {
            const auto *unit = find_unit(unit_id);
            const auto  declaration =
                unit ? unit->compatibility.find(target) : std::map<std::string, bool>::const_iterator{};
            if (!unit || declaration == unit->compatibility.end() || !declaration->second) {
                supported = false;
                diagnostics.push_back({
                    {   "code",                                 "APG_UI_TARGET_UNSUPPORTED"},
                    {   "path",                                 unit ? unit->file : unit_id},
                    {"message", unit_id + " does not declare " + target + " compatibility."},
                });
            }
        }
        target_statuses[target] = supported ? "ready" : "blocked";
    }
    package["readiness"] = {
        {  "checkedAt",                  timestamp},
        { "validation",                    "ready"},
        {    "preview",                    "ready"},
        {    "targets", std::move(target_statuses)},
        {"diagnostics",     std::move(diagnostics)},
    };
    return package.dump(2, ' ', false, nlohmann::json::error_handler_t::strict);
}

void ApgPackageDocument::save_atomic(const std::optional<std::string> &timestamp) {
    if (path_.empty())
        fail("This APG package has no save path.");
    const auto serialized = serialize_for_save(timestamp.value_or(utc_timestamp()));
    const auto temporary  = path_.parent_path() / (path_.filename().string() + ".tmp." + random_suffix());

    std::error_code error;
    const auto      original_permissions = std::filesystem::status(path_, error).permissions();
    error.clear();
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
            fail("Unable to open temporary APG package \"" + temporary.string() + "\".");
        output.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
        output.flush();
        if (!output) {
            std::filesystem::remove(temporary, error);
            fail("Unable to write temporary APG package.");
        }
    }
    if (original_permissions != std::filesystem::perms::unknown) {
        std::filesystem::permissions(temporary, original_permissions, std::filesystem::perm_options::replace, error);
        error.clear();
    }

#if !defined(_WIN32)
    const int file_descriptor = ::open(temporary.c_str(), O_RDONLY);
    if (file_descriptor < 0 || ::fsync(file_descriptor) != 0) {
        if (file_descriptor >= 0)
            ::close(file_descriptor);
        std::filesystem::remove(temporary, error);
        fail("Unable to flush temporary APG package to disk.");
    }
    ::close(file_descriptor);
#endif

#if defined(_WIN32)
    if (!::MoveFileExW(temporary.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const std::error_code move_error(static_cast<int>(::GetLastError()), std::system_category());
        std::filesystem::remove(temporary, error);
        throw std::system_error(move_error, "Unable to atomically replace APG package");
    }
#else
    std::filesystem::rename(temporary, path_, error);
    if (error) {
        const auto rename_error = error;
        std::filesystem::remove(temporary, error);
        throw std::system_error(rename_error, "Unable to atomically replace APG package");
    }
#endif

#if !defined(_WIN32)
    const int directory_descriptor = ::open(path_.parent_path().empty() ? "." : path_.parent_path().c_str(), O_RDONLY);
    if (directory_descriptor >= 0) {
        (void)::fsync(directory_descriptor);
        ::close(directory_descriptor);
    }
#endif

    Json saved             = Json::parse(serialized);
    package_               = std::make_shared<const Json>(std::move(saved));
    saved_project_content_ = project_content_;
}

} // namespace apg::terminal
