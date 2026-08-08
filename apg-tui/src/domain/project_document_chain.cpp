#include "apg_terminal/domain/project_document.hpp"
#include "apg_terminal/domain/project_document_internal.hpp"

namespace apg::terminal {

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

} // namespace apg::terminal
