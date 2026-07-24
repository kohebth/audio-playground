#include "apg_terminal/project_document.hpp"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <system_error>

namespace apg::terminal {
namespace {

std::string scalar(const YAML::Node &node, const char *key, const std::string &fallback = {}) {
    if (!node || !node.IsMap())
        return fallback;
    const auto value = node[key];
    return value && value.IsScalar() ? value.as<std::string>() : fallback;
}

std::string endpoint_node(const std::string &endpoint) {
    const auto dot = endpoint.find('.');
    return dot == std::string::npos ? endpoint : endpoint.substr(0, dot);
}

double number(const YAML::Node &node, const char *key, double fallback) {
    const auto value = node[key];
    return value && value.IsScalar() ? value.as<double>() : fallback;
}

bool has_routing_sections(const std::vector<Node> &nodes) {
    return std::any_of(nodes.begin(), nodes.end(), [](const Node &node) { return !node.routing_section.empty(); });
}

} // namespace

ProjectDocument ProjectDocument::load(const std::filesystem::path &path) {
    const YAML::Node root = YAML::LoadFile(path.string());
    ProjectDocument  document;
    document.kind_    = scalar(root, "kind", document.kind_);
    document.schema_  = scalar(root, "schema", document.schema_);
    document.name_    = scalar(root, "name", document.name_);
    document.version_ = scalar(root, "version", document.version_);

    for (const auto &item : root["units"]) {
        document.units_.push_back({scalar(item, "id"), scalar(item, "file")});
    }
    for (const auto &item : root["chain"]["nodes"]) {
        Node       node{scalar(item, "id"), scalar(item, "unit"), scalar(item["routing"], "section"), {}};
        const auto params = item["params"];
        if (params && params.IsMap()) {
            for (const auto &parameter : params) {
                node.params[parameter.first.as<std::string>()] = parameter.second.as<double>();
            }
        }
        const auto unit_id = node.unit;
        const auto unit_ref =
            std::find_if(document.units_.begin(), document.units_.end(), [&](const UnitReference &reference) {
                return reference.id == unit_id;
            });
        if (unit_ref != document.units_.end()) {
            const auto unit_path = path.parent_path() / unit_ref->file;
            try {
                const auto unit_root   = YAML::LoadFile(unit_path.string());
                const auto unit_params = unit_root["params"];
                if (unit_params && unit_params.IsMap()) {
                    for (const auto &entry : unit_params) {
                        const auto name       = entry.first.as<std::string>();
                        const auto definition = entry.second;
                        const auto ui         = definition["ui"];
                        const auto value =
                            node.params.contains(name) ? node.params.at(name) : number(definition, "default", 0.0);
                        node.params[name] = value;
                        node.parameter_specs.push_back({
                            name,
                            scalar(ui, "label", name),
                            scalar(ui, "unit"),
                            value,
                            number(definition, "min", 0.0),
                            number(definition, "max", 1.0),
                        });
                    }
                }
            } catch (const YAML::Exception &) {
                // The APGCore validator remains authoritative for missing/invalid unit files.
            }
        }
        document.nodes_.push_back(std::move(node));
    }
    for (const auto &item : root["chain"]["routes"]) {
        document.routes_.push_back({scalar(item, "from"), scalar(item, "to")});
    }
    document.default_profile_ = scalar(root["targets"], "default", document.default_profile_);
    const auto scenes         = root["scenes"];
    if (scenes && scenes.IsSequence())
        document.scenes_ = scenes;
    for (const auto &profile : root["targets"]["export"]) {
        document.export_profiles_.push_back(profile.as<std::string>());
    }
    return document;
}

void ProjectDocument::save_atomic(const std::filesystem::path &path) const {
    const auto errors = validate();
    if (!errors.empty()) {
        throw std::runtime_error("refusing to save invalid project: " + errors.front());
    }

    YAML::Node root;
    root["kind"]    = kind_;
    root["schema"]  = schema_;
    root["name"]    = name_;
    root["version"] = version_;
    for (const auto &unit : units_) {
        YAML::Node item;
        item["id"]   = unit.id;
        item["file"] = unit.file;
        root["units"].push_back(item);
    }
    for (const auto &node : nodes_) {
        YAML::Node item;
        item["id"]   = node.id;
        item["unit"] = node.unit;
        if (!node.routing_section.empty())
            item["routing"]["section"] = node.routing_section;
        for (const auto &[key, value] : node.params)
            item["params"][key] = value;
        root["chain"]["nodes"].push_back(item);
    }
    for (const auto &route : routes_) {
        YAML::Node item;
        item["from"] = route.from;
        item["to"]   = route.to;
        root["chain"]["routes"].push_back(item);
    }
    root["targets"]["default"] = default_profile_;
    for (const auto &profile : export_profiles_)
        root["targets"]["export"].push_back(profile);
    if (scenes_ && scenes_.IsSequence())
        root["scenes"] = scenes_;

    const auto temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output)
            throw std::runtime_error("cannot open temporary project file");
        output << root << '\n';
        if (!output)
            throw std::runtime_error("cannot write temporary project file");
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary);
        throw std::system_error(error, "cannot atomically replace project file");
    }
}

