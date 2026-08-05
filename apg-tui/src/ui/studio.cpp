#include "apg_terminal/ui/studio.hpp"

#include "apg_terminal/ui/studio_graph_view.hpp"
#include "apg_terminal/ui/studio_inspector_view.hpp"
#include "apg_terminal/ui/studio_modals.hpp"
#include "apg_terminal/ui/studio_scenes_view.hpp"
#include "apg_terminal/ui/studio_types.hpp"
#include "apg_terminal/ui/studio_units_view.hpp"

#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace apg::terminal {
namespace {

std::string strip_ansi_codes(const std::string &input) {
    std::string result;
    bool        in_escape = false;
    for (char character : input) {
        if (character == '\033') {
            in_escape = true;
        } else if (in_escape) {
            if ((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z')) {
                in_escape = false;
            }
        } else {
            result += character;
        }
    }
    return result;
}

std::string base64_encode(const std::string &in) {
    static const char lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string       out;
    int               val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(lookup[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6)
        out.push_back(lookup[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4)
        out.push_back('=');
    return out;
}

void copy_to_clipboard_osc52(const std::string &text) {
    std::cout << "\033]52;c;" << base64_encode(text) << "\007" << std::flush;
}

class StudioComponent final : public ftxui::ComponentBase {
  public:
    StudioComponent(
        ProjectEditor        &editor,
        AudioSession         &audio,
        std::function<void()> request_exit,
        TerminalSizeProvider  size_provider = {},
        bool                  debug_mode    = false
    )
        : editor_(editor), audio_(audio), request_exit_(std::move(request_exit)),
          size_provider_(std::move(size_provider)), debug_mode_(debug_mode) {
        if (!size_provider_)
            size_provider_ = []() -> std::pair<int, int> {
                auto size = ftxui::Terminal::Size();
                return {size.dimx, size.dimy};
            };
        normalize_selection();
        editor_.set_change_callback(
            [this](const ApgPackageDocument &doc, const std::map<std::string, bool> &bypass, bool structural) {
                (void)audio_.synchronize(doc, bypass, structural);
            }
        );
        (void)audio_.synchronize(editor_.document(), editor_.bypass(), true);
    }

    ~StudioComponent() override { editor_.set_change_callback(nullptr); }

    ftxui::Element OnRender() override {
        using namespace ftxui;
        clear_hits();
        normalize_selection();
        const auto [width, height] = size_provider_();
        if (width < 80 || height < 24) {
            return vbox({
                       text("Terminal too small") | bold | color(Color::Red),
                       text(
                           "Minimum 80×24 required; current size " + std::to_string(width) + "×" +
                           std::to_string(height)
                       ),
                   }) |
                   center;
        }
        auto body = wide_layout() ? render_wide() : render_compact();
        auto page = vbox({
            render_header(),
            body | flex,
            render_status(),
        });
        return render_modal(std::move(page));
    }

    bool OnEvent(ftxui::Event event) override {
        using namespace ftxui;
        if (event.is_mouse())
            return handle_mouse(event);
        if (searching_)
            return handle_search(event);
        if (modal_ != Modal::None)
            return handle_modal(event);
        if (event == Event::Special("\x04") || event == Event::F12 || (debug_mode_ && event == Event::Character("d"))) {
            trigger_debug_snapshot();
            return true;
        }
        if (event == Event::Character("?")) {
            modal_ = Modal::Help;
            return true;
        }
        if (event == Event::Character("q")) {
            if (editor_.dirty()) {
                modal_ = Modal::Quit;
            } else {
                request_exit_();
            }
            return true;
        }
        if (event == Event::Special("\x13")) {
            save();
            return true;
        }
        if (event == Event::Character("u") && active_pane_ != Pane::Scenes && !searching_) {
            act([&] { editor_.undo(); });
            return true;
        }
        if ((event == Event::Special("\x12") || event == Event::Special("\x19")) && active_pane_ != Pane::Scenes && !searching_) {
            act([&] { editor_.redo(); });
            return true;
        }
        if (event == Event::Special("\x1a")) {
            act([&] { editor_.undo(); });
            return true;
        }
        if (event == Event::Character(" ")) {
            toggle_transport();
            return true;
        }
        if (event == Event::Character("m")) {
            audio_.set_mute(!audio_.muted());
            transient_status_ = audio_.muted() ? "Audio muted" : "Audio unmuted";
            return true;
        }
        if (event == Event::Tab) {
            cycle_pane(1);
            return true;
        }
        if (event == Event::Special("\x1b[Z")) {
            cycle_pane(-1);
            return true;
        }
        if (event == Event::Escape) {
            if (carried_unit_ || dragged_unit_) {
                cancel_unit_placement();
                transient_status_ = "Unit placement cancelled";
                return true;
            }
        }
        switch (active_pane_) {
        case Pane::Units:
            return handle_units(event);
        case Pane::Graph:
            return handle_graph(event);
        case Pane::Inspector:
            return handle_inspector(event);
        case Pane::Scenes:
            return handle_scenes(event);
        case Pane::Audio:
            return handle_audio(event);
        case Pane::Problems:
            return false;
        }
        return false;
    }

  private:
    void clear_hits() {
        node_hits_.clear();
        route_hits_.clear();
        unit_hits_.clear();
        scene_hits_.clear();
        parameter_hits_.clear();
        tab_hits_.clear();
        pane_boxes_.fill({});
    }

    void normalize_selection() {
        const auto &nodes = editor_.document().nodes();
        if (nodes.empty()) {
            selected_node_.clear();
        } else if (!editor_.document().find_node(selected_node_)) {
            selected_node_ = nodes.front().id;
        }
        const auto &routes = editor_.document().routes();
        if (routes.empty()) {
            selected_route_.reset();
        } else if (!selected_route_ || std::find(routes.begin(), routes.end(), *selected_route_) == routes.end()) {
            selected_route_ = routes.front();
        }
        selected_unit_      = normalize_index(selected_unit_, placeable_units().size());
        selected_scene_     = normalize_index(selected_scene_, editor_.document().scenes().size());
        const auto *node    = editor_.document().find_node(selected_node_);
        selected_parameter_ = normalize_index(selected_parameter_, node ? node->parameter_specs.size() : 0);
    }

    static std::size_t normalize_index(std::size_t index, std::size_t size) {
        return size == 0 ? 0 : std::min(index, size - 1);
    }

    [[nodiscard]] bool wide_layout() const {
        const auto [width, height] = size_provider_();
        return width >= 120 && height >= 32;
    }

    [[nodiscard]] bool route_drop_active() const {
        return dragged_node_.has_value() || dragged_unit_.has_value() || carried_unit_.has_value();
    }

    void cancel_unit_placement() {
        carried_unit_.reset();
        dragged_unit_.reset();
        hovered_route_.reset();
    }

    void arm_unit_placement(const std::string &unit_id) {
        carried_unit_ = unit_id;
        hovered_route_.reset();
        active_pane_     = Pane::Graph;
        const auto *unit = editor_.document().find_unit(unit_id);
        transient_status_ =
            "Placing " + (unit && !unit->title.empty() ? unit->title : unit_id) + " — click a signal line; Esc cancels";
    }

    std::vector<const UnitReference *> placeable_units() const {
        std::vector<const UnitReference *> result;
        for (const auto &unit : editor_.document().units()) {
            if (!unit.user_placeable())
                continue;
            if (!unit_search_.empty()) {
                std::string haystack = unit.id + " " + unit.title + " " + unit.category;
                std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char value) {
                    return static_cast<char>(std::tolower(value));
                });
                std::string needle = unit_search_;
                std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char value) {
                    return static_cast<char>(std::tolower(value));
                });
                if (haystack.find(needle) == std::string::npos)
                    continue;
            }
            result.push_back(&unit);
        }
        return result;
    }

