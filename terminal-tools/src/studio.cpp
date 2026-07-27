#include "apg_terminal/studio.hpp"

#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <deque>
#include <functional>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace apg::terminal {
namespace {

enum class Pane {
    Units,
    Graph,
    Inspector,
    Scenes,
    Audio,
    Problems,
};

enum class Modal {
    None,
    Help,
    Quit,
    NewScene,
    RenameScene,
    DeleteScene,
    DeleteNode,
};

constexpr std::array<Pane, 6> kPanes{
    Pane::Units, Pane::Graph, Pane::Inspector, Pane::Scenes, Pane::Audio, Pane::Problems,
};
constexpr int kGraphNodeHeight = 5;

const char *pane_name(Pane pane) {
    switch (pane) {
    case Pane::Units:
        return "Units";
    case Pane::Graph:
        return "Graph";
    case Pane::Inspector:
        return "Inspector";
    case Pane::Scenes:
        return "Scenes";
    case Pane::Audio:
        return "Audio";
    case Pane::Problems:
        return "Problems";
    }
    return "";
}

void pop_utf8(std::string &value) {
    if (value.empty())
        return;
    std::size_t start = value.size() - 1;
    while (start > 0 && (static_cast<unsigned char>(value[start]) & 0xC0u) == 0x80u)
        --start;
    value.erase(start);
}

std::string format_value(const Parameter &parameter) {
    std::ostringstream value;
    if (parameter.type == ParameterType::Integer) {
        value << static_cast<long long>(std::llround(parameter.value));
    } else {
        value << std::fixed << std::setprecision(std::max(0, parameter.precision)) << parameter.value;
    }
    if (!parameter.unit.empty())
        value << ' ' << parameter.unit;
    return value.str();
}

double parameter_ratio(const Parameter &parameter) {
    if (parameter.max <= parameter.min)
        return 0.0;
    if (parameter.scale == ParameterScale::Logarithmic && parameter.min > 0.0 && parameter.value > 0.0) {
        return std::clamp(
            (std::log(parameter.value) - std::log(parameter.min)) / (std::log(parameter.max) - std::log(parameter.min)),
            0.0, 1.0
        );
    }
    return std::clamp((parameter.value - parameter.min) / (parameter.max - parameter.min), 0.0, 1.0);
}

double ratio_value(const Parameter &parameter, double ratio) {
    ratio        = std::clamp(ratio, 0.0, 1.0);
    double value = 0.0;
    if (parameter.scale == ParameterScale::Logarithmic && parameter.min > 0.0) {
        value = std::exp(std::log(parameter.min) + ratio * (std::log(parameter.max) - std::log(parameter.min)));
    } else {
        value = parameter.min + ratio * (parameter.max - parameter.min);
    }
    return parameter.type == ParameterType::Integer ? std::round(value) : value;
}

struct NodeHit {
    std::string id;
    ftxui::Box  box;
};

struct RouteHit {
    Route      route;
    ftxui::Box box;
};

struct UnitHit {
    std::string id;
    ftxui::Box  box;
};

struct SceneHit {
    std::string name;
    ftxui::Box  box;
};

struct ParameterHit {
    std::string node;
    std::string parameter;
    Parameter   spec;
    ftxui::Box  box;
};

struct TabHit {
    Pane       pane;
    ftxui::Box box;
};

template <typename Hit> const Hit *hit_at(const std::deque<Hit> &hits, int x, int y) {
    const auto found = std::find_if(hits.begin(), hits.end(), [&](const Hit &hit) {
        return !hit.box.IsEmpty() && hit.box.Contain(x, y);
    });
    return found == hits.end() ? nullptr : &*found;
}

class StudioComponent final : public ftxui::ComponentBase {
  public:
    StudioComponent(
        ProjectEditor        &editor,
        AudioSession         &audio,
        std::function<void()> request_exit,
        TerminalSizeProvider  size_provider
    )
        : editor_(editor), audio_(audio), request_exit_(std::move(request_exit)),
          size_provider_(std::move(size_provider)) {
        if (!size_provider_) {
            size_provider_ = [] {
                const auto dimensions = ftxui::Terminal::Size();
                return std::pair{dimensions.dimx, dimensions.dimy};
            };
        }
        normalize_selection();
        editor_.set_change_callback(
            [this](const ApgPackageDocument &document, const std::map<std::string, bool> &bypass, bool structural) {
                if (audio_.synchronize(document, bypass, structural))
                    return;
                if (structural && audio_.running()) {
                    const bool was_muted = audio_.muted();
                    audio_.stop();
                    if (audio_.synchronize(document, bypass, true)) {
                        audio_.set_mute(was_muted);
                        if (audio_.start())
                            transient_status_ = "Audio graph resynchronized after a pending swap";
                        else
                            transient_status_ = "Error: " + audio_.diagnostic();
                        return;
                    }
                }
                transient_status_ = "Audio: " + audio_.diagnostic();
            }
        );
        if (!audio_.synchronize(editor_.document(), editor_.bypass(), true))
            transient_status_ = "Audio: " + audio_.diagnostic();
    }

