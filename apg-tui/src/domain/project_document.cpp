#include "apg_terminal/domain/project_document.hpp"

extern "C" {
#include <apgcore/compiler/project_compiler_v2.h>
#include <apgcore/registry/registry_builder_v2.h>
#include <apgcore/validator/project_v2.h>
#include <yaml/arena.h>
#include <yaml/error.h>
}

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <new>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

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
namespace {

constexpr const char *kPackageSchema    = "apg.project.package.v1";
constexpr int         kPackageVersion   = 1;
constexpr const char *kWorkspaceSchema  = "apg.ui.workspace.v2";
constexpr int         kWorkspaceVersion = 2;

[[noreturn]] void fail(const std::string &message) { throw std::runtime_error(message); }

bool nonempty_string(const ApgPackageDocument::Json &value) {
    if (!value.is_string())
        return false;
    const auto &text = value.get_ref<const std::string &>();
    return std::any_of(text.begin(), text.end(), [](unsigned char character) { return !std::isspace(character); });
}

const ApgPackageDocument::Json &
required_object(const ApgPackageDocument::Json &parent, const char *key, const std::string &label) {
    if (!parent.contains(key) || !parent.at(key).is_object())
        fail(label + " must be an object.");
    return parent.at(key);
}

const ApgPackageDocument::Json &
required_array(const ApgPackageDocument::Json &parent, const char *key, const std::string &label) {
    if (!parent.contains(key) || !parent.at(key).is_array())
        fail(label + " must be an array.");
    return parent.at(key);
}

std::string required_string(const ApgPackageDocument::Json &parent, const char *key, const std::string &label) {
    if (!parent.contains(key) || !nonempty_string(parent.at(key)))
        fail(label + " must be a non-empty string.");
    return parent.at(key).get<std::string>();
}

std::optional<std::string> normalized_workspace_path(const std::string &path) {
    if (path.empty() || path.front() == '/' || path.find('\\') != std::string::npos ||
        path.find(':') != std::string::npos)
        return std::nullopt;

    std::vector<std::string> segments;
    std::size_t              cursor = 0;
    while (cursor <= path.size()) {
        const auto end     = path.find('/', cursor);
        const auto segment = path.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor);
        if (segment.empty() || segment == ".") {
            // Canonical workspace paths may not need normalization.
        } else if (segment == "..") {
            if (segments.empty())
                return std::nullopt;
            segments.pop_back();
        } else {
            segments.push_back(segment);
        }
        if (end == std::string::npos)
            break;
        cursor = end + 1;
    }

    if (segments.empty())
        return std::nullopt;
    std::ostringstream result;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        if (i > 0)
            result << '/';
        result << segments[i];
    }
    return result.str();
}

std::string resolve_workspace_path(const std::string &base_file, const std::string &reference) {
    if (reference.empty() || reference.front() == '/' || reference.find('\\') != std::string::npos ||
        reference.find(':') != std::string::npos)
        fail("Workspace reference \"" + reference + "\" is not a confined relative path.");

    std::vector<std::string> segments;
    std::size_t              cursor = 0;
    while (cursor <= base_file.size()) {
        const auto end     = base_file.find('/', cursor);
        const auto segment = base_file.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor);
        if (!segment.empty())
            segments.push_back(segment);
        if (end == std::string::npos)
            break;
        cursor = end + 1;
    }
    if (!segments.empty())
        segments.pop_back();

    cursor = 0;
    while (cursor <= reference.size()) {
        const auto end     = reference.find('/', cursor);
        const auto segment = reference.substr(cursor, end == std::string::npos ? std::string::npos : end - cursor);
        if (segment.empty() || segment == ".") {
            // Skip.
        } else if (segment == "..") {
            if (segments.empty())
                fail("Workspace reference \"" + reference + "\" escapes its root.");
            segments.pop_back();
        } else {
            segments.push_back(segment);
        }
        if (end == std::string::npos)
            break;
        cursor = end + 1;
    }
    if (segments.empty())
        fail("Workspace reference \"" + reference + "\" resolves to an empty path.");

    std::ostringstream result;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        if (i > 0)
            result << '/';
        result << segments[i];
    }
    return result.str();
}

std::string scalar(const YAML::Node &node, const char *key, const std::string &fallback = {}) {
    if (!node || !node.IsMap())
        return fallback;
    const auto value = node[key];
    return value && value.IsScalar() ? value.Scalar() : fallback;
}

double scalar_number(const YAML::Node &node, const char *key, double fallback) {
    const auto value = node && node.IsMap() ? node[key] : YAML::Node{};
    if (!value || !value.IsScalar())
        return fallback;
    try {
        const double result = value.as<double>();
        return std::isfinite(result) ? result : fallback;
    } catch (const YAML::Exception &) { return fallback; }
}

bool scalar_bool(const YAML::Node &node, const char *key, bool fallback = false) {
    const auto value = node && node.IsMap() ? node[key] : YAML::Node{};
    if (!value || !value.IsScalar())
        return fallback;
    try {
        return value.as<bool>();
    } catch (const YAML::Exception &) { return fallback; }
}

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

struct Endpoint {
    std::string instance;
    std::string port;
};

constexpr const char *kDefaultPathPanner2Yaml = R"(kind: apg.unit
schema: apg.unit.v2
name: path_panner_2
version: 2.0.0

meta:
  title: Pan 2
  category: routing
  description: Splits one mono signal into two independently levelled paths.

routing:
  role: panner
  paths:
    - port: path_1
      level_param: path_1_db
    - port: path_2
      level_param: path_2_db