    ftxui::Element pane_frame(const std::string &title, ftxui::Element content, Pane pane) {
        using namespace ftxui;
        const auto pane_index = static_cast<std::size_t>(pane);
        Element    element;
        if (active_pane_ == pane) {
            element =
                window(text(" " + title + " ") | bold | color(Color::Cyan), std::move(content) | color(Color::White)) |
                color(Color::Cyan);
        } else {
            element = window(text(" " + title + " ") | bold, std::move(content));
        }
        return element | reflect(pane_boxes_[pane_index]);
    }

    ftxui::Element render_wide() {
        using namespace ftxui;
        auto    main = hbox({
            pane_frame("Units", render_units(), Pane::Units) | size(WIDTH, EQUAL, 27),
            pane_frame("Route graph", render_graph(), Pane::Graph) | flex,
            pane_frame("Inspector", render_inspector(), Pane::Inspector) | size(WIDTH, EQUAL, 35),
        });
        Element drawer;
        if (active_pane_ == Pane::Audio)
            drawer = pane_frame("Audio", render_audio(), Pane::Audio);
        else if (active_pane_ == Pane::Problems)
            drawer = pane_frame("Problems", render_problems(), Pane::Problems);
        else
            drawer = pane_frame("Scenes", render_scenes(), Pane::Scenes);
        return vbox({
            main | flex,
            drawer | size(HEIGHT, EQUAL, 7),
        });
    }