    ~StudioComponent() override { editor_.set_change_callback({}); }

    ftxui::Element OnRender() override {
        using namespace ftxui;

        clear_hits();
        normalize_selection();
        const auto [width, height] = size_provider_();
        const bool dirty           = editor_.dirty();
        auto       header          = hbox({
            text(" APG TUI ") | bold | color(Color::Cyan),
            separator(),
            text(" " + editor_.document().name() + (dirty ? " *" : "")) | flex,
            text(audio_.running() ? (audio_.muted() ? " RUN/MUTED " : " RUN/LIVE ") : " STOPPED ") |
                color(audio_.running() && !audio_.muted() ? Color::Green : Color::Yellow),
        });

        const auto shortcuts = width >= 120 ? " Tab panes · r route · Enter activate · Space transport · b bypass · m "
                                              "mute · Ctrl+S save · ? help · q quit"
                                            : " Tab panes · Space audio · Ctrl+S save · ? help · q quit";
        auto footer = vbox({
            text(" " + status_text()) | color(transient_status_.starts_with("Error:") ? Color::Red : Color::GrayLight),
            text(shortcuts) | dim,
        });

        Element body;
        if (width < 80 || height < 24) {
            body = vbox({
                       filler(),
                       text("Terminal too small") | bold | center,
                       text("Resize to at least 80×24. Ctrl+S and q remain available.") | center,
                       filler(),
                   }) |
                   border | flex;
        } else if (width >= 120 && height >= 32) {
            body = render_wide();
        } else {
            body = render_compact();
        }

        auto page = vbox({
                        header | size(HEIGHT, EQUAL, 1),
                        body | flex,
                        footer | size(HEIGHT, EQUAL, 2),
                    }) |
                    border;
        return render_modal(std::move(page));
    }

