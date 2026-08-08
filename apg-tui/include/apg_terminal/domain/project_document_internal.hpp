#ifndef APG_TERMINAL_PROJECT_DOCUMENT_INTERNAL_HPP
#define APG_TERMINAL_PROJECT_DOCUMENT_INTERNAL_HPP

#include "apg_terminal/domain/project_document.hpp"

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
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace apg::terminal {

constexpr const char *kPackageSchema    = "apg.project.package.v1";
constexpr int         kPackageVersion   = 1;
constexpr const char *kWorkspaceSchema  = "apg.ui.workspace.v2";
constexpr int         kWorkspaceVersion = 2;

[[noreturn]] inline void fail(const std::string &message) {
    throw std::runtime_error(message);
}

inline bool nonempty_string(const ApgPackageDocument::Json &value) {
    if (!value.is_string())
        return false;
    const auto &text = value.get_ref<const std::string &>();
    return std::any_of(text.begin(), text.end(), [](unsigned char character) { return !std::isspace(character); });
}

inline const ApgPackageDocument::Json &
required_object(const ApgPackageDocument::Json &parent, const char *key, const std::string &label) {
    if (!parent.contains(key) || !parent.at(key).is_object())
        fail(label + " must be an object.");
    return parent.at(key);
}

inline const ApgPackageDocument::Json &
required_array(const ApgPackageDocument::Json &parent, const char *key, const std::string &label) {
    if (!parent.contains(key) || !parent.at(key).is_array())
        fail(label + " must be an array.");
    return parent.at(key);
}

inline std::string required_string(const ApgPackageDocument::Json &parent, const char *key, const std::string &label) {
    if (!parent.contains(key) || !nonempty_string(parent.at(key)))
        fail(label + " must be a non-empty string.");
    return parent.at(key).get<std::string>();
}

inline std::optional<std::string> normalized_workspace_path(const std::string &path) {
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

inline std::string resolve_workspace_path(const std::string &base_file, const std::string &reference) {
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

inline std::string scalar(const YAML::Node &node, const char *key, const std::string &fallback = {}) {
    if (!node || !node.IsMap())
        return fallback;
    const auto value = node[key];
    return value && value.IsScalar() ? value.Scalar() : fallback;
}

inline double scalar_number(const YAML::Node &node, const char *key, double fallback) {
    const auto value = node && node.IsMap() ? node[key] : YAML::Node{};
    if (!value || !value.IsScalar())
        return fallback;
    try {
        const double result = value.as<double>();
        return std::isfinite(result) ? result : fallback;
    } catch (const YAML::Exception &) { return fallback; }
}

inline bool scalar_bool(const YAML::Node &node, const char *key, bool fallback = false) {
    const auto value = node && node.IsMap() ? node[key] : YAML::Node{};
    if (!value || !value.IsScalar())
        return fallback;
    try {
        return value.as<bool>();
    } catch (const YAML::Exception &) { return fallback; }
}

inline std::string trim(std::string value) {
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

inline Endpoint parse_endpoint(const std::string &value) {
    const auto dot = value.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= value.size())
        fail("Invalid project endpoint \"" + value + "\".");
    return {value.substr(0, dot), value.substr(dot + 1)};
}

inline std::string emit_yaml(const YAML::Node &root) {
    YAML::Emitter emitter;
    emitter.SetIndent(2);
    emitter << root;
    if (!emitter.good())
        fail("Unable to serialize project YAML.");
    return std::string(emitter.c_str()) + "\n";
}

inline std::vector<Route> yaml_routes(const YAML::Node &root) {
    std::vector<Route> result;
    const auto         routes = root["chain"]["routes"];
    if (!routes || !routes.IsSequence())
        return result;
    result.reserve(routes.size());
    for (const auto &item : routes)
        result.push_back({scalar(item, "from"), scalar(item, "to")});
    return result;
}

inline void set_yaml_routes(YAML::Node &root, const std::vector<Route> &routes) {
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

inline std::vector<YAML::Node> yaml_nodes(const YAML::Node &root) {
    std::vector<YAML::Node> result;
    const auto              nodes = root["chain"]["nodes"];
    if (!nodes || !nodes.IsSequence())
        return result;
    result.reserve(nodes.size());
    for (const auto &node : nodes)
        result.push_back(node);
    return result;
}

inline void set_yaml_nodes(YAML::Node &root, const std::vector<YAML::Node> &nodes) {
    YAML::Node sequence(YAML::NodeType::Sequence);
    for (const auto &node : nodes)
        sequence.push_back(node);
    if (sequence.size() == 0)
        sequence.SetStyle(YAML::EmitterStyle::Flow);
    root["chain"]["nodes"] = sequence;
}

inline std::string format_number(double value, ParameterType type) {
    if (type == ParameterType::Integer)
        return std::to_string(static_cast<long long>(std::llround(value)));
    std::ostringstream output;
    output << std::setprecision(9) << std::defaultfloat << value;
    return output.str();
}

inline std::string sanitize_identifier(std::string value) {
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

inline std::string random_suffix() {
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

inline std::string utc_timestamp() {
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

inline void remove_scene_references(YAML::Node &root, const std::string &node_id) {
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

inline YAML::Node make_project_node(
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

inline bool routing_contracts_match(const UnitReference &panner, const UnitReference &mixer) {
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

inline std::size_t find_route(const std::vector<Route> &routes, const Route &route) {
    const auto found = std::find(routes.begin(), routes.end(), route);
    if (found == routes.end())
        fail("Selected route \"" + route.from + " -> " + route.to + "\" no longer exists.");
    return static_cast<std::size_t>(std::distance(routes.begin(), found));
}

inline std::size_t find_single_route_to(const std::vector<Route> &routes, const std::string &target) {
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

inline std::size_t find_single_route_from(const std::vector<Route> &routes, const std::string &source) {
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

inline void ensure_panner_mixer_pair(std::vector<UnitReference> &units) {
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

} // namespace apg::terminal

#endif