    ftxui::Element render_compact() {
        using namespace ftxui;
        Elements tabs;
        for (const auto pane : kPanes) {
            tab_hits_.push_back({pane, {}});
            auto tab = text(std::string(" ") + pane_name(pane) + " ");
            if (pane == active_pane_)
                tab = tab | bold | inverted;
            tabs.push_back(tab | reflect(tab_hits_.back().box));
        }
        Element content;
        switch (active_pane_) {
        case Pane::Units:
            content = render_units();
            break;
        case Pane::Graph:
            content = render_graph();
            break;
        case Pane::Inspector:
            content = render_inspector();
            break;
        case Pane::Scenes:
            content = render_scenes();
            break;
        case Pane::Audio:
            content = render_audio();
            break;
        case Pane::Problems:
            content = render_problems();
            break;
        }
        return vbox({
            hbox(std::move(tabs)) | frame | size(HEIGHT, EQUAL, 1),
            pane_frame(pane_name(active_pane_), std::move(content), active_pane_) | flex,
        });
    }

    ftxui::Element render_header() const {
        using namespace ftxui;
        return hbox({
            text(" APG Studio ") | bold | color(Color::Cyan),
            text(editor_.document().name() + " v" + editor_.document().version()) | dim,
            filler(),
            text(editor_.dirty() ? "Modified *" : "Saved") | color(editor_.dirty() ? Color::Yellow : Color::Green),
            text(" "),
        });
    }

    ftxui::Element render_status() const {
        using namespace ftxui;
        return hbox({
            text(" " + status_text() + " "),
            filler(),
            text("? help · Tab pane · Space audio ") | dim,
        });
    }

    ftxui::Element render_units() {
        return render_units_view(
            placeable_units(), searching_, unit_search_, wide_layout(), selected_unit_, active_pane_, dragged_unit_,
            carried_unit_, unit_hits_
        );
    }

    ftxui::Element render_graph() {
        const auto [width, height] = size_provider_();
        GraphRenderOptions options{
            .active_pane       = active_pane_,
            .selected_node     = selected_node_,
            .selected_route    = selected_route_,
            .hovered_route     = hovered_route_,
            .route_drop_active = route_drop_active(),
            .scroll_x          = graph_scroll_x_,
            .scroll_y          = graph_scroll_y_,
            .wide_layout       = wide_layout(),
            .width             = width,
            .height            = height,
        };
        return render_graph_view(editor_, options, route_hits_, node_hits_, render_error_, graph_content_box_);
    }

    ftxui::Element render_inspector() {
        return render_inspector_view(editor_, selected_node_, selected_parameter_, active_pane_, parameter_hits_);
    }

    ftxui::Element render_scenes() { return render_scenes_view(editor_, selected_scene_, active_pane_, scene_hits_); }

    ftxui::Element render_audio() {
        using namespace ftxui;
        const auto meter       = audio_.meter();
        const auto config      = audio_.config();
        const auto devices     = audio_.devices();
        const auto device_name = [&](const std::string &id, AudioDeviceKind kind) {
            const auto default_id = kind == AudioDeviceKind::Capture ? "capture:default" : "playback:default";
            if (id.empty() || id == default_id)
                return std::string("system default");
            const auto found = std::find_if(devices.begin(), devices.end(), [&](const AudioDeviceInfo &device) {
                return device.id == id && device.kind == kind;
            });
            return found == devices.end() ? id : found->name;
        };
        return vbox({
            hbox({
                text(audio_.running() ? "● Running" : "○ Stopped") |
                    color(audio_.running() ? Color::Green : Color::Yellow),
                text(audio_.muted() ? " · muted" : " · live"),
                filler(),
                text(std::to_string(config.sample_rate) + " Hz · " + std::to_string(config.period_frames) + " frames"),
            }),
            text(
                "Capture: " + device_name(config.capture_device, AudioDeviceKind::Capture) +
                " · Playback: " + device_name(config.playback_device, AudioDeviceKind::Playback)
            ),
            hbox({
                text("IN  "),
                gauge(std::min(1.0f, meter.input_peak)) | flex | color(Color::Green),
                text("  OUT "),
                gauge(std::min(1.0f, meter.output_peak)) | flex | color(meter.clipped ? Color::Red : Color::Green),
            }),
            text(audio_.diagnostic()) | dim,
            text("Space start/stop · m mute · c capture · p playback · s rate · f period") | dim,
        });
    }