    bool OnEvent(ftxui::Event event) override {
        using namespace ftxui;

        if (event == Event::Custom) {
            audio_.service();
            return true;
        }
        if (modal_ != Modal::None)
            return handle_modal(event);
        if (searching_)
            return handle_search(event);

        if (event == Event::CtrlS) {
            save();
            return true;
        }
        if (event == Event::CtrlZ) {
            act([&] { editor_.undo(); });
            return true;
        }
        if (event == Event::CtrlY) {
            act([&] { editor_.redo(); });
            return true;
        }
        if (event == Event::Character("?") || event == Event::F1) {
            modal_ = Modal::Help;
            return true;
        }
        if (event == Event::Escape && carried_unit_) {
            cancel_unit_placement();
            transient_status_ = "Placement cancelled";
            return true;
        }
        if (event == Event::Character("q") || event == Event::Escape) {
            if (editor_.dirty())
                modal_ = Modal::Quit;
            else
                request_exit_();
            return true;
        }
        if (event == Event::Character(" ")) {
            toggle_transport();
            return true;
        }
        if (event == Event::Character("m")) {
            audio_.set_mute(!audio_.muted());
            transient_status_ = audio_.muted() ? "Monitoring muted" : "Monitoring live";
            return true;
        }
        if (event == Event::Tab || event == Event::TabReverse) {
            cycle_pane(event == Event::Tab ? 1 : -1);
            return true;
        }
        if (event.is_mouse())
            return handle_mouse(event);

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

    bool Focusable() const override { return true; }

  private:
    void clear_hits() {
        node_hits_.clear();
        route_hits_.clear();
        unit_hits_.clear();
        scene_hits_.clear();
        parameter_hits_.clear();
        tab_hits_.clear();
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

    ftxui::Element pane_frame(const std::string &title, ftxui::Element content, Pane pane) const {
        using namespace ftxui;
        auto element = window(text(" " + title + " ") | bold, std::move(content));
        if (active_pane_ == pane)
            element = element | color(Color::Cyan);
        return element;
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

    ftxui::Element render_units() {
        using namespace ftxui;
        const auto units = placeable_units();
        Elements   rows;
        rows.push_back(
            text(
                searching_      ? "Search: " + unit_search_ + "▌"
                : wide_layout() ? "/ search · drag to signal"
                                : "/ search · click to place"
            ) |
            dim
        );
        if (units.empty()) {
            rows.push_back(text("No matching package units") | dim);
        } else {
            for (std::size_t index = 0; index < units.size(); ++index) {
                const auto *unit = units[index];
                unit_hits_.push_back({unit->id, {}});
                auto card = vbox({
                                text(unit->title.empty() ? unit->id : unit->title) | bold,
                                text(unit->id + " · " + unit->category) | dim,
                            }) |
                            border;
                if (index == selected_unit_) {
                    card = card | color(Color::Cyan);
                    if (active_pane_ == Pane::Units)
                        card = card | focus;
                }
                if ((dragged_unit_ && *dragged_unit_ == unit->id) || (carried_unit_ && *carried_unit_ == unit->id))
                    card = card | dim;
                rows.push_back(card | reflect(unit_hits_.back().box));
            }
        }
        return vbox(std::move(rows)) | yframe | flex;
    }

    ftxui::Element render_graph() {
        using namespace ftxui;
        render_error_.clear();
        try {
            const auto topology = editor_.document().topology();
            return vbox({
                       render_sequence(topology, 0),
                       filler(),
                   }) |
                   xframe | yframe | flex;
        } catch (const std::exception &error) {
            render_error_ = error.what();
            return vbox({
                       text("Unable to render route topology") | bold | color(Color::Red),
                       paragraph(error.what()),
                   }) |
                   flex;
        }
    }

    ftxui::Element render_sequence(const TopologySequence &sequence, int depth) {
        using namespace ftxui;
        if (depth > 32)
            return text("nesting limit") | color(Color::Red);
        Elements items;
        for (std::size_t index = 0; index < sequence.routes.size(); ++index) {
            items.push_back(render_route(sequence.routes[index]));
            if (index >= sequence.elements.size())
                continue;
            const auto &element = sequence.elements[index];
            if (element.kind == TopologyElement::Kind::Effect) {
                items.push_back(render_node(element.node_id, false));
            } else if (element.parallel) {
                Elements paths;
                for (const auto &path : element.parallel->paths) {
                    paths.push_back(
                        hbox({
                            text(" " + path.name + " ") | bold,
                            path.sequence ? render_sequence(*path.sequence, depth + 1) : text("disconnected"),
                        }) |
                        border
                    );
                }
                items.push_back(
                    hbox({
                        render_node(element.parallel->panner_id, true),
                        text("≋") | vcenter,
                        vbox(std::move(paths)),
                        text("≋") | vcenter,
                        render_node(element.parallel->mixer_id, true),
                    }) |
                    vcenter
                );
            }
        }
        return hbox(std::move(items));
    }

    ftxui::Element render_route(const Route &route) {
        using namespace ftxui;
        route_hits_.push_back({route, {}});
        const bool hovered  = hovered_route_ && *hovered_route_ == route;
        const bool selected = selected_route_ && *selected_route_ == route;
        auto       element  = text(hovered || selected ? "──◆──" : "──◇──");
        if (hovered) {
            element = element | color(Color::Green) | bold;
        } else if (route_drop_active()) {
            element = element | color(Color::Cyan);
        } else if (selected) {
            element = element | color(Color::Yellow) | bold;
            if (active_pane_ == Pane::Graph)
                element = element | focus;
        } else {
            element = element | color(Color::GrayDark);
        }
        return (element | reflect(route_hits_.back().box)) | vcenter;
    }

    ftxui::Element render_node(const std::string &node_id, bool helper) {
        using namespace ftxui;
        node_hits_.push_back({node_id, {}});
        const auto *node    = editor_.document().find_node(node_id);
        const auto *unit    = node ? editor_.document().find_unit(node->unit) : nullptr;
        const auto  label   = unit && !unit->title.empty() ? unit->title : node_id;
        auto        element = vbox({
                           text(label) | bold,
                           filler(),
                           text(
                               node_id + (helper                      ? " · routing"
                                                 : editor_.bypassed(node_id) ? " · BYPASS"
                                                                             : "")
                           ) | dim,
                       }) |
                       border | size(HEIGHT, EQUAL, kGraphNodeHeight);
        if (selected_node_ == node_id) {
            element = element | color(Color::Cyan);
            if (active_pane_ == Pane::Graph)
                element = element | focus;
        }
        if (!helper && editor_.bypassed(node_id))
            element = element | dim;
        if (dragged_node_ && *dragged_node_ == node_id)
            element = element | dim;
        return (element | reflect(node_hits_.back().box)) | vcenter;
    }

    ftxui::Element render_inspector() {
        using namespace ftxui;
        const auto *node = editor_.document().find_node(selected_node_);
        if (!node)
            return text("Select a node in the graph") | dim;
        const auto *unit = editor_.document().find_unit(node->unit);
        Elements    rows{
            text(node->id) | bold | color(Color::Cyan),
            text(unit ? unit->title : node->unit),
            text(
                node->routing_helper()       ? "Always active routing helper"
                : editor_.bypassed(node->id) ? "Bypassed · b enables"
                                             : "Enabled · b bypasses"
            ) | dim,
            separator(),
        };
        if (node->parameter_specs.empty())
            rows.push_back(text("No public parameters") | dim);
        for (std::size_t index = 0; index < node->parameter_specs.size(); ++index) {
            const auto &parameter = node->parameter_specs[index];
            parameter_hits_.push_back({node->id, parameter.name, parameter, {}});
            auto row = vbox({
                           hbox({
                               text(parameter.label) | flex,
                               text(format_value(parameter)),
                           }),
                           gauge(static_cast<float>(parameter_ratio(parameter))) | color(Color::Green),
                       }) |
                       border;
            if (index == selected_parameter_) {
                row = row | color(Color::Cyan);
                if (active_pane_ == Pane::Inspector)
                    row = row | focus;
            }
            rows.push_back(row | reflect(parameter_hits_.back().box));
        }
        if (!node->routing_helper())
            rows.push_back(text("b bypass · d remove · arrows/Page keys adjust") | dim);
        return vbox(std::move(rows)) | yframe | flex;
    }

    ftxui::Element render_scenes() {
        using namespace ftxui;
        Elements    rows;
        const auto &scenes = editor_.document().scenes();
        if (scenes.empty()) {
            rows.push_back(text("No scenes · n captures the current sound") | dim);
        } else {
            Elements chips;
            for (std::size_t index = 0; index < scenes.size(); ++index) {
                const auto &scene = scenes[index];
                scene_hits_.push_back({scene.name, {}});
                const bool active = editor_.active_scene() && *editor_.active_scene() == scene.name;
                auto       chip   = vbox({
                                text(scene.name + (active && editor_.scene_modified() ? " *" : "")) | bold,
                                text(
                                    std::to_string(scene.params.size()) + " controls · " +
                                    std::to_string(scene.bypass.size()) + " switches"
                                ) | dim,
                            }) |
                            border;
                if (index == selected_scene_) {
                    chip = chip | color(Color::Cyan);
                    if (active_pane_ == Pane::Scenes)
                        chip = chip | focus;
                }
                chips.push_back(chip | reflect(scene_hits_.back().box));
            }
            rows.push_back(hbox(std::move(chips)) | xframe);
        }
        rows.push_back(text("Enter recall · n new · u update · e rename · d delete") | dim);
        return vbox(std::move(rows)) | flex;
    }

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
                gauge(std::min(1.0f, meter.input_peak)) | flex | color(Color::Blue),
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
        using namespace ftxui;
        if (modal_ == Modal::None)
            return page;
        Element     content;
        std::string title;
        switch (modal_) {
        case Modal::Help:
            title   = "Help";
            content = vbox({
                text("Tab/Shift-Tab switch panes; arrows navigate the active pane."),
                text("Graph: r cycles routes, x moves the effect, c collapses an empty parallel section."),
                text("Units: drag onto a signal line; compact clicks carry a unit to Graph. Enter inserts."),
                text("Inspector: arrows adjust, Page keys coarse, Home/End bounds, b bypass, d remove."),
                text("Scenes: Enter recall, n create, u update, e rename, d delete."),
                text("Audio: Space transport, m mute. Ctrl+S save, Ctrl+Z/Y history, q guarded quit."),
                separator(),
                text("Press Escape or Enter to close.") | dim,
            });
            break;
        case Modal::Quit:
            title   = "Unsaved changes";
            content = vbox({
                text("Save before leaving?"),
                text("[s] Save and quit   [d] Discard   [c/Esc] Cancel") | bold,
            });
            break;
        case Modal::NewScene:
            title   = "Capture scene";
            content = vbox({
                text("Name: " + modal_text_ + "▌"),
                text("Enter saves · Escape cancels") | dim,
            });
            break;
        case Modal::RenameScene:
            title   = "Rename scene";
            content = vbox({
                text("Name: " + modal_text_ + "▌"),
                text("Enter renames · Escape cancels") | dim,
            });
            break;
        case Modal::DeleteScene:
            title   = "Delete scene";
            content = vbox({
                text("Delete \"" + selected_scene_name() + "\"?"),
                text("[y] Delete   [n/Esc] Cancel") | bold,
            });
            break;
        case Modal::DeleteNode:
            title   = "Remove effect";
            content = vbox({
                text("Remove \"" + selected_node_ + "\" and bridge its route?"),
                text("[y] Remove   [n/Esc] Cancel") | bold,
            });
            break;
        case Modal::None:
            break;
        }
        auto dialog = window(text(" " + title + " ") | bold, std::move(content)) | size(WIDTH, LESS_THAN, 72) |
                      clear_under | center;
        return dbox({std::move(page), dialog});
    }

    bool handle_modal(const ftxui::Event &event) {
        using namespace ftxui;
        if (modal_ == Modal::Help) {
            if (event == Event::Escape || event == Event::Return || event == Event::Character("?"))
                modal_ = Modal::None;
            return true;
        }
        if (modal_ == Modal::Quit) {
            if (event == Event::Character("s")) {
                if (save()) {
                    modal_ = Modal::None;
                    request_exit_();
                }
            } else if (event == Event::Character("d")) {
                modal_ = Modal::None;
                request_exit_();
            } else if (event == Event::Character("c") || event == Event::Escape) {
                modal_ = Modal::None;
            }
            return true;
        }
        if (modal_ == Modal::DeleteScene || modal_ == Modal::DeleteNode) {
            if (event == Event::Character("y")) {
                if (modal_ == Modal::DeleteScene) {
                    const auto name = selected_scene_name();
                    act([&] { editor_.remove_scene(name); });
                } else {
                    const auto id = selected_node_;
                    act([&] { editor_.remove_node(id); });
                }
                modal_ = Modal::None;
            } else if (event == Event::Character("n") || event == Event::Escape) {
                modal_ = Modal::None;
            }
            return true;
        }
        if (event == Event::Escape) {
            modal_ = Modal::None;
            modal_text_.clear();
            return true;
        }
        if (event == Event::Backspace) {
            pop_utf8(modal_text_);
            return true;
        }
        if (event == Event::Return) {
            const auto value = modal_text_;
            if (modal_ == Modal::NewScene)
                act([&] { editor_.save_scene(value); });
            else if (modal_ == Modal::RenameScene) {
                const auto current = selected_scene_name();
                act([&] { editor_.rename_scene(current, value); });
            }
            modal_ = Modal::None;
            modal_text_.clear();
            return true;
        }
        if (event.is_character() && event.character().size() <= 4 && modal_text_.size() < 64) {
            modal_text_ += event.character();
            return true;
        }
        return true;
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

    bool handle_mouse(ftxui::Event event) {
        using namespace ftxui;
        const auto &mouse = event.mouse();
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
        if (const auto *parameter = hit_at(parameter_hits_, mouse.x, mouse.y)) {
            selected_node_   = parameter->node;
            const auto *node = editor_.document().find_node(parameter->node);
            if (node) {
                const auto found = std::find_if(
                    node->parameter_specs.begin(), node->parameter_specs.end(),
                    [&](const Parameter &item) { return item.name == parameter->parameter; }
                );
                if (found != node->parameter_specs.end())
                    selected_parameter_ = static_cast<std::size_t>(std::distance(node->parameter_specs.begin(), found));
            }
            active_pane_       = Pane::Inspector;
            dragged_parameter_ = *parameter;
            return true;
        }
        return false;
    }

    bool handle_units(const ftxui::Event &event) {
        using namespace ftxui;
        const auto units = placeable_units();
        if (event == Event::Character("/")) {
            searching_ = true;
            return true;
        }
        if (units.empty())
            return false;
        if (event == Event::ArrowUp || event == Event::Character("k")) {
            selected_unit_ = selected_unit_ == 0 ? units.size() - 1 : selected_unit_ - 1;
            return true;
        }
        if (event == Event::ArrowDown || event == Event::Character("j")) {
            selected_unit_ = (selected_unit_ + 1) % units.size();
            return true;
        }
        if (event == Event::Return) {
            if (!selected_route_) {
                transient_status_ = "Error: select a graph route first";
            } else {
                const auto route = *selected_route_;
                const auto unit  = units[selected_unit_]->id;
                act([&] { selected_node_ = editor_.insert_on_route(route, unit); });
            }
            return true;
        }
        if (event == Event::Character("p")) {
            if (!selected_route_) {
                transient_status_ = "Error: select a graph route first";
            } else {
                const auto route = *selected_route_;
                const auto unit  = units[selected_unit_]->id;
                act([&] { selected_node_ = editor_.add_parallel_on_route(route, unit); });
            }
            return true;
        }
        return false;
    }

    bool handle_graph(const ftxui::Event &event) {
        using namespace ftxui;
        const auto &nodes  = editor_.document().nodes();
        const auto &routes = editor_.document().routes();
        if ((event == Event::ArrowLeft || event == Event::Character("h")) && !nodes.empty()) {
            cycle_node(-1);
            return true;
        }
        if ((event == Event::ArrowRight || event == Event::Character("l")) && !nodes.empty()) {
            cycle_node(1);
            return true;
        }
        if ((event == Event::Character("r") || event == Event::ArrowDown) && !routes.empty()) {
            cycle_route(1);
            return true;
        }
        if (event == Event::ArrowUp && !routes.empty()) {
            cycle_route(-1);
            return true;
        }
        if (event == Event::Return) {
            active_pane_ = Pane::Inspector;
            return true;
        }
        if (event == Event::Character("x") && selected_route_ && !selected_node_.empty()) {
            const auto node        = selected_node_;
            const auto destination = *selected_route_;
            act([&] { editor_.move_to_route(node, destination); });
            return true;
        }
        if (event == Event::Character("c") && !selected_node_.empty()) {
            const auto *node = editor_.document().find_node(selected_node_);
            if (!node || node->routing_section.empty()) {
                transient_status_ = "Error: select a routing helper from an empty parallel section";
            } else {
                const auto section = node->routing_section;
                act([&] { editor_.collapse_parallel(section); });
            }
            return true;
        }
        return false;
    }

    bool handle_inspector(const ftxui::Event &event) {
        using namespace ftxui;
        const auto *node = editor_.document().find_node(selected_node_);
        if (!node)
            return false;
        if (event == Event::Character("b")) {
            act([&] { editor_.toggle_bypass(node->id); });
            return true;
        }
        if (event == Event::Character("d") && !node->routing_helper()) {
            modal_ = Modal::DeleteNode;
            return true;
        }
        if (node->parameter_specs.empty())
            return false;
        if (event == Event::ArrowUp || event == Event::Character("k")) {
            selected_parameter_ = selected_parameter_ == 0 ? node->parameter_specs.size() - 1 : selected_parameter_ - 1;
            return true;
        }
        if (event == Event::ArrowDown || event == Event::Character("j")) {
            selected_parameter_ = (selected_parameter_ + 1) % node->parameter_specs.size();
            return true;
        }
        auto parameter = node->parameter_specs[selected_parameter_];
        if (event == Event::Home) {
            act([&] { editor_.set_param(node->id, parameter.name, parameter.min); });
            return true;
        }
        if (event == Event::End) {
            act([&] { editor_.set_param(node->id, parameter.name, parameter.max); });
            return true;
        }
        double delta = 0.0;
        if (event == Event::ArrowLeft || event == Event::Character("h"))
            delta = -0.01;
        else if (event == Event::ArrowRight || event == Event::Character("l"))
            delta = 0.01;
        else if (event == Event::PageDown)
            delta = -0.10;
        else if (event == Event::PageUp)
            delta = 0.10;
        if (delta != 0.0) {
            const auto ratio = parameter_ratio(parameter);
            act([&] { editor_.set_param(node->id, parameter.name, ratio_value(parameter, ratio + delta)); });
            return true;
        }
        return false;
    }

    bool handle_scenes(const ftxui::Event &event) {
        using namespace ftxui;
        const auto &scenes = editor_.document().scenes();
        if ((event == Event::ArrowLeft || event == Event::ArrowUp || event == Event::Character("h")) &&
            !scenes.empty()) {
            selected_scene_ = selected_scene_ == 0 ? scenes.size() - 1 : selected_scene_ - 1;
            return true;
        }
        if ((event == Event::ArrowRight || event == Event::ArrowDown || event == Event::Character("l")) &&
            !scenes.empty()) {
            selected_scene_ = (selected_scene_ + 1) % scenes.size();
            return true;
        }
        if (event == Event::Return && !scenes.empty()) {
            const auto name = scenes[selected_scene_].name;
            act([&] { editor_.recall_scene(name); });
            return true;
        }
        if (event == Event::Character("n")) {
            modal_text_.clear();
            modal_ = Modal::NewScene;
            return true;
        }
        if (event == Event::Character("u") && !scenes.empty()) {
            const auto name = scenes[selected_scene_].name;
            act([&] { editor_.save_scene(name, true); });
            return true;
        }
        if (event == Event::Character("e") && !scenes.empty()) {
            modal_text_ = scenes[selected_scene_].name;
            modal_      = Modal::RenameScene;
            return true;
        }
        if (event == Event::Character("d") && !scenes.empty()) {
            modal_ = Modal::DeleteScene;
            return true;
        }
        return false;
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

    void cycle_node(int direction) {
        const auto &nodes = editor_.document().nodes();
        const auto  found =
            std::find_if(nodes.begin(), nodes.end(), [&](const Node &node) { return node.id == selected_node_; });
        const auto current  = found == nodes.end() ? 0 : static_cast<int>(std::distance(nodes.begin(), found));
        const auto count    = static_cast<int>(nodes.size());
        selected_node_      = nodes[static_cast<std::size_t>((current + direction + count) % count)].id;
        selected_parameter_ = 0;
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

    std::string selected_scene_name() const {
        const auto &scenes = editor_.document().scenes();
        return scenes.empty() ? std::string{} : scenes[normalize_index(selected_scene_, scenes.size())].name;
    }

    std::string status_text() const { return transient_status_.empty() ? editor_.status() : transient_status_; }

    ProjectEditor              &editor_;
    AudioSession               &audio_;
    std::function<void()>       request_exit_;
    TerminalSizeProvider        size_provider_;
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
};

} // namespace

ftxui::Component studio_component(
    ProjectEditor &editor, AudioSession &audio, std::function<void()> request_exit, TerminalSizeProvider size_provider
) {
    return std::make_shared<StudioComponent>(editor, audio, std::move(request_exit), std::move(size_provider));
}

} // namespace apg::terminal