params:
  path_1_db:
    type: float
    default: 0.0
    min: -60.0
    max: 6.0
    smoothing_ms: 10
    ui:
      label: Path 1
      control: knob
      unit: dB
      scale: linear
      display_precision: 1
  path_2_db:
    type: float
    default: 0.0
    min: -60.0
    max: 6.0
    smoothing_ms: 10
    ui:
      label: Path 2
      control: knob
      unit: dB
      scale: linear
      display_precision: 1

ports:
  inputs:
    - name: input
      type: audio
      channels: 1
  outputs:
    - name: path_1
      type: audio
      channels: 1
    - name: path_2
      type: audio
      channels: 1

graph:
  signals:
    - input
    - path_1
    - path_2
  nodes:
    - id: level_path_1
      atom: amplitude_gain_db
      in:
        signal: input
      out:
        signal: path_1
      config:
        gain_db: ${params.path_1_db}
    - id: level_path_2
      atom: amplitude_gain_db
      in:
        signal: input
      out:
        signal: path_2
      config:
        gain_db: ${params.path_2_db}

compatibility:
  desktop_full: true
  wasm_realtime: true
  m7_static: true
  offline_render: true
)";

constexpr const char *kDefaultPathMixer2Yaml = R"(kind: apg.unit
schema: apg.unit.v2
name: path_mixer_2
version: 2.0.0

meta:
  title: Mix 2
  category: routing
  description: Sums two mono paths with an independent level for each input.

routing:
  role: mixer
  paths:
    - port: path_1
      level_param: path_1_db
    - port: path_2
      level_param: path_2_db

params:
  path_1_db:
    type: float
    default: -6.0206
    min: -60.0
    max: 6.0
    smoothing_ms: 10
    ui:
      label: Path 1
      control: knob
      unit: dB
      scale: linear
      display_precision: 1
  path_2_db:
    type: float
    default: -6.0206
    min: -60.0
    max: 6.0
    smoothing_ms: 10
    ui:
      label: Path 2
      control: knob
      unit: dB
      scale: linear
      display_precision: 1

ports:
  inputs:
    - name: path_1
      type: audio
      channels: 1
    - name: path_2
      type: audio
      channels: 1
  outputs:
    - name: output
      type: audio
      channels: 1

graph:
  signals:
    - path_1
    - path_2
    - path_1_scaled
    - path_2_scaled
    - output
  nodes:
    - id: level_path_1
      atom: amplitude_gain_db
      in:
        signal: path_1
      out:
        signal: path_1_scaled
      config:
        gain_db: ${params.path_1_db}
    - id: level_path_2
      atom: amplitude_gain_db
      in:
        signal: path_2
      out:
        signal: path_2_scaled
      config:
        gain_db: ${params.path_2_db}
    - id: sum_paths
      atom: amplitude_add
      in:
        signal_a: path_1_scaled
        signal_b: path_2_scaled
      out:
        signal: output

compatibility:
  desktop_full: true
  wasm_realtime: true
  m7_static: true
  offline_render: true
)";

void ensure_panner_mixer_pair(std::vector<UnitReference> &units) {
    bool has_panner = false;
    bool has_mixer  = false;
    for (const auto &unit : units) {
        if (unit.routing.role == RoutingRole::Panner && unit.routing.paths.size() == 2)
            has_panner = true;
        if (unit.routing.role == RoutingRole::Mixer && unit.routing.paths.size() == 2)
            has_mixer = true;
    }
    if (!has_panner) {
        UnitReference panner;
        panner.id            = "path_panner_2";
        panner.file          = "../units-v2/path_panner_2.unit.v2.yaml";
        panner.name          = "path_panner_2";
        panner.title         = "Pan 2";
        panner.category      = "routing";
        panner.routing.role  = RoutingRole::Panner;
        panner.routing.paths = {{"path_1", "path_1_db"}, {"path_2", "path_2_db"}};
        panner.inputs        = {"input"};
        panner.outputs       = {"path_1", "path_2"};
        Parameter p1;
        p1.name          = "path_1_db";
        p1.label         = "Path 1";
        p1.unit          = "dB";
        p1.control       = "knob";
        p1.type          = ParameterType::Float;
        p1.scale         = ParameterScale::Linear;
        p1.value         = 0.0;
        p1.default_value = 0.0;
        p1.min           = -60.0;
        p1.max           = 6.0;
        p1.precision     = 1;
        Parameter p2     = p1;
        p2.name          = "path_2_db";
        p2.label         = "Path 2";
        panner.parameters = {p1, p2};
        units.push_back(std::move(panner));
    }
    if (!has_mixer) {
        UnitReference mixer;
        mixer.id            = "path_mixer_2";
        mixer.file          = "../units-v2/path_mixer_2.unit.v2.yaml";
        mixer.name          = "path_mixer_2";
        mixer.title         = "Mix 2";
        mixer.category      = "routing";
        mixer.routing.role  = RoutingRole::Mixer;
        mixer.routing.paths = {{"path_1", "path_1_db"}, {"path_2", "path_2_db"}};
        mixer.inputs        = {"path_1", "path_2"};
        mixer.outputs       = {"output"};
        Parameter m1;
        m1.name          = "path_1_db";
        m1.label         = "Path 1";
        m1.unit          = "dB";
        m1.control       = "knob";
        m1.type          = ParameterType::Float;
        m1.scale         = ParameterScale::Linear;
        m1.value         = -6.0206;
        m1.default_value = -6.0206;
        m1.min           = -60.0;
        m1.max           = 6.0;
        m1.precision     = 1;
        Parameter m2     = m1;
        m2.name          = "path_2_db";
        m2.label         = "Path 2";
        mixer.parameters = {m1, m2};
        units.push_back(std::move(mixer));
    }
}

Endpoint parse_endpoint(const std::string &value) {
    const auto dot = value.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= value.size())
        fail("Invalid project endpoint \"" + value + "\".");
    return {value.substr(0, dot), value.substr(dot + 1)};
}