    ftxui::Element render_problems() {
        using namespace ftxui;
        Elements rows{
            text("APGCore validation: ready") | color(Color::Green),
            text(editor_.dirty() ? "Package has unsaved changes" : "Package matches disk") |
                color(editor_.dirty() ? Color::Yellow : Color::Green),
            text("Audio: " + audio_.diagnostic()),
        };
        if (!render_error_.empty())
            rows.push_back(paragraph(render_error_) | color(Color::Red));
        return vbox(std::move(rows)) | yframe | flex;
    }

    ftxui::Element render_modal(ftxui::Element page) {
        return render_modal_dialog(modal_, modal_text_, selected_scene_name(), selected_node_, std::move(page));
    }

    bool handle_modal(const ftxui::Event &event) {
        return handle_modal_event(
            event, modal_, modal_text_, editor_, selected_scene_name(), selected_node_, [&] { return save(); },
            [&] { request_exit_(); }, [&](auto fn) { act(fn); }
        );
    }

    bool handle_search(const ftxui::Event &event) {
        using namespace ftxui;
        if (event == Event::Escape || event == Event::Return) {
            searching_ = false;
            return true;
        }
        if (event == Event::Backspace) {
            pop_utf8(unit_search_);
            selected_unit_ = 0;
            return true;
        }
        if (event.is_character() && event.character().size() <= 4 && unit_search_.size() < 64) {
            unit_search_ += event.character();
            selected_unit_ = 0;
            return true;
        }
        return true;
    }

    Pane pane_at(int x, int y) const {
        for (std::size_t index = 0; index < pane_boxes_.size(); ++index) {
            if (!pane_boxes_[index].IsEmpty() && pane_boxes_[index].Contain(x, y))
                return static_cast<Pane>(index);
        }
        return active_pane_;
    }

