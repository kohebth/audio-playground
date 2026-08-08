#include "apg_terminal/domain/project_document.hpp"
#include "apg_terminal/domain/project_document_internal.hpp"

namespace apg::terminal {

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

} // namespace apg::terminal