std::string emit_yaml(const YAML::Node &root) {
    YAML::Emitter emitter;
    emitter.SetIndent(2);
    emitter << root;
    if (!emitter.good())
        fail("Unable to serialize project YAML.");
    return std::string(emitter.c_str()) + "\n";
}

std::vector<Route> yaml_routes(const YAML::Node &root) {
    std::vector<Route> result;
    const auto         routes = root["chain"]["routes"];
    if (!routes || !routes.IsSequence())
        return result;
    result.reserve(routes.size());
    for (const auto &item : routes)
        result.push_back({scalar(item, "from"), scalar(item, "to")});
    return result;
}

void set_yaml_routes(YAML::Node &root, const std::vector<Route> &routes) {
    YAML::Node sequence(YAML::NodeType::Sequence);
    for (const auto &route : routes) {
        YAML::Node item(YAML::NodeType::Map);
        item["from"] = route.from;
        item["to"]   = route.to;
        sequence.push_back(item);
    }
    if (sequence.size() == 0)
        sequence.SetStyle(YAML::EmitterStyle::Flow);
    root["chain"]["routes"] = sequence;
}

std::vector<YAML::Node> yaml_nodes(const YAML::Node &root) {
    std::vector<YAML::Node> result;
    const auto              nodes = root["chain"]["nodes"];
    if (!nodes || !nodes.IsSequence())
        return result;
    result.reserve(nodes.size());
    for (const auto &node : nodes)
        result.push_back(node);
    return result;
}

void set_yaml_nodes(YAML::Node &root, const std::vector<YAML::Node> &nodes) {
    YAML::Node sequence(YAML::NodeType::Sequence);
    for (const auto &node : nodes)
        sequence.push_back(node);
    if (sequence.size() == 0)
        sequence.SetStyle(YAML::EmitterStyle::Flow);
    root["chain"]["nodes"] = sequence;
}

std::string format_number(double value, ParameterType type) {
    if (type == ParameterType::Integer)
        return std::to_string(static_cast<long long>(std::llround(value)));
    std::ostringstream output;
    output << std::setprecision(9) << std::defaultfloat << value;
    return output.str();
}

std::string sanitize_identifier(std::string value) {
    if (value.size() > 5 && value.ends_with("_unit"))
        value.resize(value.size() - 5);
    std::string result;
    for (char character : value) {
        const auto lower = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        if ((lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9') || lower == '_')
            result.push_back(lower);
        else if (!result.empty() && result.back() != '_')
            result.push_back('_');
    }
    while (!result.empty() && result.back() == '_')
        result.pop_back();
    if (result.empty() || result.front() < 'a' || result.front() > 'z')
        result = "effect";
    return result;
}

std::string random_suffix() {
    static std::atomic<std::uint64_t> counter{0};
    std::random_device                random;
    std::ostringstream                value;
    value << std::hex << random() << counter.fetch_add(1, std::memory_order_relaxed);
    return value.str();
}

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        const auto parent = std::filesystem::temp_directory_path();
        for (int attempt = 0; attempt < 32; ++attempt) {
            path_ = parent / ("apg-tui-" + random_suffix());
            std::error_code error;
            if (std::filesystem::create_directory(path_, error)) {
                std::filesystem::permissions(
                    path_, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace, error
                );
                if (!error)
                    return;
                std::filesystem::remove_all(path_, error);
            }
        }
        fail("Unable to create a private temporary APG workspace.");
    }

    TemporaryDirectory(const TemporaryDirectory &)            = delete;
    TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    [[nodiscard]] const std::filesystem::path &path() const { return path_; }

  private:
    std::filesystem::path path_;
};

std::string utc_timestamp() {
    const auto        now = std::chrono::system_clock::now();
    const auto        ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    std::tm           utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &seconds);
#else
    gmtime_r(&seconds, &utc);
#endif
    std::ostringstream value;
    value << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << ms << 'Z';
    return value.str();
}

void remove_scene_references(YAML::Node &root, const std::string &node_id) {
    auto scenes = root["scenes"];
    if (!scenes || !scenes.IsSequence())
        return;
    for (auto scene : scenes) {
        auto params = scene["params"];
        if (params && params.IsMap()) {
            std::vector<std::string> removals;
            for (const auto &entry : params) {
                const auto key = entry.first.Scalar();
                if (key.starts_with(node_id + "."))
                    removals.push_back(key);
            }
            for (const auto &key : removals)
                params.remove(key);
            if (params.size() == 0)
                params.SetStyle(YAML::EmitterStyle::Flow);
        }
        auto bypass = scene["bypass"];
        if (bypass && bypass.IsMap()) {
            bypass.remove(node_id);
            if (bypass.size() == 0)
                bypass.SetStyle(YAML::EmitterStyle::Flow);
        }
    }
}

YAML::Node make_project_node(
    const UnitReference &unit, const std::string &id, const std::optional<std::string> &routing_section = std::nullopt
) {
    YAML::Node item(YAML::NodeType::Map);
    item["id"]   = id;
    item["unit"] = unit.id;
    if (routing_section)
        item["routing"]["section"] = *routing_section;
    YAML::Node params(YAML::NodeType::Map);
    for (const auto &parameter : unit.parameters)
        params[parameter.name] = parameter.default_text.empty() ? format_number(parameter.default_value, parameter.type)
                                                                : parameter.default_text;
    if (params.size() == 0)
        params.SetStyle(YAML::EmitterStyle::Flow);
    item["params"] = params;
    return item;
}