    bool handle_mouse(ftxui::Event event) {
        using namespace ftxui;
        const auto &mouse = event.mouse();
        if (mouse.button == Mouse::WheelUp || mouse.button == Mouse::WheelDown || mouse.button == Mouse::WheelLeft ||
            mouse.button == Mouse::WheelRight) {
            const int  delta  = (mouse.button == Mouse::WheelDown || mouse.button == Mouse::WheelRight) ? 1 : -1;
            const auto target = pane_at(mouse.x, mouse.y);
            switch (target) {
            case Pane::Units: {
                active_pane_     = Pane::Units;
                const auto units = placeable_units();
                if (!units.empty()) {
                    if (delta > 0)
                        selected_unit_ = (selected_unit_ + 1 < units.size()) ? selected_unit_ + 1 : units.size() - 1;
                    else
                        selected_unit_ = (selected_unit_ > 0) ? selected_unit_ - 1 : 0;
                }
                break;
            }
            case Pane::Inspector: {
                active_pane_     = Pane::Inspector;
                const auto *node = editor_.document().find_node(selected_node_);
                if (node && !node->parameter_specs.empty()) {
                    if (delta > 0)
                        selected_parameter_ = (selected_parameter_ + 1 < node->parameter_specs.size())
                                                  ? selected_parameter_ + 1
                                                  : node->parameter_specs.size() - 1;
                    else
                        selected_parameter_ = (selected_parameter_ > 0) ? selected_parameter_ - 1 : 0;
                }
                break;
            }
            case Pane::Scenes: {
                active_pane_       = Pane::Scenes;
                const auto &scenes = editor_.document().scenes();
                if (!scenes.empty()) {
                    if (delta > 0)
                        selected_scene_ =
                            (selected_scene_ + 1 < scenes.size()) ? selected_scene_ + 1 : scenes.size() - 1;
                    else
                        selected_scene_ = (selected_scene_ > 0) ? selected_scene_ - 1 : 0;
                }
                break;
            }
            case Pane::Graph: {
                active_pane_ = Pane::Graph;
                std::vector<std::string> ordered;
                try {
                    ordered = editor_.document().node_ids_in_route_order();
                } catch (const std::exception &) {
                    ordered.clear();
                }
                if (ordered.empty())
                    break;
                const auto found   = std::find(ordered.begin(), ordered.end(), selected_node_);
                const auto current = found == ordered.end() ? 0 : static_cast<int>(std::distance(ordered.begin(), found));
                const int  target  = delta > 0 ? std::min(current + 1, static_cast<int>(ordered.size()) - 1)
                                               : std::max(current - 1, 0);
                selected_node_      = ordered[static_cast<std::size_t>(target)];
                selected_parameter_ = 0;
                scroll_node_into_view(selected_node_);
                break;
            }
            default:
                break;
            }
            return true;
        }
        if (mouse.motion == Mouse::Moved && route_drop_active()) {
            if (const auto *route = hit_at(route_hits_, mouse.x, mouse.y))
                hovered_route_ = route->route;
            else
                hovered_route_.reset();
            return true;
        }
        if (mouse.button != Mouse::Left)
            return false;
        if (mouse.motion == Mouse::Released && dragged_parameter_) {
            const auto hit = *dragged_parameter_;
            dragged_parameter_.reset();
            const auto width = std::max(1, hit.box.x_max - hit.box.x_min);
            const auto ratio =
                static_cast<double>(std::clamp(mouse.x - hit.box.x_min, 0, width)) / static_cast<double>(width);
            act([&] { editor_.set_param(hit.node, hit.parameter, ratio_value(hit.spec, ratio)); });
            return true;
        }
        if (mouse.motion == Mouse::Released && dragged_node_) {
            const auto node = *dragged_node_;
            dragged_node_.reset();
            hovered_route_.reset();
            if (const auto *route = hit_at(route_hits_, mouse.x, mouse.y)) {
                const auto destination = route->route;
                act([&] { editor_.move_to_route(node, destination); });
            }
            return true;
        }
        if (mouse.motion == Mouse::Released && dragged_unit_) {
            const auto unit = *dragged_unit_;
            dragged_unit_.reset();
            hovered_route_.reset();
            if (const auto *route = hit_at(route_hits_, mouse.x, mouse.y)) {
                const auto destination = route->route;
                bool       inserted    = false;
                act([&] {
                    selected_node_ = editor_.insert_on_route(destination, unit);
                    inserted       = true;
                });
                if (inserted)
                    carried_unit_.reset();
            } else if (!wide_layout()) {
                const auto *released_unit = hit_at(unit_hits_, mouse.x, mouse.y);
                if (released_unit && released_unit->id == unit)
                    arm_unit_placement(unit);
            }
            return true;
        }
        if (mouse.motion == Mouse::Released && carried_unit_) {
            hovered_route_.reset();
            if (const auto *route = hit_at(route_hits_, mouse.x, mouse.y)) {
                const auto unit        = *carried_unit_;
                const auto destination = route->route;
                bool       inserted    = false;
                act([&] {
                    selected_node_ = editor_.insert_on_route(destination, unit);
                    inserted       = true;
                });
                if (inserted)
                    carried_unit_.reset();
            }
            return true;
        }
        if (mouse.motion != Mouse::Pressed)
            return false;
        if (const auto *tab = hit_at(tab_hits_, mouse.x, mouse.y)) {
            active_pane_ = tab->pane;
            hovered_route_.reset();
            return true;
        }
        const auto *pressed_unit = hit_at(unit_hits_, mouse.x, mouse.y);
        if (carried_unit_ && !pressed_unit) {
            if (const auto *route = hit_at(route_hits_, mouse.x, mouse.y)) {
                selected_route_ = route->route;
                hovered_route_  = route->route;
            }
            return true;
        }
        if (const auto *node = hit_at(node_hits_, mouse.x, mouse.y)) {
            selected_node_       = node->id;
            active_pane_         = Pane::Graph;
            selected_parameter_  = 0;
            const auto *selected = editor_.document().find_node(node->id);
            if (selected && !selected->routing_helper()) {
                dragged_node_ = node->id;
                hovered_route_.reset();
            }
            return true;
        }
        if (const auto *route = hit_at(route_hits_, mouse.x, mouse.y)) {
            selected_route_ = route->route;
            active_pane_    = Pane::Graph;
            return true;
        }
        if (const auto *unit = pressed_unit) {
            const auto units = placeable_units();
            const auto found = std::find_if(units.begin(), units.end(), [&](const UnitReference *item) {
                return item->id == unit->id;
            });
            if (found != units.end())
                selected_unit_ = static_cast<std::size_t>(std::distance(units.begin(), found));
            active_pane_  = Pane::Units;
            dragged_unit_ = unit->id;
            hovered_route_.reset();
            return true;
        }
        if (const auto *scene = hit_at(scene_hits_, mouse.x, mouse.y)) {
            const auto &scenes = editor_.document().scenes();
            const auto  found =
                std::find_if(scenes.begin(), scenes.end(), [&](const Scene &item) { return item.name == scene->name; });
            if (found != scenes.end())
                selected_scene_ = static_cast<std::size_t>(std::distance(scenes.begin(), found));
            active_pane_ = Pane::Scenes;
            return true;
        }
        if (pane_at(mouse.x, mouse.y) == Pane::Inspector) {
            if (const auto *parameter = hit_at(parameter_hits_, mouse.x, mouse.y)) {
                selected_node_   = parameter->node;
                const auto *node = editor_.document().find_node(parameter->node);
                if (node) {
                    const auto found = std::find_if(
                        node->parameter_specs.begin(), node->parameter_specs.end(),
                        [&](const Parameter &item) { return item.name == parameter->parameter; }
                    );
                    if (found != node->parameter_specs.end())
                        selected_parameter_ =
                            static_cast<std::size_t>(std::distance(node->parameter_specs.begin(), found));
                }
                active_pane_       = Pane::Inspector;
                dragged_parameter_ = *parameter;
                return true;
            }
        }
        return false;
    }

