#include "apg_terminal/domain/project_document.hpp"
#include "apg_terminal/domain/project_document_internal.hpp"

extern "C" {
#include <apgcore/compiler/project_compiler_v2.h>
#include <apgcore/registry/registry_builder_v2.h>
#include <apgcore/validator/project_v2.h>
#include <yaml/arena.h>
#include <yaml/error.h>
}

namespace apg::terminal {

std::string ValidationReport::summary() const {
    if (diagnostics.empty())
        return "OK";
    std::ostringstream value;
    value << diagnostics.front().message;
    if (diagnostics.size() > 1)
        value << " (and " << diagnostics.size() - 1 << " more)";
    return value.str();
}

bool UnitReference::user_placeable() const {
    return routing.role == RoutingRole::None && inputs.size() == 1 && outputs.size() == 1;
}

ApgPackageDocument ApgPackageDocument::load(const std::filesystem::path &path) {
    if (path.extension() != ".apg")
        fail("APG terminal projects must be web-compatible .apg packages, not raw YAML.");
    std::ifstream input(path, std::ios::binary);
    if (!input)
        fail("Unable to open APG package \"" + path.string() + "\".");
    const std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return parse(content, path, true);
}

ApgPackageDocument
ApgPackageDocument::parse(const std::string &json, const std::filesystem::path &source_path, bool validate_core) {
    try {
        return from_json(Json::parse(json), source_path, validate_core);
    } catch (const nlohmann::json::exception &error) { fail(std::string("Invalid APG package JSON: ") + error.what()); }
}

ApgPackageDocument
ApgPackageDocument::from_json(Json package, const std::filesystem::path &source_path, bool validate_core) {
    if (!package.is_object())
        fail("APG project package must be an object.");
    if (!package.contains("schema") || package["schema"] != kPackageSchema || !package.contains("version") ||
        package["version"] != kPackageVersion)
        fail("APG project package must use apg.project.package.v1 version 1.");

    const auto &manifest = required_object(package, "manifest", "APG project package manifest");
    (void)required_string(manifest, "id", "APG project id");
    (void)required_string(manifest, "name", "APG project name");
    (void)required_string(manifest, "createdAt", "APG project createdAt");
    (void)required_string(manifest, "updatedAt", "APG project updatedAt");
    const auto mode = required_string(manifest, "lastMode", "APG project mode");
    if (mode != "simple" && mode != "pro")
        fail("APG project package mode is invalid.");
    if (manifest.contains("description") && !manifest["description"].is_string())
        fail("APG project description must be a string.");

    const auto &workspace = required_object(package, "workspace", "Project workspace");
    if (!workspace.contains("schema") || workspace["schema"] != kWorkspaceSchema || !workspace.contains("version") ||
        workspace["version"] != kWorkspaceVersion)
        fail("Workspace payload must use apg.ui.workspace.v2 format version 2.");
    const auto &files = required_array(workspace, "files", "Workspace files");
    if (files.empty())
        fail("Workspace payload must contain files.");

    std::set<std::string> paths;
    std::size_t           entry_index = std::numeric_limits<std::size_t>::max();
    for (std::size_t index = 0; index < files.size(); ++index) {
        const auto &file = files[index];
        if (!file.is_object())
            fail("Workspace file " + std::to_string(index) + " is invalid.");
        const auto path       = required_string(file, "path", "Workspace file path");
        const auto normalized = normalized_workspace_path(path);
        if (!normalized || *normalized != path)
            fail("Workspace file " + std::to_string(index) + " has an invalid path.");
        if (!paths.insert(path).second)
            fail("Workspace file path \"" + path + "\" is duplicated.");
        const auto role = required_string(file, "role", "Workspace file role");
        if (role != "project" && role != "unit")
            fail("Workspace file \"" + path + "\" has an invalid role.");
        if (!file.contains("content") || !file["content"].is_string())
            fail("Workspace file \"" + path + "\" has invalid content.");
    }

    const auto entry            = required_string(workspace, "entryProject", "Workspace entry project");
    const auto normalized_entry = normalized_workspace_path(entry);
    if (!normalized_entry || *normalized_entry != entry)
        fail("Workspace entry project has an invalid path.");
    for (std::size_t index = 0; index < files.size(); ++index) {
        if (files[index]["path"] == entry && files[index]["role"] == "project") {
            entry_index = index;
            break;
        }
    }
    if (entry_index == std::numeric_limits<std::size_t>::max())
        fail("Workspace entry project is missing or is not a project file.");

    const auto           &audio = required_array(package, "audio", "APG project package audio");
    std::set<std::string> audio_ids;
    for (std::size_t index = 0; index < audio.size(); ++index) {
        const auto &asset = audio[index];
        if (!asset.is_object())
            fail("Audio asset must be an object.");
        const auto id = required_string(asset, "id", "Audio asset id");
        (void)required_string(asset, "name", "Audio asset name");
        (void)required_string(asset, "mimeType", "Audio asset mimeType");
        if (!asset.contains("channels") || asset["channels"] != 1)
            fail("Audio Playground project packages only support mono audio assets.");
        if (!asset.contains("encoding") || asset["encoding"] != "base64")
            fail("Audio asset encoding must be base64.");
        if (!asset.contains("data") || !asset["data"].is_string())
            fail("Audio asset data must be a string.");
        for (const char *field : {"sampleRate", "durationSeconds"}) {
            if (!asset.contains(field) || asset[field].is_null())
                continue;
            if (!asset[field].is_number() || !std::isfinite(asset[field].get<double>()) ||
                asset[field].get<double>() < 0.0)
                fail(std::string("Audio asset ") + field + " must be a positive number.");
        }
        if (!audio_ids.insert(id).second)
            fail("Audio asset id \"" + id + "\" is duplicated.");
    }

    const auto &readiness    = required_object(package, "readiness", "Project readiness");
    const auto  valid_status = [](const Json &value) {
        return value.is_string() && (value == "unknown" || value == "ready" || value == "blocked");
    };
    if (!readiness.contains("validation") || !valid_status(readiness["validation"]) || !readiness.contains("preview") ||
        !valid_status(readiness["preview"]))
        fail("Project readiness status is invalid.");
    if (!readiness.contains("checkedAt") ||
        !(readiness["checkedAt"].is_null() || nonempty_string(readiness["checkedAt"])))
        fail("Project readiness checkedAt is invalid.");
    const auto &target_statuses = required_object(readiness, "targets", "Project readiness targets");
    for (const auto &[target, value] : target_statuses.items()) {
        (void)target;
        if (!valid_status(value))
            fail("Project readiness target status is invalid.");
    }
    const auto &diagnostics = required_array(readiness, "diagnostics", "Project readiness diagnostics");
    for (const auto &diagnostic : diagnostics) {
        if (!diagnostic.is_object() || !diagnostic.contains("message") || !nonempty_string(diagnostic["message"]))
            fail("Project readiness diagnostic is invalid.");
    }

    ApgPackageDocument result;
    result.path_                  = source_path;
    result.package_               = std::make_shared<const Json>(std::move(package));
    result.entry_file_index_      = entry_index;
    result.entry_project_         = entry;
    result.project_content_       = (*result.package_)["workspace"]["files"][entry_index]["content"].get<std::string>();
    result.saved_project_content_ = result.project_content_;
    result.refresh_project_view();
    if (validate_core) {
        const auto report = result.validate_core();
        if (!report.ok())
            fail("APGCore rejected the packaged project: " + report.summary());
    }
    return result;
}

void ApgPackageDocument::refresh_project_view() {
    YAML::Node root;
    try {
        root = YAML::Load(project_content_);
    } catch (const YAML::Exception &error) { fail(std::string("Invalid entry project YAML: ") + error.what()); }
    if (!root || !root.IsMap())
        fail("Entry project YAML must be a mapping.");

    name_    = scalar(root, "name", "unnamed_project");
    version_ = scalar(root, "version", "1.0.0");
    units_.clear();
    nodes_.clear();
    routes_.clear();
    scenes_.clear();
    export_targets_.clear();

    const auto                                     files = workspace_files();
    std::unordered_map<std::string, WorkspaceFile> files_by_path;
    for (const auto &file : files)
        files_by_path.emplace(file.path, file);

    std::unordered_set<std::string> active_unit_ids;
    const auto                      active_nodes = root["chain"]["nodes"];
    if (active_nodes && active_nodes.IsSequence()) {
        for (const auto &node : active_nodes) {
            const auto unit_id = scalar(node, "unit");
            if (!unit_id.empty())
                active_unit_ids.insert(unit_id);
        }
    }

    const auto unit_nodes = root["units"];
    if (unit_nodes && unit_nodes.IsSequence()) {
        for (const auto &item : unit_nodes) {
            UnitReference unit;
            unit.id   = scalar(item, "id");
            unit.file = scalar(item, "file");
            if (unit.id.empty() || unit.file.empty())
                fail("Project unit references require id and file.");
            const auto resolved = resolve_workspace_path(entry_project_, unit.file);
            auto file     = files_by_path.find(resolved);
            if (file == files_by_path.end()) {
                for (auto it = files_by_path.begin(); it != files_by_path.end(); ++it) {
                    if (std::filesystem::path(it->first).filename() == std::filesystem::path(resolved).filename()) {
                        file = it;
                        break;
                    }
                }
            }
            const bool active   = active_unit_ids.contains(unit.id);
            if (file == files_by_path.end() || file->second.role != "unit") {
                if (active)
                    fail("Unit source for \"" + unit.id + "\" is missing from the APG package.");
                unit.name     = unit.id;
                unit.title    = unit.id;
                unit.category = "unavailable";
                units_.push_back(std::move(unit));
                continue;
            }

            try {
                const auto unit_root = YAML::Load(file->second.content);
                if (!unit_root || !unit_root.IsMap())
                    fail("Unit \"" + resolved + "\" must be a YAML mapping.");
                unit.name     = scalar(unit_root, "name", unit.id);
                unit.title    = scalar(unit_root["meta"], "title", unit.name);
                unit.category = scalar(unit_root["meta"], "category", "other");

                const auto params = unit_root["params"];
                if (params && params.IsMap()) {
                    for (const auto &entry : params) {
                        Parameter parameter;
                        parameter.name          = entry.first.Scalar();
                        const auto definition   = entry.second;
                        const auto type         = scalar(definition, "type", "float");
                        parameter.type          = type == "int" ? ParameterType::Integer : ParameterType::Float;
                        parameter.default_text  = scalar(definition, "default", "0");
                        parameter.default_value = scalar_number(definition, "default", 0.0);
                        parameter.value         = parameter.default_value;
                        parameter.min           = scalar_number(definition, "min", parameter.default_value);
                        parameter.max           = scalar_number(definition, "max", parameter.default_value);
                        const auto ui           = definition["ui"];
                        parameter.label         = scalar(ui, "label", parameter.name);
                        parameter.unit          = scalar(ui, "unit");
                        parameter.control       = scalar(ui, "control", "knob");
                        parameter.scale         = scalar(ui, "scale", "linear") == "log" ? ParameterScale::Logarithmic
                                                                                         : ParameterScale::Linear;
                        parameter.precision     = static_cast<int>(std::clamp(
                            scalar_number(ui, "display_precision", parameter.type == ParameterType::Integer ? 0 : 2),
                            0.0, 9.0
                        ));
                        unit.parameters.push_back(std::move(parameter));
                    }
                }

                const auto read_ports = [](const YAML::Node &ports) {
                    std::vector<std::string> names;
                    if (!ports || !ports.IsSequence())
                        return names;
                    for (const auto &port : ports) {
                        if (scalar(port, "type") == "audio")
                            names.push_back(scalar(port, "name"));
                    }
                    return names;
                };
                const auto ports = unit_root["ports"];
                if (ports && ports.IsMap()) {
                    unit.inputs  = read_ports(ports["inputs"]);
                    unit.outputs = read_ports(ports["outputs"]);
                }

                const auto routing = unit_root["routing"];
                if (routing && routing.IsMap()) {
                    const auto role   = scalar(routing, "role");
                    unit.routing.role = role == "panner"  ? RoutingRole::Panner
                                        : role == "mixer" ? RoutingRole::Mixer
                                                          : RoutingRole::None;
                    const auto paths  = routing["paths"];
                    if (paths && paths.IsSequence()) {
                        for (const auto &path : paths)
                            unit.routing.paths.push_back({scalar(path, "port"), scalar(path, "level_param")});
                    }
                }
                const auto compatibility = unit_root["compatibility"];
                if (compatibility && compatibility.IsMap()) {
                    for (const auto &entry : compatibility) {
                        try {
                            unit.compatibility[entry.first.Scalar()] = entry.second.as<bool>();
                        } catch (const YAML::Exception &) { unit.compatibility[entry.first.Scalar()] = false; }
                    }
                }
            } catch (const std::bad_alloc &) { throw; } catch (const std::exception &error) {
                if (active)
                    fail("Invalid active unit \"" + resolved + "\": " + error.what());
                const auto id   = unit.id;
                const auto path = unit.file;
                unit            = {};
                unit.id         = id;
                unit.file       = path;
                unit.name       = id;
                unit.title      = id;
                unit.category   = "unavailable";
            }
            units_.push_back(std::move(unit));
        }
    }
    ensure_panner_mixer_pair(units_);

    const auto node_nodes = root["chain"]["nodes"];
    if (node_nodes && node_nodes.IsSequence()) {
        for (const auto &item : node_nodes) {
            Node node;
            node.id              = scalar(item, "id");
            node.unit            = scalar(item, "unit");
            node.routing_section = scalar(item["routing"], "section");
            const auto params    = item["params"];
            if (params && params.IsMap()) {
                for (const auto &parameter : params)
                    node.params[parameter.first.Scalar()] = parameter.second.Scalar();
            }
            const auto *unit = find_unit(node.unit);
            if (unit) {
                node.parameter_specs = unit->parameters;
                for (auto &parameter : node.parameter_specs) {
                    const auto value = node.params.find(parameter.name);
                    if (value == node.params.end())
                        continue;
                    try {
                        const double parsed = std::stod(value->second);
                        if (std::isfinite(parsed))
                            parameter.value = parsed;
                    } catch (const std::exception &) {
                        // APGCore reports the invalid scalar with contract location.
                    }
                }
            }
            nodes_.push_back(std::move(node));
        }
    }

    routes_                = yaml_routes(root);
    const auto scene_nodes = root["scenes"];
    if (scene_nodes && scene_nodes.IsSequence()) {
        for (const auto &item : scene_nodes) {
            Scene scene;
            scene.name        = scalar(item, "name");
            const auto params = item["params"];
            if (params && params.IsMap()) {
                for (const auto &entry : params)
                    scene.params[entry.first.Scalar()] = entry.second.Scalar();
            }
            const auto bypass = item["bypass"];
            if (bypass && bypass.IsMap()) {
                for (const auto &entry : bypass) {
                    try {
                        scene.bypass[entry.first.Scalar()] = entry.second.as<bool>();
                    } catch (const YAML::Exception &) {
                        // APGCore supplies the authoritative diagnostic.
                    }
                }
            }
            scenes_.push_back(std::move(scene));
        }
    }

    default_target_    = scalar(root["targets"], "default", "desktop_full");
    const auto exports = root["targets"]["export"];
    if (exports && exports.IsSequence()) {
        for (const auto &profile : exports)
            export_targets_.push_back(profile.Scalar());
    }
}

ValidationReport ApgPackageDocument::validate_core(float sample_rate, std::uint32_t maximum_frames) const {
    ValidationReport report;
    try {
        TemporaryDirectory workspace;
        const auto         entry_path = materialize_to(workspace.path());
        uc_arena           project_arena{};
        if (uc_arena_init(&project_arena, 16u * 1024u * 1024u) != 0) {
            report.diagnostics.push_back(
                {"APG_CORE_OOM", entry_project_, "Unable to allocate APGCore validation arena."}
            );
            return report;
        }
        apg_project_v2_resolved_t resolved{};
        apg_project_v2_compiled_t compiled{};
        uc_error                  error{};
        uc_status                 status = apg_project_v2_load_resolved_file_with_root(
            entry_path.string().c_str(), workspace.path().string().c_str(), &project_arena, &resolved, &error
        );
        if (status == UC_OK)
            status = apg_project_v2_compile(&resolved, &project_arena, &compiled, &error);

        uc_arena registry_arena{};
        if (status == UC_OK) {
            const apg_prepare_context_t context = {
                .maximum_frames = maximum_frames,
                .sample_rate    = sample_rate,
            };
            apg_v2_registry_t registry{};
            status = apg_v2_registry_build_with_growth(&compiled.plan, &context, &registry_arena, &registry, &error);
        }
        if (status != UC_OK) {
            report.diagnostics.push_back({
                "APG_CORE_VALIDATION",
                entry_project_,
                error.msg[0] != '\0' ? error.msg : uc_status_str(status),
            });
        }
        if (registry_arena.base)
            uc_arena_free(&registry_arena);
        uc_arena_free(&project_arena);
    } catch (const std::exception &error) {
        report.diagnostics.push_back({"APG_PACKAGE_IO", entry_project_, error.what()});
    }
    return report;
}

void ApgPackageDocument::replace_project_content(std::string content, bool validate) {
    const auto previous = project_content_;
    project_content_    = std::move(content);
    try {
        refresh_project_view();
        if (validate) {
            const auto report = validate_core();
            if (!report.ok())
                fail(report.summary());
        }
    } catch (...) {
        project_content_ = previous;
        refresh_project_view();
        throw;
    }
}

} // namespace apg::terminal