std::vector<std::string> ProjectDocument::validate() const {
    std::vector<std::string> errors;
    if (kind_ != "apg.project")
        errors.push_back("kind must be apg.project");
    if (schema_ != "apg.project.v2")
        errors.push_back("schema must be apg.project.v2");
    std::map<std::string, bool> units;
    for (const auto &unit : units_) {
        if (unit.id.empty() || unit.file.empty())
            errors.push_back("unit references need id and file");
        if (!units.emplace(unit.id, true).second)
            errors.push_back("duplicate unit reference: " + unit.id);
    }
    std::map<std::string, bool> nodes;
    for (const auto &node : nodes_) {
        if (node.id.empty())
            errors.push_back("node id cannot be empty");
        if (!nodes.emplace(node.id, true).second)
            errors.push_back("duplicate node: " + node.id);
        if (!units.contains(node.unit))
            errors.push_back("node references unknown unit: " + node.unit);
    }
    std::map<std::string, bool> sources;
    std::map<std::string, bool> targets;
    for (const auto &route : routes_) {
        const auto source_node = endpoint_node(route.from);
        const auto target_node = endpoint_node(route.to);
        if (source_node != "system" && !nodes.contains(source_node))
            errors.push_back("route source node missing: " + source_node);
        if (target_node != "system" && !nodes.contains(target_node))
            errors.push_back("route target node missing: " + target_node);
        if (!sources.emplace(route.from, true).second)
            errors.push_back("route source connected twice: " + route.from);
        if (!targets.emplace(route.to, true).second)
            errors.push_back("route target connected twice: " + route.to);
    }
    if (nodes.empty() && routes_.size() != 1)
        errors.push_back("empty project needs one direct route");
    return errors;
}

bool ProjectDocument::add_after(const std::string &after_id, Node node) {
    const auto before = *this;
    if (node.id.empty() || node.unit.empty())
        return false;
    if (std::any_of(nodes_.begin(), nodes_.end(), [&](const Node &item) { return item.id == node.id; }))
        return false;
    const auto source = after_id.empty() ? "system.input" : after_id + ".output";
    auto route = std::find_if(routes_.begin(), routes_.end(), [&](const Route &item) { return item.from == source; });
    if (route == routes_.end())
        return false;
    const auto old_target = route->to;
    route->to             = node.id + ".input";
    routes_.push_back({node.id + ".output", old_target});
    nodes_.push_back(std::move(node));
    if (!validate().empty()) {
        *this = before;
        return false;
    }
    return true;
}