    bool handle_units(const ftxui::Event &event) {
        return handle_units_event(
            event, placeable_units(), searching_, selected_unit_, selected_route_, transient_status_, editor_,
            selected_node_, [&](auto fn) { act(fn); }
        );
    }

    bool handle_graph(const ftxui::Event &event) {
        return handle_graph_event(
            event, editor_, selected_node_, selected_route_, active_pane_, transient_status_, [&](auto fn) { act(fn); },
            [&](int dir) { cycle_node(dir); }, [&](int dir) { cycle_route(dir); }
        );
    }

    bool handle_inspector(const ftxui::Event &event) {
        return handle_inspector_event(event, editor_, selected_node_, selected_parameter_, modal_, [&](auto fn) {
            act(fn);
        });
    }

    bool handle_scenes(const ftxui::Event &event) {
        return handle_scenes_event(
            event, editor_, selected_scene_, modal_, modal_text_, transient_status_, [&](auto fn) { act(fn); }
        );
    }

    bool handle_audio(const ftxui::Event &event) {
        using namespace ftxui;
        if (event == Event::Character("c")) {
            cycle_audio_device(AudioDeviceKind::Capture);
            return true;
        }
        if (event == Event::Character("p")) {
            cycle_audio_device(AudioDeviceKind::Playback);
            return true;
        }
        if (event == Event::Character("s")) {
            auto config        = audio_.config();
            config.sample_rate = config.sample_rate == 48000 ? 44100 : 48000;
            configure_audio(config);
            return true;
        }
        if (event == Event::Character("f")) {
            auto                                   config = audio_.config();
            constexpr std::array<std::uint32_t, 3> frames{128, 256, 512};
            const auto found = std::find(frames.begin(), frames.end(), config.period_frames);
            const auto index =
                found == frames.end()
                    ? 0
                    : (static_cast<std::size_t>(std::distance(frames.begin(), found)) + 1) % frames.size();
            config.period_frames = frames[index];
            configure_audio(config);
            return true;
        }
        return false;
    }

    void cycle_audio_device(AudioDeviceKind kind) {
        if (audio_.running()) {
            transient_status_ = "Error: stop audio before changing devices";
            return;
        }
        std::vector<AudioDeviceInfo> devices;
        for (const auto &device : audio_.devices()) {
            if (device.kind == kind)
                devices.push_back(device);
        }
        if (devices.empty()) {
            transient_status_ = "Error: no matching audio devices";
            return;
        }
        auto       config  = audio_.config();
        const auto current = kind == AudioDeviceKind::Capture ? config.capture_device : config.playback_device;
        const auto found   = std::find_if(devices.begin(), devices.end(), [&](const AudioDeviceInfo &device) {
            return device.id == current;
        });
        const auto index   = found == devices.end()
                                 ? 0
                                 : (static_cast<std::size_t>(std::distance(devices.begin(), found)) + 1) % devices.size();
        if (kind == AudioDeviceKind::Capture)
            config.capture_device = devices[index].id;
        else
            config.playback_device = devices[index].id;
        configure_audio(config);
    }

    void configure_audio(const AudioDeviceConfig &config) {
        if (audio_.configure(config))
            transient_status_ = "Audio configuration updated";
        else
            transient_status_ = "Error: " + audio_.diagnostic();
    }

