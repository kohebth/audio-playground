#include "apg_terminal/domain/project_document.hpp"
#include "apg_terminal/domain/project_document_internal.hpp"

#include <algorithm>

namespace apg::terminal {

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

} // namespace apg::terminal
