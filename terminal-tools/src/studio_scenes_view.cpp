#include "apg_terminal/studio_scenes_view.hpp"

namespace apg::terminal {

ftxui::Element render_scenes_view(
    const ProjectEditor &editor, std::size_t selected_scene, Pane active_pane, std::deque<SceneHit> &scene_hits
) {
    using namespace ftxui;
    Elements    rows;
    const auto &scenes = editor.document().scenes();
    if (scenes.empty()) {
        rows.push_back(text("No scenes · n captures the current sound") | dim);
    } else {
        Elements chips;
        for (std::size_t index = 0; index < scenes.size(); ++index) {
            const auto &scene = scenes[index];
            scene_hits.push_back({scene.name, {}});
            const bool active = editor.active_scene() && *editor.active_scene() == scene.name;
            auto       chip   = vbox({
                            text(scene.name + (active && editor.scene_modified() ? " *" : "")) | bold,
                            text(
                                std::to_string(scene.params.size()) + " controls · " +
                                std::to_string(scene.bypass.size()) + " switches"
                            ) | dim,
                        }) |
                        border;
            if (index == selected_scene) {
                chip = chip | color(Color::Cyan);
                if (active_pane == Pane::Scenes)
                    chip = chip | focus;
            }
            chips.push_back(chip | reflect(scene_hits.back().box));
        }
        rows.push_back(hbox(std::move(chips)) | hscroll_indicator | xframe);
    }
    rows.push_back(text("Enter recall · n new · u update · e rename · d delete") | dim);
    return vbox(std::move(rows)) | flex;
}

bool handle_scenes_event(
    const ftxui::Event                        &event,
    ProjectEditor                             &editor,
    std::size_t                               &selected_scene,
    Modal                                     &modal,
    std::string                               &modal_text,
    std::string                               &transient_status,
    std::function<void(std::function<void()>)> act_fn
) {
    using namespace ftxui;
    const auto &scenes = editor.document().scenes();
    if (event == Event::Character("n")) {
        modal_text = "Scene " + std::to_string(scenes.size() + 1);
        modal      = Modal::NewScene;
        return true;
    }
    if (scenes.empty())
        return false;
    if (event == Event::ArrowLeft || event == Event::Character("h")) {
        selected_scene = selected_scene == 0 ? scenes.size() - 1 : selected_scene - 1;
        return true;
    }
    if (event == Event::ArrowRight || event == Event::Character("l")) {
        selected_scene = (selected_scene + 1) % scenes.size();
        return true;
    }
    const auto scene_name = scenes[selected_scene].name;
    if (event == Event::Return) {
        act_fn([&] { editor.recall_scene(scene_name); });
        return true;
    }
    if (event == Event::Character("u")) {
        act_fn([&] { editor.save_scene(scene_name); });
        return true;
    }
    if (event == Event::Character("e")) {
        modal_text = scene_name;
        modal      = Modal::RenameScene;
        return true;
    }
    if (event == Event::Character("d")) {
        modal = Modal::DeleteScene;
        return true;
    }
    return false;
}

} // namespace apg::terminal