    void scroll_node_into_view(const std::string &node_id) {
        const auto graph_index = static_cast<std::size_t>(Pane::Graph);
        if (graph_index >= pane_boxes_.size() || pane_boxes_[graph_index].IsEmpty() || graph_content_box_.IsEmpty())
            return;
        const auto hit = std::find_if(node_hits_.begin(), node_hits_.end(),
                                      [&](const NodeHit &candidate) { return candidate.id == node_id; });
        if (hit == node_hits_.end())
            return;
        const auto &viewport = pane_boxes_[graph_index];
        const int   dx       = viewport.x_min - graph_content_box_.x_min;
        const int   dy       = viewport.y_min - graph_content_box_.y_min;
        const int   center_x = (hit->box.x_min + hit->box.x_max) / 2;
        const int   center_y = (hit->box.y_min + hit->box.y_max) / 2;
        graph_scroll_x_.set(center_x + dx - viewport.x_min);
        graph_scroll_y_.set(center_y + dy - viewport.y_min);
    }

    void cycle_node(int direction) {
        std::vector<std::string> ordered;
        try {
            ordered = editor_.document().node_ids_in_route_order();
        } catch (const std::exception &) {
            ordered.clear();
        }
        if (ordered.empty()) {
            const auto &nodes = editor_.document().nodes();
            ordered.reserve(nodes.size());
            for (const auto &node : nodes)
                ordered.push_back(node.id);
        }
        const auto found  = std::find(ordered.begin(), ordered.end(), selected_node_);
        const auto current = found == ordered.end() ? 0 : static_cast<int>(std::distance(ordered.begin(), found));
        const auto count   = static_cast<int>(ordered.size());
        selected_node_     = ordered[static_cast<std::size_t>((current + direction + count) % count)];
        selected_parameter_ = 0;
        scroll_node_into_view(selected_node_);
    }

    void cycle_route(int direction) {
        const auto &routes = editor_.document().routes();
        const auto  found  = selected_route_ ? std::find(routes.begin(), routes.end(), *selected_route_) : routes.end();
        const auto  current = found == routes.end() ? 0 : static_cast<int>(std::distance(routes.begin(), found));
        const auto  count   = static_cast<int>(routes.size());
        selected_route_     = routes[static_cast<std::size_t>((current + direction + count) % count)];
    }

    void cycle_pane(int direction) {
        const auto found   = std::find(kPanes.begin(), kPanes.end(), active_pane_);
        const auto current = found == kPanes.end() ? 0 : static_cast<int>(std::distance(kPanes.begin(), found));
        const auto count   = static_cast<int>(kPanes.size());
        active_pane_       = kPanes[static_cast<std::size_t>((current + direction + count) % count)];
        hovered_route_.reset();
    }

    void toggle_transport() {
        if (audio_.running()) {
            audio_.stop();
            transient_status_ = "Audio stopped";
        } else if (audio_.start()) {
            transient_status_ = audio_.muted() ? "Audio started muted; press m to monitor" : "Audio started";
        } else {
            transient_status_ = "Error: " + audio_.diagnostic();
        }
    }

    bool save() {
        try {
            editor_.save();
            transient_status_ = editor_.status();
            return true;
        } catch (const std::exception &error) {
            transient_status_ = "Error: " + std::string(error.what());
            return false;
        }
    }

    template <typename Function> void act(Function &&function) {
        try {
            function();
            transient_status_ = editor_.status();
            normalize_selection();
        } catch (const std::exception &error) { transient_status_ = "Error: " + std::string(error.what()); }
    }

