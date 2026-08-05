#include "apg_terminal/application/editor.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace apg::terminal {
namespace {

constexpr std::size_t kHistoryLimit = 100;

std::string trimmed(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

} // namespace

ProjectEditor::ProjectEditor(ApgPackageDocument document) : current_{.document = std::move(document)} {
    saved_project_content_ = current_.document.project_content();
    initialize_bypass();
    status_ = "Loaded " + current_.document.path().string();
}

void ProjectEditor::initialize_bypass() {
    for (const auto &node : current_.document.nodes()) {
        if (!node.routing_helper())
            current_.bypass.try_emplace(node.id, false);
    }
}

bool ProjectEditor::bypassed(const std::string &node_id) const {
    const auto found = current_.bypass.find(node_id);
    return found != current_.bypass.end() && found->second;
}

void ProjectEditor::push_undo() {
    undo_.push_back(current_);
    if (undo_.size() > kHistoryLimit)
        undo_.erase(undo_.begin());
    redo_.clear();
}

void ProjectEditor::notify(bool structural) {
    if (on_change_)
        on_change_(current_.document, current_.bypass, structural);
}

void ProjectEditor::set_modified() {
    if (current_.active_scene)
        current_.scene_modified = true;
}

void ProjectEditor::set_param(const std::string &node_id, const std::string &parameter, double value) {
    auto candidate = current_.document;
    candidate.set_param(node_id, parameter, value);
    if (candidate.project_content() == current_.document.project_content()) {
        status_ = "Parameter is already at that value";
        return;
    }
    push_undo();
    current_.document = std::move(candidate);
    set_modified();
    status_ = "Adjusted " + node_id + "." + parameter;
    notify(false);
}

void ProjectEditor::toggle_bypass(const std::string &node_id) {
    const auto *node = current_.document.find_node(node_id);
    if (!node)
        throw std::runtime_error("Project instance \"" + node_id + "\" was not found.");
    if (node->routing_helper())
        throw std::runtime_error("Routing helpers are always active and cannot be bypassed.");
    push_undo();
    current_.bypass[node_id] = !bypassed(node_id);
    set_modified();
    status_ = current_.bypass[node_id] ? "Bypassed " + node_id : "Enabled " + node_id;
    notify(false);
}

std::string ProjectEditor::insert_on_route(const Route &route, const std::string &unit_id) {
    auto       candidate = current_.document;
    const auto id        = candidate.insert_on_route(route, unit_id);
    push_undo();
    current_.document   = std::move(candidate);
    current_.bypass[id] = false;
    status_             = "Inserted " + id;
    notify(true);
    return id;
}

void ProjectEditor::move_to_route(const std::string &node_id, const Route &route) {
    auto candidate = current_.document;
    candidate.move_to_route(node_id, route);
    push_undo();
    current_.document = std::move(candidate);
    status_           = "Moved " + node_id;
    notify(true);
}

void ProjectEditor::remove_node(const std::string &node_id) {
    auto candidate = current_.document;
    candidate.remove_node(node_id);
    push_undo();
    current_.document = std::move(candidate);
    current_.bypass.erase(node_id);
    set_modified();
    status_ = "Removed " + node_id;
    notify(true);
}

std::string ProjectEditor::add_parallel_on_route(const Route &route, const std::string &effect_unit_id) {
    auto       candidate = current_.document;
    const auto id        = candidate.add_parallel_on_route(route, effect_unit_id);
    push_undo();
    current_.document = std::move(candidate);
    initialize_bypass();
    status_ = "Added nested parallel path with " + id;
    notify(true);
    return id;
}

void ProjectEditor::collapse_parallel(const std::string &section) {
    std::vector<std::string> removed;
    for (const auto &node : current_.document.nodes()) {
        if (node.routing_section == section)
            removed.push_back(node.id);
    }
    auto candidate = current_.document;
    candidate.collapse_parallel(section);
    push_undo();
    current_.document = std::move(candidate);
    for (const auto &node : removed)
        current_.bypass.erase(node);
    set_modified();
    status_ = "Collapsed " + section;
    notify(true);
}

void ProjectEditor::save_scene(const std::string &name, bool allow_overwrite) {
    const auto normalized = trimmed(name);
    auto       candidate  = current_.document;
    candidate.upsert_scene(name, current_.bypass, allow_overwrite);
    push_undo();
    current_.document       = std::move(candidate);
    current_.active_scene   = normalized;
    current_.scene_modified = false;
    status_                 = (allow_overwrite ? "Updated scene " : "Saved scene ") + normalized;
    notify(false);
}

void ProjectEditor::recall_scene(const std::string &name) {
    auto candidate = current_.document;
    auto recalled  = candidate.recall_scene(name);
    push_undo();
    current_.document = std::move(candidate);
    for (const auto &[node, bypassed] : recalled.bypass)
        current_.bypass[node] = bypassed;
    current_.active_scene   = name;
    current_.scene_modified = false;
    status_                 = "Recalled scene " + name;
    notify(false);
}

void ProjectEditor::rename_scene(const std::string &name, const std::string &next_name) {
    const auto normalized = trimmed(next_name);
    auto       candidate  = current_.document;
    candidate.rename_scene(name, next_name);
    push_undo();
    current_.document = std::move(candidate);
    if (current_.active_scene && *current_.active_scene == name)
        current_.active_scene = normalized;
    status_ = "Renamed scene " + name + " to " + normalized;
    notify(false);
}

void ProjectEditor::remove_scene(const std::string &name) {
    auto candidate = current_.document;
    candidate.remove_scene(name);
    push_undo();
    current_.document = std::move(candidate);
    if (current_.active_scene && *current_.active_scene == name) {
        current_.active_scene.reset();
        current_.scene_modified = false;
    }
    status_ = "Removed scene " + name;
    notify(false);
}

bool ProjectEditor::undo() {
    if (undo_.empty()) {
        status_ = "Nothing to undo";
        return false;
    }
    redo_.push_back(current_);
    current_ = std::move(undo_.back());
    undo_.pop_back();
    status_ = "Undid last edit";
    notify(true);
    return true;
}

bool ProjectEditor::redo() {
    if (redo_.empty()) {
        status_ = "Nothing to redo";
        return false;
    }
    undo_.push_back(current_);
    current_ = std::move(redo_.back());
    redo_.pop_back();
    status_ = "Redid last edit";
    notify(true);
    return true;
}

void ProjectEditor::save(const std::optional<std::string> &timestamp) {
    current_.document.save_atomic(timestamp);
    saved_project_content_ = current_.document.project_content();
    status_                = "Saved " + current_.document.path().string();
}

} // namespace apg::terminal