bool ProjectDocument::add_before(const std::string &before_id, Node node) {
    const auto before = *this;
    if (node.id.empty() || node.unit.empty())
        return false;
    if (std::any_of(nodes_.begin(), nodes_.end(), [&](const Node &item) { return item.id == node.id; }))
        return false;
    const auto target =
        std::find_if(nodes_.begin(), nodes_.end(), [&](const Node &item) { return item.id == before_id; });
    if (target == nodes_.end())
        return false;
    const auto source = std::find_if(routes_.begin(), routes_.end(), [&](const Route &item) {
        return item.to == before_id + ".input";
    });
    if (source == routes_.end())
        return false;
    const auto old_target = source->to;
    source->to            = node.id + ".input";
    routes_.push_back({node.id + ".output", old_target});
    nodes_.insert(target, std::move(node));
    if (!validate().empty()) {
        *this = before;
        return false;
    }
    return true;
}

bool ProjectDocument::remove(const std::string &id) {
    const auto before = *this;
    const auto node   = std::find_if(nodes_.begin(), nodes_.end(), [&](const Node &item) { return item.id == id; });
    if (node == nodes_.end())
        return false;
    auto incoming =
        std::find_if(routes_.begin(), routes_.end(), [&](const Route &route) { return route.to == id + ".input"; });
    auto outgoing =
        std::find_if(routes_.begin(), routes_.end(), [&](const Route &route) { return route.from == id + ".output"; });
    if (incoming == routes_.end() || outgoing == routes_.end())
        return false;
    const auto replacement = Route{incoming->from, outgoing->to};
    routes_.erase(
        std::remove_if(
            routes_.begin(), routes_.end(),
            [&](const Route &route) { return route.to == id + ".input" || route.from == id + ".output"; }
        ),
        routes_.end()
    );
    routes_.push_back(replacement);
    nodes_.erase(node);
    if (!validate().empty()) {
        *this = before;
        return false;
    }
    return true;
}

bool ProjectDocument::move_before(const std::string &id, const std::string &before_id) {
    const auto before = *this;
    if (id == before_id)
        return true;
    if (has_routing_sections(nodes_))
        return false;
    auto source = std::find_if(nodes_.begin(), nodes_.end(), [&](const Node &node) { return node.id == id; });
    auto target = std::find_if(nodes_.begin(), nodes_.end(), [&](const Node &node) { return node.id == before_id; });
    if (source == nodes_.end() || target == nodes_.end() || !source->routing_section.empty() ||
        !target->routing_section.empty())
        return false;
    Node moving = *source;
    if (!remove(id))
        return false;
    auto target_after_remove =
        std::find_if(nodes_.begin(), nodes_.end(), [&](const Node &node) { return node.id == before_id; });
    const auto previous = std::find_if(routes_.begin(), routes_.end(), [&](const Route &route) {
        return route.to == before_id + ".input";
    });
    if (target_after_remove == nodes_.end() || previous == routes_.end()) {
        *this = before;
        return false;
    }
    const auto old_target = previous->to;
    previous->to          = id + ".input";
    routes_.push_back({id + ".output", old_target});
    nodes_.insert(target_after_remove, std::move(moving));
    if (!validate().empty()) {
        *this = before;
        return false;
    }
    return true;
}

bool ProjectDocument::move_after(const std::string &id, const std::string &after_id) {
    const auto before = *this;
    if (id == after_id)
        return true;
    if (has_routing_sections(nodes_))
        return false;
    auto source = std::find_if(nodes_.begin(), nodes_.end(), [&](const Node &node) { return node.id == id; });
    auto target = std::find_if(nodes_.begin(), nodes_.end(), [&](const Node &node) { return node.id == after_id; });
    if (source == nodes_.end() || target == nodes_.end() || !source->routing_section.empty() ||
        !target->routing_section.empty())
        return false;
    Node moving = *source;
    if (!remove(id))
        return false;
    auto target_after_remove =
        std::find_if(nodes_.begin(), nodes_.end(), [&](const Node &node) { return node.id == after_id; });
    auto next = std::find_if(routes_.begin(), routes_.end(), [&](const Route &route) {
        return route.from == after_id + ".output";
    });
    if (target_after_remove == nodes_.end() || next == routes_.end()) {
        *this = before;
        return false;
    }
    const auto old_target = next->to;
    next->to              = id + ".input";
    routes_.push_back({id + ".output", old_target});
    nodes_.insert(std::next(target_after_remove), std::move(moving));
    if (!validate().empty()) {
        *this = before;
        return false;
    }
    return true;
}