bool routing_contracts_match(const UnitReference &panner, const UnitReference &mixer) {
    if (panner.routing.role != RoutingRole::Panner || mixer.routing.role != RoutingRole::Mixer ||
        panner.routing.paths.size() != mixer.routing.paths.size())
        return false;
    for (std::size_t index = 0; index < panner.routing.paths.size(); ++index) {
        if (panner.routing.paths[index].port != mixer.routing.paths[index].port ||
            panner.routing.paths[index].level_param != mixer.routing.paths[index].level_param)
            return false;
    }
    return true;
}

std::size_t find_route(const std::vector<Route> &routes, const Route &route) {
    const auto found = std::find(routes.begin(), routes.end(), route);
    if (found == routes.end())
        fail("Selected route \"" + route.from + " -> " + route.to + "\" no longer exists.");
    return static_cast<std::size_t>(std::distance(routes.begin(), found));
}

std::size_t find_single_route_to(const std::vector<Route> &routes, const std::string &target) {
    std::optional<std::size_t> result;
    for (std::size_t index = 0; index < routes.size(); ++index) {
        if (routes[index].to != target)
            continue;
        if (result)
            fail("Project endpoint \"" + target + "\" has multiple incoming routes.");
        result = index;
    }
    if (!result)
        fail("Project endpoint \"" + target + "\" has no incoming route.");
    return *result;
}

std::size_t find_single_route_from(const std::vector<Route> &routes, const std::string &source) {
    std::optional<std::size_t> result;
    for (std::size_t index = 0; index < routes.size(); ++index) {
        if (routes[index].from != source)
            continue;
        if (result)
            fail("Project endpoint \"" + source + "\" has multiple outgoing routes.");
        result = index;
    }
    if (!result)
        fail("Project endpoint \"" + source + "\" has no outgoing route.");
    return *result;
}

} // namespace

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

const Node *ApgPackageDocument::find_node(const std::string &id) const {
    const auto found = std::find_if(nodes_.begin(), nodes_.end(), [&](const Node &node) { return node.id == id; });
    return found == nodes_.end() ? nullptr : &*found;
}

const UnitReference *ApgPackageDocument::find_unit(const std::string &id) const {
    const auto found =
        std::find_if(units_.begin(), units_.end(), [&](const UnitReference &unit) { return unit.id == id; });
    return found == units_.end() ? nullptr : &*found;
}

const Scene *ApgPackageDocument::find_scene(const std::string &name) const {
    const auto found =
        std::find_if(scenes_.begin(), scenes_.end(), [&](const Scene &scene) { return scene.name == name; });
    return found == scenes_.end() ? nullptr : &*found;
}

std::vector<WorkspaceFile> ApgPackageDocument::workspace_files() const {
    std::vector<WorkspaceFile> result;
    const auto                &files = (*package_)["workspace"]["files"];
    result.reserve(files.size() + 2);
    bool has_panner = false;
    bool has_mixer  = false;
    for (std::size_t index = 0; index < files.size(); ++index) {
        const auto path = files[index]["path"].get<std::string>();
        if (path.find("path_panner_2") != std::string::npos)
            has_panner = true;
        if (path.find("path_mixer_2") != std::string::npos)
            has_mixer = true;
        result.push_back({
            path,
            files[index]["role"].get<std::string>(),
            index == entry_file_index_ ? project_content_ : files[index]["content"].get<std::string>(),
        });
    }
    if (!has_panner) {
        const auto panner_resolved = resolve_workspace_path(entry_project_, "../units-v2/path_panner_2.unit.v2.yaml");
        result.push_back({panner_resolved, "unit", kDefaultPathPanner2Yaml});
        if (panner_resolved != "units-v2/path_panner_2.unit.v2.yaml") {
            result.push_back({"units-v2/path_panner_2.unit.v2.yaml", "unit", kDefaultPathPanner2Yaml});
        }
    }
    if (!has_mixer) {
        const auto mixer_resolved = resolve_workspace_path(entry_project_, "../units-v2/path_mixer_2.unit.v2.yaml");
        result.push_back({mixer_resolved, "unit", kDefaultPathMixer2Yaml});
        if (mixer_resolved != "units-v2/path_mixer_2.unit.v2.yaml") {
            result.push_back({"units-v2/path_mixer_2.unit.v2.yaml", "unit", kDefaultPathMixer2Yaml});
        }
    }
    return result;
}

std::filesystem::path ApgPackageDocument::materialize_to(const std::filesystem::path &directory) const {
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error)
        throw std::system_error(error, "Unable to create APG workspace");
    for (const auto &file : workspace_files()) {
        const auto destination = directory / std::filesystem::path(file.path);
        std::filesystem::create_directories(destination.parent_path(), error);
        if (error)
            throw std::system_error(error, "Unable to create APG workspace directory");
        std::ofstream output(destination, std::ios::binary | std::ios::trunc);
        if (!output)
            fail("Unable to materialize workspace file \"" + file.path + "\".");
        output.write(file.content.data(), static_cast<std::streamsize>(file.content.size()));
        if (!output)
            fail("Unable to write materialized workspace file \"" + file.path + "\".");
    }
    return directory / std::filesystem::path(entry_project_);
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

std::string ApgPackageDocument::unique_node_id(const std::string &base) const {
    const std::string     candidate_base = sanitize_identifier(base);
    std::set<std::string> existing;
    for (const auto &node : nodes_)
        existing.insert(node.id);
    if (!existing.contains(candidate_base))
        return candidate_base;
    for (std::size_t suffix = 2;; ++suffix) {
        const auto candidate = candidate_base + "_" + std::to_string(suffix);
        if (!existing.contains(candidate))
            return candidate;
    }
}

