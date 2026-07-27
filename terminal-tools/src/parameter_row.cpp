#include "apg_terminal/parameter_row.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string_view>

#include <ftxui/dom/elements.hpp>

namespace apg::terminal {

namespace {

constexpr std::array<std::string_view, 8> kKnobChars{"○", "◴", "◑", "◷", "●", "◶", "◐", "◵"};

} // namespace

std::size_t ParameterRow::KnobIndex(double ratio) {
    if (!std::isfinite(ratio))
        return 0;
    return static_cast<std::size_t>(std::clamp(ratio, 0.0, 1.0) * 7.999);
}

double ParameterRow::ClampedValueRatio(double value, double min, double max) {
    if (!std::isfinite(value) || !std::isfinite(min) || !std::isfinite(max) || max <= min)
        return 0.0;
    return std::clamp((value - min) / (max - min), 0.0, 1.0);
}

std::string ParameterRow::KnobSymbol(double ratio) { return std::string(kKnobChars[KnobIndex(ratio)]); }

ftxui::Element ParameterRow::Render(const ParameterItem &parameter, ftxui::Box &box) {
    using namespace ftxui;

    const auto ratio = ClampedValueRatio(parameter.value, parameter.min, parameter.max);
    const auto knob  = KnobSymbol(ratio);
    char       buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.3g", parameter.value);
    return text(
               " " + parameter.label + " " + knob + " " + std::string(buffer) +
               (parameter.unit.empty() ? "" : " " + parameter.unit)
           ) |
           reflect(box);
}

} // namespace apg::terminal