bool ProjectDocument::add_parallel_branch(const std::string &id) {
    const auto before = *this;
    if (has_routing_sections(nodes_))
        return false;
    const auto selected = std::find_if(nodes_.begin(), nodes_.end(), [&](const Node &node) { return node.id == id; });
    if (selected == nodes_.end())
        return false;
    auto panner = std::find_if(units_.begin(), units_.end(), [](const UnitReference &unit) {
        return unit.file.find("path_panner_2") != std::string::npos;
    });
    auto panner_id = panner == units_.end() ? std::string("path_panner_2_unit") : panner->id;
    if (panner == units_.end())
        units_.push_back({"path_panner_2_unit", "../units-v2/path_panner_2.unit.v2.yaml"});
    auto mixer = std::find_if(units_.begin(), units_.end(), [](const UnitReference &unit) {
        return unit.file.find("path_mixer_2") != std::string::npos;
    });
    auto mixer_id = mixer == units_.end() ? std::string("path_mixer_2_unit") : mixer->id;
    if (mixer == units_.end())
        units_.push_back({"path_mixer_2_unit", "../units-v2/path_mixer_2.unit.v2.yaml"});
    auto incoming =
        std::find_if(routes_.begin(), routes_.end(), [&](const Route &route) { return route.to == id + ".input"; });
    auto outgoing =
        std::find_if(routes_.begin(), routes_.end(), [&](const Route &route) { return route.from == id + ".output"; });
    if (incoming == routes_.end() || outgoing == routes_.end())
        return false;
    const auto section = "parallel_" + std::to_string(nodes_.size() + 1);
    const auto pan_id  = section + "_pan";
    const auto mix_id  = section + "_mix";
    const auto source  = incoming->from;
    const auto target  = outgoing->to;
    nodes_.insert(selected, Node{pan_id, panner_id, section, {}, {}});
    const auto selected_after =
        std::find_if(nodes_.begin(), nodes_.end(), [&](const Node &node) { return node.id == id; });
    nodes_.insert(std::next(selected_after), Node{mix_id, mixer_id, section, {}, {}});
    routes_.erase(
        std::remove_if(
            routes_.begin(), routes_.end(),
            [&](const Route &route) { return route.to == id + ".input" || route.from == id + ".output"; }
        ),
        routes_.end()
    );
    routes_.push_back({source, pan_id + ".input"});
    routes_.push_back({pan_id + ".path_1", mix_id + ".path_1"});
    routes_.push_back({pan_id + ".path_2", id + ".input"});
    routes_.push_back({id + ".output", mix_id + ".path_2"});
    routes_.push_back({mix_id + ".output", target});
    if (!validate().empty()) {
        *this = before;
        return false;
    }
    return true;
}

bool ProjectDocument::set_param(const std::string &id, const std::string &key, double value) {
    auto node = std::find_if(nodes_.begin(), nodes_.end(), [&](const Node &item) { return item.id == id; });
    if (node == nodes_.end() || key.empty())
        return false;
    for (auto &parameter : node->parameter_specs) {
        if (parameter.name == key) {
            value           = std::clamp(value, parameter.min, parameter.max);
            parameter.value = value;
            break;
        }
    }
    node->params[key] = value;
    return true;
}

const Node *ProjectDocument::selected_node(const std::string &id) const {
    const auto node = std::find_if(nodes_.begin(), nodes_.end(), [&](const Node &item) { return item.id == id; });
    return node == nodes_.end() ? nullptr : &*node;
}

} // namespace apg::terminal