std::string ApgPackageDocument::unique_section_id() const {
    std::set<std::string> existing;
    for (const auto &node : nodes_) {
        if (!node.routing_section.empty())
            existing.insert(node.routing_section);
    }
    for (std::size_t suffix = 1;; ++suffix) {
        const auto candidate = "parallel_" + std::to_string(suffix);
        if (!existing.contains(candidate))
            return candidate;
    }
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

std::string ApgPackageDocument::insert_on_route(const Route &route, const std::string &unit_id) {
    const auto *unit = find_unit(unit_id);
    if (!unit)
        fail("Project unit \"" + unit_id + "\" was not found.");
    if (!unit->user_placeable())
        fail("Only ordinary one-input/one-output effects can be inserted on a route.");

    YAML::Node root   = YAML::Load(project_content_);
    auto       routes = yaml_routes(root);
    const auto index  = find_route(routes, route);
    const auto id     = unique_node_id(unit->name.empty() ? unit->id : unit->name);
    auto       nodes  = yaml_nodes(root);
    nodes.push_back(make_project_node(*unit, id));
    set_yaml_nodes(root, nodes);

    routes.erase(routes.begin() + static_cast<std::ptrdiff_t>(index));
    routes.insert(
        routes.begin() + static_cast<std::ptrdiff_t>(index), {
                                                                 route.from,
                                                                 id + "." + unit->inputs.front(),
                                                             }
    );
    routes.insert(
        routes.begin() + static_cast<std::ptrdiff_t>(index + 1), {
                                                                     id + "." + unit->outputs.front(),
                                                                     route.to,
                                                                 }
    );
    set_yaml_routes(root, routes);
    replace_project_content(emit_yaml(root));
    return id;
}

void ApgPackageDocument::move_to_route(const std::string &node_id, const Route &route) {
    const auto *node = find_node(node_id);
    if (!node)
        fail("Project instance \"" + node_id + "\" was not found.");
    if (node->routing_helper())
        fail("Routing helpers cannot be moved independently.");
    const auto *unit = find_unit(node->unit);
    if (!unit || !unit->user_placeable())
        fail("Only ordinary one-input/one-output effects can be moved.");

    auto routes = routes_;
    (void)find_route(routes, route);
    const std::string input          = node_id + "." + unit->inputs.front();
    const std::string output         = node_id + "." + unit->outputs.front();
    const auto        incoming_index = find_single_route_to(routes, input);
    const auto        outgoing_index = find_single_route_from(routes, output);
    if (routes[incoming_index] == route || routes[outgoing_index] == route)
        fail("Choose a route outside the effect being moved.");

    const Route bridge{routes[incoming_index].from, routes[outgoing_index].to};
    const auto  first  = std::min(incoming_index, outgoing_index);
    const auto  second = std::max(incoming_index, outgoing_index);
    routes.erase(routes.begin() + static_cast<std::ptrdiff_t>(second));
    routes.erase(routes.begin() + static_cast<std::ptrdiff_t>(first));
    routes.insert(routes.begin() + static_cast<std::ptrdiff_t>(first), bridge);

    const auto destination = find_route(routes, route);
    routes.erase(routes.begin() + static_cast<std::ptrdiff_t>(destination));
    routes.insert(
        routes.begin() + static_cast<std::ptrdiff_t>(destination), {
                                                                       route.from,
                                                                       input,
                                                                   }
    );
    routes.insert(
        routes.begin() + static_cast<std::ptrdiff_t>(destination + 1), {
                                                                           output,
                                                                           route.to,
                                                                       }
    );

    YAML::Node root = YAML::Load(project_content_);
    set_yaml_routes(root, routes);
    replace_project_content(emit_yaml(root));
}

void ApgPackageDocument::remove_node(const std::string &node_id) {
    const auto *node = find_node(node_id);
    if (!node)
        fail("Project instance \"" + node_id + "\" was not found.");
    if (node->routing_helper())
        fail("Routing helpers can only be removed by collapsing their complete section.");
    const auto *unit = find_unit(node->unit);
    if (!unit || !unit->user_placeable())
        fail("Only ordinary one-input/one-output effects can be removed directly.");

    auto        routes   = routes_;
    const auto  incoming = find_single_route_to(routes, node_id + "." + unit->inputs.front());
    const auto  outgoing = find_single_route_from(routes, node_id + "." + unit->outputs.front());
    const Route bridge{routes[incoming].from, routes[outgoing].to};
    const auto  first  = std::min(incoming, outgoing);
    const auto  second = std::max(incoming, outgoing);
    routes.erase(routes.begin() + static_cast<std::ptrdiff_t>(second));
    routes.erase(routes.begin() + static_cast<std::ptrdiff_t>(first));
    routes.insert(routes.begin() + static_cast<std::ptrdiff_t>(first), bridge);

    YAML::Node root  = YAML::Load(project_content_);
    auto       nodes = yaml_nodes(root);
    nodes.erase(
        std::remove_if(
            nodes.begin(), nodes.end(), [&](const YAML::Node &item) { return scalar(item, "id") == node_id; }
        ),
        nodes.end()
    );
    set_yaml_nodes(root, nodes);
    set_yaml_routes(root, routes);
    remove_scene_references(root, node_id);
    replace_project_content(emit_yaml(root));
}

void ensure_yaml_units_contain(YAML::Node &root, const UnitReference &unit) {
    auto units = root["units"];
    if (!units || !units.IsSequence())
        return;
    for (const auto &item : units) {
        if (scalar(item, "id") == unit.id)
            return;
    }
    YAML::Node entry(YAML::NodeType::Map);
    entry["id"]   = unit.id;
    entry["file"] = unit.file.empty() ? ("units/" + unit.id + ".unit.v2.yaml") : unit.file;
    units.push_back(entry);
}

std::string ApgPackageDocument::add_parallel_on_route(const Route &route, const std::string &effect_unit_id) {
    ensure_panner_mixer_pair(units_);
    const UnitReference *effect = nullptr;
    if (!effect_unit_id.empty()) {
        effect = find_unit(effect_unit_id);
        if (!effect || !effect->user_placeable())
            fail("Parallel effects must expose exactly one audio input and one audio output.");
    }
    (void)find_route(routes_, route);

    const UnitReference *panner = nullptr;
    const UnitReference *mixer  = nullptr;
    for (const auto &candidate_panner : units_) {
        if (candidate_panner.routing.role != RoutingRole::Panner || candidate_panner.routing.paths.size() != 2 ||
            candidate_panner.inputs.size() != 1 || candidate_panner.outputs.size() != 2)
            continue;
        for (const auto &candidate_mixer : units_) {
            if (candidate_mixer.routing.role != RoutingRole::Mixer || candidate_mixer.inputs.size() != 2 ||
                candidate_mixer.outputs.size() != 1 || !routing_contracts_match(candidate_panner, candidate_mixer))
                continue;
            if (!panner || std::pair{candidate_panner.id, candidate_mixer.id} < std::pair{panner->id, mixer->id}) {
                panner = &candidate_panner;
                mixer  = &candidate_mixer;
            }
        }
    }
    if (!panner || !mixer)
        fail("The package does not contain a compatible two-path panner/mixer pair.");

    const auto            section = unique_section_id();
    std::set<std::string> reserved_ids;
    for (const auto &node : nodes_)
        reserved_ids.insert(node.id);
    const auto reserve_id = [&](const std::string &base) {
        const auto candidate_base = sanitize_identifier(base);
        if (reserved_ids.insert(candidate_base).second)
            return candidate_base;
        for (std::size_t suffix = 2;; ++suffix) {
            const auto candidate = candidate_base + "_" + std::to_string(suffix);
            if (reserved_ids.insert(candidate).second)
                return candidate;
        }
    };
    const auto panner_id = reserve_id(section + "_pan");
    const auto mixer_id  = reserve_id(section + "_mix");
    const auto effect_id = effect ? reserve_id(effect->name.empty() ? effect->id : effect->name) : std::string();

    YAML::Node root  = YAML::Load(project_content_);
    ensure_yaml_units_contain(root, *panner);
    if (effect)
        ensure_yaml_units_contain(root, *effect);
    ensure_yaml_units_contain(root, *mixer);
    auto       nodes = yaml_nodes(root);
    nodes.push_back(make_project_node(*panner, panner_id, section));
    if (effect)
        nodes.push_back(make_project_node(*effect, effect_id));
    nodes.push_back(make_project_node(*mixer, mixer_id, section));
    set_yaml_nodes(root, nodes);

    auto       routes = yaml_routes(root);
    const auto index  = find_route(routes, route);
    routes.erase(routes.begin() + static_cast<std::ptrdiff_t>(index));
    const auto              &path_1 = panner->routing.paths[0].port;
    const auto              &path_2 = panner->routing.paths[1].port;
    std::vector<Route> replacement;
    if (effect) {
        replacement = {
            {                               route.from, panner_id + "." + panner->inputs.front()},
            {                 panner_id + "." + path_1,                  mixer_id + "." + path_1},
            {                 panner_id + "." + path_2, effect_id + "." + effect->inputs.front()},
            {effect_id + "." + effect->outputs.front(),                  mixer_id + "." + path_2},
            {  mixer_id + "." + mixer->outputs.front(),                                 route.to},
        };
    } else {
        replacement = {
            {                             route.from, panner_id + "." + panner->inputs.front()},
            {               panner_id + "." + path_1,                mixer_id + "." + path_1},
            {               panner_id + "." + path_2,                mixer_id + "." + path_2},
            {mixer_id + "." + mixer->outputs.front(),                               route.to},
        };
    }
    routes.insert(routes.begin() + static_cast<std::ptrdiff_t>(index), replacement.begin(), replacement.end());
    set_yaml_routes(root, routes);
    replace_project_content(emit_yaml(root));
    return effect_id.empty() ? panner_id : effect_id;
}

std::string ApgPackageDocument::wrap_node_in_parallel(const std::string &node_id) {
    ensure_panner_mixer_pair(units_);
    const auto *target_node = find_node(node_id);
    if (!target_node)
        fail("Node \"" + node_id + "\" not found.");
    if (target_node->routing_helper())
        fail("Cannot wrap a routing helper in a parallel section.");

    const auto *target_unit = find_unit(target_node->unit);
    if (!target_unit || target_unit->inputs.size() != 1 || target_unit->outputs.size() != 1)
        fail("Node \"" + node_id + "\" must expose one input and one output.");

    const auto target_input  = node_id + "." + target_unit->inputs.front();
    const auto target_output = node_id + "." + target_unit->outputs.front();

    const auto incoming_idx = find_single_route_to(routes_, target_input);
    const auto outgoing_idx = find_single_route_from(routes_, target_output);

    const auto incoming_route = routes_[incoming_idx];
    const auto outgoing_route = routes_[outgoing_idx];

    const UnitReference *panner = nullptr;
    const UnitReference *mixer  = nullptr;
    for (const auto &candidate_panner : units_) {
        if (candidate_panner.routing.role != RoutingRole::Panner || candidate_panner.routing.paths.size() != 2 ||
            candidate_panner.inputs.size() != 1 || candidate_panner.outputs.size() != 2)
            continue;
        for (const auto &candidate_mixer : units_) {
            if (candidate_mixer.routing.role != RoutingRole::Mixer || candidate_mixer.inputs.size() != 2 ||
                candidate_mixer.outputs.size() != 1 || !routing_contracts_match(candidate_panner, candidate_mixer))
                continue;
            if (!panner || std::pair{candidate_panner.id, candidate_mixer.id} < std::pair{panner->id, mixer->id}) {
                panner = &candidate_panner;
                mixer  = &candidate_mixer;
            }
        }
    }
    if (!panner || !mixer)
        fail("The package does not contain a compatible two-path panner/mixer pair.");

    const auto            section = unique_section_id();
    std::set<std::string> reserved_ids;
    for (const auto &node : nodes_)
        reserved_ids.insert(node.id);
    const auto reserve_id = [&](const std::string &base) {
        const auto candidate_base = sanitize_identifier(base);
        if (reserved_ids.insert(candidate_base).second)
            return candidate_base;
        for (std::size_t suffix = 2;; ++suffix) {
            const auto candidate = candidate_base + "_" + std::to_string(suffix);
            if (reserved_ids.insert(candidate).second)
                return candidate;
        }
    };
    const auto panner_id = reserve_id(section + "_pan");
    const auto mixer_id  = reserve_id(section + "_mix");

    YAML::Node root  = YAML::Load(project_content_);
    ensure_yaml_units_contain(root, *panner);
    ensure_yaml_units_contain(root, *mixer);
    auto       nodes = yaml_nodes(root);
    nodes.push_back(make_project_node(*panner, panner_id, section));
    nodes.push_back(make_project_node(*mixer, mixer_id, section));
    set_yaml_nodes(root, nodes);

    const auto &path_1 = panner->routing.paths[0].port;
    const auto &path_2 = panner->routing.paths[1].port;

    std::set<std::size_t> removals{incoming_idx, outgoing_idx};
    std::vector<Route> replacement{
        {         incoming_route.from, panner_id + "." + panner->inputs.front()},
        {    panner_id + "." + path_1,                             target_input},
        {               target_output,                 mixer_id + "." + path_1},
        {    panner_id + "." + path_2,                 mixer_id + "." + path_2},
        {mixer_id + "." + mixer->outputs.front(),          outgoing_route.to},
    };

    const auto insertion = *removals.begin();
    std::vector<Route> next_routes;
    next_routes.reserve(routes_.size() - removals.size() + replacement.size());
    for (std::size_t index = 0; index < routes_.size(); ++index) {
        if (index == insertion) {
            for (const auto &r : replacement)
                next_routes.push_back(r);
        }
        if (!removals.contains(index))
            next_routes.push_back(routes_[index]);
    }
    set_yaml_routes(root, next_routes);
    replace_project_content(emit_yaml(root));
    return panner_id;
}

void ApgPackageDocument::collapse_parallel(const std::string &section) {
    const Node *panner_node = nullptr;
    const Node *mixer_node  = nullptr;
    for (const auto &node : nodes_) {
        if (node.routing_section != section)
            continue;
        const auto *unit = find_unit(node.unit);
        if (!unit)
            continue;
        if (unit->routing.role == RoutingRole::Panner) {
            if (panner_node)
                fail("Routing section \"" + section + "\" has multiple panners.");
            panner_node = &node;
        } else if (unit->routing.role == RoutingRole::Mixer) {
            if (mixer_node)
                fail("Routing section \"" + section + "\" has multiple mixers.");
            mixer_node = &node;
        }
    }
    if (!panner_node || !mixer_node)
        fail("Routing section \"" + section + "\" is incomplete.");
    const auto *panner = find_unit(panner_node->unit);
    const auto *mixer  = find_unit(mixer_node->unit);
    if (!panner || !mixer || !routing_contracts_match(*panner, *mixer))
        fail("Routing section \"" + section + "\" has incompatible helpers.");

    auto                  routes   = routes_;
    const auto            incoming = find_single_route_to(routes, panner_node->id + "." + panner->inputs.front());
    const auto            outgoing = find_single_route_from(routes, mixer_node->id + "." + mixer->outputs.front());
    std::set<std::size_t> removals{incoming, outgoing};
    for (const auto &path : panner->routing.paths) {
        const Route direct{panner_node->id + "." + path.port, mixer_node->id + "." + path.port};
        removals.insert(find_route(routes, direct));
    }
    if (removals.size() != panner->routing.paths.size() + 2)
        fail("Routing section \"" + section + "\" cannot be collapsed.");

    const Route        bridge{routes[incoming].from, routes[outgoing].to};
    const auto         insertion = *removals.begin();
    std::vector<Route> next;
    next.reserve(routes.size() - removals.size() + 1);
    for (std::size_t index = 0; index < routes.size(); ++index) {
        if (index == insertion)
            next.push_back(bridge);
        if (!removals.contains(index))
            next.push_back(routes[index]);
    }

    YAML::Node root  = YAML::Load(project_content_);
    auto       nodes = yaml_nodes(root);
    nodes.erase(
        std::remove_if(
            nodes.begin(), nodes.end(),
            [&](const YAML::Node &item) {
                const auto id = scalar(item, "id");
                return id == panner_node->id || id == mixer_node->id;
            }
        ),
        nodes.end()
    );
    set_yaml_nodes(root, nodes);
    set_yaml_routes(root, next);
    remove_scene_references(root, panner_node->id);
    remove_scene_references(root, mixer_node->id);
    replace_project_content(emit_yaml(root));
}

void ApgPackageDocument::set_param(const std::string &node_id, const std::string &parameter_name, double value) {
    const auto *node = find_node(node_id);
    if (!node)
        fail("Project instance \"" + node_id + "\" was not found.");
    const auto spec =
        std::find_if(node->parameter_specs.begin(), node->parameter_specs.end(), [&](const Parameter &parameter) {
            return parameter.name == parameter_name;
        });
    if (spec == node->parameter_specs.end())
        fail("Project parameter \"" + node_id + "." + parameter_name + "\" was not found.");
    if (!std::isfinite(value))
        fail("Parameter values must be finite.");
    value = std::clamp(value, spec->min, spec->max);
    if (spec->type == ParameterType::Integer)
        value = std::round(value);
    if (value == spec->value)
        return;

    YAML::Node root  = YAML::Load(project_content_);
    bool       found = false;
    for (auto item : root["chain"]["nodes"]) {
        if (scalar(item, "id") != node_id)
            continue;
        item["params"][parameter_name] = format_number(value, spec->type);
        found                          = true;
        break;
    }
    if (!found)
        fail("Project instance \"" + node_id + "\" disappeared during the edit.");
    replace_project_content(emit_yaml(root));
}

void ApgPackageDocument::upsert_scene(
    const std::string &raw_name, const std::map<std::string, bool> &live_bypass, bool allow_overwrite
) {
    const auto name = trim(raw_name);
    if (name.empty())
        fail("Scene name is required.");
    const bool exists = find_scene(name) != nullptr;
    if (exists && !allow_overwrite)
        fail("Scene \"" + name + "\" already exists.");

    YAML::Node root   = YAML::Load(project_content_);
    YAML::Node scenes = root["scenes"];
    if (!scenes || !scenes.IsSequence())
        scenes = YAML::Node(YAML::NodeType::Sequence);

    YAML::Node params(YAML::NodeType::Map);
    YAML::Node bypass(YAML::NodeType::Map);
    for (const auto &node : nodes_) {
        for (const auto &parameter : node.parameter_specs) {
            const auto current = node.params.find(parameter.name);
            params[node.id + "." + parameter.name] =
                current == node.params.end()
                    ? (parameter.default_text.empty() ? format_number(parameter.default_value, parameter.type)
                                                      : parameter.default_text)
                    : current->second;
        }
        if (!node.routing_helper()) {
            const auto current = live_bypass.find(node.id);
            bypass[node.id]    = current != live_bypass.end() && current->second;
        }
    }
    if (params.size() == 0)
        params.SetStyle(YAML::EmitterStyle::Flow);
    if (bypass.size() == 0)
        bypass.SetStyle(YAML::EmitterStyle::Flow);

    YAML::Node next(YAML::NodeType::Sequence);
    bool       replaced = false;
    for (std::size_t index = 0; index < scenes.size(); ++index) {
        YAML::Node scene = YAML::Clone(scenes[index]);
        if (scalar(scene, "name") == name) {
            scene["name"]   = name;
            scene["params"] = params;
            scene["bypass"] = bypass;
            replaced        = true;
        }
        next.push_back(scene);
    }
    if (!replaced) {
        YAML::Node scene(YAML::NodeType::Map);
        scene["name"]   = name;
        scene["params"] = params;
        scene["bypass"] = bypass;
        next.push_back(scene);
    }
    root["scenes"] = next;
    replace_project_content(emit_yaml(root));
}

void ApgPackageDocument::rename_scene(const std::string &name, const std::string &raw_next_name) {
    const auto next_name = trim(raw_next_name);
    if (next_name.empty())
        fail("Scene name is required.");
    if (name != next_name && find_scene(next_name))
        fail("Scene \"" + next_name + "\" already exists.");

    YAML::Node root  = YAML::Load(project_content_);
    bool       found = false;
    for (auto scene : root["scenes"]) {
        if (scalar(scene, "name") == name) {
            scene["name"] = next_name;
            found         = true;
            break;
        }
    }
    if (!found)
        fail("Scene \"" + name + "\" was not found.");
    replace_project_content(emit_yaml(root));
}

void ApgPackageDocument::remove_scene(const std::string &name) {
    YAML::Node root   = YAML::Load(project_content_);
    YAML::Node scenes = root["scenes"];
    if (!scenes || !scenes.IsSequence())
        fail("Scene \"" + name + "\" was not found.");
    YAML::Node next(YAML::NodeType::Sequence);
    bool       found = false;
    for (const auto &scene : scenes) {
        if (scalar(scene, "name") == name) {
            found = true;
            continue;
        }
        next.push_back(scene);
    }
    if (!found)
        fail("Scene \"" + name + "\" was not found.");
    if (next.size() == 0)
        next.SetStyle(YAML::EmitterStyle::Flow);
    root["scenes"] = next;
    replace_project_content(emit_yaml(root));
}

SceneRecall ApgPackageDocument::recall_scene(const std::string &name) {
    const auto *snapshot = find_scene(name);
    if (!snapshot)
        fail("Scene \"" + name + "\" was not found.");
    YAML::Node root = YAML::Load(project_content_);
    for (const auto &[path, value] : snapshot->params) {
        const auto  endpoint = parse_endpoint(path);
        const auto *node     = find_node(endpoint.instance);
        if (!node)
            fail("Scene parameter \"" + path + "\" references a missing instance.");
        const auto parameter =
            std::find_if(node->parameter_specs.begin(), node->parameter_specs.end(), [&](const Parameter &spec) {
                return spec.name == endpoint.port;
            });
        if (parameter == node->parameter_specs.end())
            fail("Scene parameter \"" + path + "\" does not exist.");
        bool updated = false;
        for (auto item : root["chain"]["nodes"]) {
            if (scalar(item, "id") == endpoint.instance) {
                item["params"][endpoint.port] = value;
                updated                       = true;
                break;
            }
        }
        if (!updated)
            fail("Scene parameter \"" + path + "\" references a missing instance.");
    }
    for (const auto &[instance, bypassed] : snapshot->bypass) {
        (void)bypassed;
        const auto *node = find_node(instance);
        if (!node)
            fail("Scene bypass references missing instance \"" + instance + "\".");
        if (node->routing_helper())
            fail("Routing helper \"" + instance + "\" is always active and cannot be bypassed.");
    }
    const auto bypass = snapshot->bypass;
    replace_project_content(emit_yaml(root));
    return {bypass};
}

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