    void trigger_debug_snapshot() {
        using namespace ftxui;
        const auto [width, height] = size_provider_();
        const int w = width > 0 ? width : 120;
        const int h = height > 0 ? height : 40;

        Screen screen(w, h);
        ftxui::Render(screen, this->Render());
        const std::string raw_screen   = screen.ToString();
        const std::string clean_screen = strip_ansi_codes(raw_screen);

        std::ostringstream ss;
        ss << "=== APG-TUI DEBUG SNAPSHOT ===\n";
        ss << "Active Pane: " << pane_name(active_pane_) << "\n";

        if (!selected_node_.empty()) {
            ss << "Selected Node ID: " << selected_node_ << "\n";
            const auto *node = editor_.document().find_node(selected_node_);
            if (node) {
                ss << "  Unit: " << node->unit << "\n";
                ss << "  Bypass: "
                   << (editor_.bypass().contains(selected_node_) && editor_.bypass().at(selected_node_) ? "true"
                                                                                                         : "false")
                   << "\n";
                if (!node->routing_section.empty())
                    ss << "  Routing Section: " << node->routing_section << "\n";
                if (!node->params.empty()) {
                    ss << "  Parameters:\n";
                    for (const auto &[k, v] : node->params) {
                        ss << "    " << k << ": " << v << "\n";
                    }
                }
            }
        }
        if (!selected_scene_name().empty())
            ss << "Selected Scene: " << selected_scene_name() << "\n";

        ss << "Graph Scroll: " << graph_scroll_x_.offset << " " << graph_scroll_y_.offset << "\n";
        const ftxui::Box viewport = pane_boxes_[static_cast<std::size_t>(Pane::Graph)];
        ss << "Graph Viewport: " << viewport.x_min << " " << viewport.y_min << " " << viewport.x_max << " "
           << viewport.y_max << "\n";
        const ftxui::Box pane_inner = ftxui::Box{viewport.x_min + 1, viewport.x_max - 1, viewport.y_min + 1,
                                                 viewport.y_max - 1};
        ss << "Graph Pane Box: " << pane_inner.x_min << " " << pane_inner.y_min << " " << pane_inner.x_max << " "
           << pane_inner.y_max << "\n";
        ss << "Graph Content Box: " << graph_content_box_.x_min << " " << graph_content_box_.y_min << " "
           << graph_content_box_.x_max << " " << graph_content_box_.y_max << "\n";
        if (!selected_node_.empty()) {
            const auto hit = std::find_if(node_hits_.begin(), node_hits_.end(),
                                          [&](const NodeHit &n) { return n.id == selected_node_; });
            if (hit != node_hits_.end()) {
                ss << "Selected Node Box: " << hit->box.x_min << " " << hit->box.y_min << " " << hit->box.x_max
                   << " " << hit->box.y_max << "\n";
            }
        }

        ss << "\n--- C++ EXPECTED TEST ASSERTIONS ---\n";
        if (!selected_node_.empty()) {
            ss << "assert(clean.find(\"" << selected_node_ << "\") != std::string::npos);\n";
        }
        ss << "/* Rendered Screen Dimension: " << w << "x" << h << " */\n";

        ss << "\n--- CLEAN SCREEN DUMP ---\n";
        ss << clean_screen << "\n";
        ss << "=================================\n";

        const std::string snapshot = ss.str();

        copy_to_clipboard_osc52(snapshot);

        std::ofstream out("apg-tui-debug.txt", std::ios::out | std::ios::app);
        if (out.is_open())
            out << snapshot << "\n";

        modal_text_       = snapshot;
        modal_            = Modal::Debug;
        transient_status_ = "[Debug] Snapshot copied to clipboard & saved to apg-tui-debug.txt";
    }

    std::string selected_scene_name() const {
        const auto &scenes = editor_.document().scenes();
        return scenes.empty() ? std::string{} : scenes[normalize_index(selected_scene_, scenes.size())].name;
    }

    std::string status_text() const { return transient_status_.empty() ? editor_.status() : transient_status_; }

    ProjectEditor              &editor_;
    AudioSession               &audio_;
    std::function<void()>       request_exit_;
    TerminalSizeProvider        size_provider_;
    bool                        debug_mode_  = false;
    Pane                        active_pane_ = Pane::Graph;
    Modal                       modal_       = Modal::None;
    std::string                 modal_text_;
    std::string                 selected_node_;
    std::optional<Route>        selected_route_;
    std::size_t                 selected_unit_      = 0;
    std::size_t                 selected_scene_     = 0;
    std::size_t                 selected_parameter_ = 0;
    std::string                 unit_search_;
    bool                        searching_ = false;
    std::string                 transient_status_;
    std::string                 render_error_;
    std::optional<ParameterHit> dragged_parameter_;
    std::optional<std::string>  dragged_node_;
    std::optional<std::string>  dragged_unit_;
    std::optional<std::string>  carried_unit_;
    std::optional<Route>        hovered_route_;
    std::deque<NodeHit>         node_hits_;
    std::deque<RouteHit>        route_hits_;
    std::deque<UnitHit>         unit_hits_;
    std::deque<SceneHit>        scene_hits_;
    std::deque<ParameterHit>    parameter_hits_;
    std::deque<TabHit>          tab_hits_;
    std::array<ftxui::Box, 6>   pane_boxes_{};
    ftxui::Box                  graph_content_box_{};
    ScrollState                 units_scroll_;
    ScrollState                 graph_scroll_x_;
    ScrollState                 graph_scroll_y_;
    ScrollState                 inspector_scroll_;
    ScrollState                 scenes_scroll_;
};

} // namespace

ftxui::Component studio_component(
    ProjectEditor        &editor,
    AudioSession         &audio,
    std::function<void()> request_exit,
    TerminalSizeProvider  size_provider,
    bool                  debug_mode
) {
    return std::make_shared<StudioComponent>(
        editor, audio, std::move(request_exit), std::move(size_provider), debug_mode
    );
}

} // namespace apg::terminal
