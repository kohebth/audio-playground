#ifndef APG_TERMINAL_PROJECT_DOCUMENT_HPP
#define APG_TERMINAL_PROJECT_DOCUMENT_HPP

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace apg::terminal {

struct UnitReference {
    std::string id;
    std::string file;
};

struct Parameter {
    std::string name;
    std::string label;
    std::string unit;
    double      value = 0.0;
    double      min   = 0.0;
    double      max   = 1.0;
};

struct Node {
    std::string                   id;
    std::string                   unit;
    std::string                   routing_section;
    std::map<std::string, double> params;
    std::vector<Parameter>        parameter_specs;
};

struct Route {
    std::string from;
    std::string to;
};

class ProjectDocument {
  public:
    static ProjectDocument load(const std::filesystem::path &path);

    void                     save_atomic(const std::filesystem::path &path) const;
    std::vector<std::string> validate() const;

    const std::string                &name() const { return name_; }
    const std::string                &version() const { return version_; }
    const std::vector<UnitReference> &units() const { return units_; }
    const std::vector<Node>          &nodes() const { return nodes_; }
    std::vector<Node>                &nodes() { return nodes_; }
    const std::vector<Route>         &routes() const { return routes_; }

    bool        add_after(const std::string &after_id, Node node);
    bool        add_before(const std::string &before_id, Node node);
    bool        remove(const std::string &id);
    bool        move_before(const std::string &id, const std::string &before_id);
    bool        move_after(const std::string &id, const std::string &after_id);
    bool        add_parallel_branch(const std::string &id);
    bool        set_param(const std::string &id, const std::string &key, double value);
    const Node *selected_node(const std::string &id) const;

  private:
    std::string                kind_    = "apg.project";
    std::string                schema_  = "apg.project.v2";
    std::string                name_    = "terminal-project";
    std::string                version_ = "2.1.0";
    std::vector<UnitReference> units_;
    std::vector<Node>          nodes_;
    std::vector<Route>         routes_;
    std::vector<std::string>   export_profiles_;
    std::string                default_profile_ = "desktop_full";
    YAML::Node                 scenes_;
};

} // namespace apg::terminal

#endif
