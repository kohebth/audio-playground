#ifndef APG_TERMINAL_PROJECT_DOCUMENT_HPP
#define APG_TERMINAL_PROJECT_DOCUMENT_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace apg::terminal {

struct Diagnostic {
    std::string code;
    std::string path;
    std::string message;
};

struct ValidationReport {
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool        ok() const { return diagnostics.empty(); }
    [[nodiscard]] std::string summary() const;
};

enum class ParameterType {
    Float,
    Integer,
};

enum class ParameterScale {
    Linear,
    Logarithmic,
};

struct Parameter {
    std::string    name;
    std::string    label;
    std::string    unit;
    std::string    control;
    std::string    default_text;
    ParameterType  type          = ParameterType::Float;
    ParameterScale scale         = ParameterScale::Linear;
    double         value         = 0.0;
    double         default_value = 0.0;
    double         min           = 0.0;
    double         max           = 1.0;
    int            precision     = 2;
};

struct RoutingPath {
    std::string port;
    std::string level_param;
};

enum class RoutingRole {
    None,
    Panner,
    Mixer,
};

struct RoutingContract {
    RoutingRole              role = RoutingRole::None;
    std::vector<RoutingPath> paths;
};

struct UnitReference {
    std::string                 id;
    std::string                 file;
    std::string                 name;
    std::string                 title;
    std::string                 category;
    std::vector<std::string>    inputs;
    std::vector<std::string>    outputs;
    std::vector<Parameter>      parameters;
    RoutingContract             routing;
    std::map<std::string, bool> compatibility;

    [[nodiscard]] bool user_placeable() const;
};

struct Node {
    std::string                        id;
    std::string                        unit;
    std::string                        routing_section;
    std::map<std::string, std::string> params;
    std::vector<Parameter>             parameter_specs;

    [[nodiscard]] bool routing_helper() const { return !routing_section.empty(); }
};

struct Route {
    std::string from;
    std::string to;

    [[nodiscard]] bool operator==(const Route &) const = default;
};

struct Scene {
    std::string                        name;
    std::map<std::string, std::string> params;
    std::map<std::string, bool>        bypass;
};

struct WorkspaceFile {
    std::string path;
    std::string role;
    std::string content;
};

struct TopologySequence;
struct ParallelTopology;

struct TopologyElement {
    enum class Kind {
        Effect,
        Parallel,
    };

    Kind                              kind = Kind::Effect;
    std::string                       node_id;
    std::shared_ptr<ParallelTopology> parallel;
};

struct TopologyPath {
    std::string                       name;
    std::shared_ptr<TopologySequence> sequence;
};

struct ParallelTopology {
    std::string               section;
    std::string               panner_id;
    std::string               mixer_id;
    std::vector<TopologyPath> paths;
};

struct TopologySequence {
    std::vector<TopologyElement> elements;
    std::vector<Route>           routes;
};

struct SceneRecall {
    std::map<std::string, bool> bypass;
};

class ApgPackageDocument {
  public:
    using Json = nlohmann::ordered_json;

    static ApgPackageDocument load(const std::filesystem::path &path);
    static ApgPackageDocument
    parse(const std::string &json, const std::filesystem::path &source_path = "memory.apg", bool validate_core = true);

    [[nodiscard]] const std::filesystem::path      &path() const { return path_; }
    [[nodiscard]] const std::string                &name() const { return name_; }
    [[nodiscard]] const std::string                &version() const { return version_; }
    [[nodiscard]] const std::string                &entry_project() const { return entry_project_; }
    [[nodiscard]] const std::string                &project_content() const { return project_content_; }
    [[nodiscard]] const std::vector<UnitReference> &units() const { return units_; }
    [[nodiscard]] const std::vector<Node>          &nodes() const { return nodes_; }
    [[nodiscard]] const std::vector<Route>         &routes() const { return routes_; }
    [[nodiscard]] const std::vector<Scene>         &scenes() const { return scenes_; }
    [[nodiscard]] const std::string                &default_target() const { return default_target_; }
    [[nodiscard]] const std::vector<std::string>   &export_targets() const { return export_targets_; }
    [[nodiscard]] bool                              dirty() const { return project_content_ != saved_project_content_; }

    [[nodiscard]] const Node                *find_node(const std::string &id) const;
    [[nodiscard]] const UnitReference       *find_unit(const std::string &id) const;
    [[nodiscard]] const Scene               *find_scene(const std::string &name) const;
    [[nodiscard]] std::vector<WorkspaceFile> workspace_files() const;
    [[nodiscard]] std::filesystem::path      materialize_to(const std::filesystem::path &directory) const;
    [[nodiscard]] ValidationReport
    validate_core(float sample_rate = 48000.0f, std::uint32_t maximum_frames = 1024) const;
    [[nodiscard]] TopologySequence topology() const;

    [[nodiscard]] std::string unique_node_id(const std::string &base) const;
    [[nodiscard]] std::string unique_section_id() const;

    std::string insert_on_route(const Route &route, const std::string &unit_id);
    void        move_to_route(const std::string &node_id, const Route &route);
    void        remove_node(const std::string &node_id);
    std::string add_parallel_on_route(const Route &route, const std::string &effect_unit_id);
    void        collapse_parallel(const std::string &section);
    void        set_param(const std::string &node_id, const std::string &parameter, double value);

    void
    upsert_scene(const std::string &name, const std::map<std::string, bool> &live_bypass, bool allow_overwrite = false);
    void        rename_scene(const std::string &name, const std::string &next_name);
    void        remove_scene(const std::string &name);
    SceneRecall recall_scene(const std::string &name);

    [[nodiscard]] std::string serialize_for_save(const std::string &timestamp) const;
    void                      save_atomic(const std::optional<std::string> &timestamp = std::nullopt);

  private:
    std::filesystem::path       path_;
    std::shared_ptr<const Json> package_;
    std::size_t                 entry_file_index_ = 0;
    std::string                 entry_project_;
    std::string                 project_content_;
    std::string                 saved_project_content_;
    std::string                 name_;
    std::string                 version_;
    std::vector<UnitReference>  units_;
    std::vector<Node>           nodes_;
    std::vector<Route>          routes_;
    std::vector<Scene>          scenes_;
    std::string                 default_target_;
    std::vector<std::string>    export_targets_;

    static ApgPackageDocument from_json(Json package, const std::filesystem::path &source_path, bool validate_core);
    void                      refresh_project_view();
    void                      replace_project_content(std::string content, bool validate = true);
};

} // namespace apg::terminal

#endif
