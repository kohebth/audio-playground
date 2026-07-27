#ifndef APG_TERMINAL_EDITOR_HPP
#define APG_TERMINAL_EDITOR_HPP

#include "apg_terminal/project_document.hpp"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace apg::terminal {

class ProjectEditor {
  public:
    using ChangeCallback =
        std::function<void(const ApgPackageDocument &, const std::map<std::string, bool> &, bool structural)>;

    explicit ProjectEditor(ApgPackageDocument document);

    [[nodiscard]] const ApgPackageDocument          &document() const { return current_.document; }
    [[nodiscard]] ApgPackageDocument                &document() { return current_.document; }
    [[nodiscard]] const std::map<std::string, bool> &bypass() const { return current_.bypass; }
    [[nodiscard]] bool                               bypassed(const std::string &node_id) const;
    [[nodiscard]] const std::optional<std::string>  &active_scene() const { return current_.active_scene; }
    [[nodiscard]] bool                               scene_modified() const { return current_.scene_modified; }
    [[nodiscard]] bool                               can_undo() const { return !undo_.empty(); }
    [[nodiscard]] bool                               can_redo() const { return !redo_.empty(); }
    [[nodiscard]] bool dirty() const { return current_.document.project_content() != saved_project_content_; }
    [[nodiscard]] const std::string &status() const { return status_; }

    void set_change_callback(ChangeCallback callback) { on_change_ = std::move(callback); }

    void        set_param(const std::string &node_id, const std::string &parameter, double value);
    void        toggle_bypass(const std::string &node_id);
    std::string insert_on_route(const Route &route, const std::string &unit_id);
    void        move_to_route(const std::string &node_id, const Route &route);
    void        remove_node(const std::string &node_id);
    std::string add_parallel_on_route(const Route &route, const std::string &effect_unit_id);
    void        collapse_parallel(const std::string &section);

    void save_scene(const std::string &name, bool allow_overwrite = false);
    void recall_scene(const std::string &name);
    void rename_scene(const std::string &name, const std::string &next_name);
    void remove_scene(const std::string &name);

    bool undo();
    bool redo();
    void save(const std::optional<std::string> &timestamp = std::nullopt);

  private:
    struct Revision {
        ApgPackageDocument          document;
        std::map<std::string, bool> bypass;
        std::optional<std::string>  active_scene;
        bool                        scene_modified = false;
    };

    Revision              current_;
    std::string           saved_project_content_;
    std::vector<Revision> undo_;
    std::vector<Revision> redo_;
    std::string           status_;
    ChangeCallback        on_change_;

    void push_undo();
    void notify(bool structural);
    void set_modified();
    void initialize_bypass();
};

} // namespace apg::terminal

#endif
