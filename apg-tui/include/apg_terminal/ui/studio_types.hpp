#ifndef APG_TERMINAL_STUDIO_TYPES_HPP
#define APG_TERMINAL_STUDIO_TYPES_HPP

#include "apg_terminal/domain/project_document.hpp"

#include <ftxui/screen/box.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <iomanip>
#include <sstream>
#include <string>

namespace apg::terminal {

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
    Debug,
};

constexpr std::array<Pane, 6> kPanes{
    Pane::Units, Pane::Graph, Pane::Inspector, Pane::Scenes, Pane::Audio, Pane::Problems,
};
constexpr int kGraphNodeHeight = 5;

inline const char *pane_name(Pane pane) {
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

inline void pop_utf8(std::string &value) {
    if (value.empty())
        return;
    std::size_t start = value.size() - 1;
    while (start > 0 && (static_cast<unsigned char>(value[start]) & 0xC0u) == 0x80u)
        --start;
    value.erase(start);
}

inline std::string format_value(const Parameter &parameter) {
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

inline double parameter_ratio(const Parameter &parameter) {
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

inline double ratio_value(const Parameter &parameter, double ratio) {
    ratio        = std::clamp(ratio, 0.0, 1.0);
    double value = 0.0;
    if (parameter.scale == ParameterScale::Logarithmic && parameter.min > 0.0) {
        value = std::exp(std::log(parameter.min) + ratio * (std::log(parameter.max) - std::log(parameter.min)));
    } else {
        value = parameter.min + ratio * (parameter.max - parameter.min);
    }
    return parameter.type == ParameterType::Integer ? std::round(value) : value;
}

struct ScrollState {
    int  offset = 0;
    void scroll(int delta) { offset = std::max(0, offset + delta); }
    void reset() { offset = 0; }
};

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

} // namespace apg::terminal

#endif // APG_TERMINAL_STUDIO_TYPES_HPP
