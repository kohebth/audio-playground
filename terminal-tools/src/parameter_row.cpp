#include "apg_terminal/parameter_row.hpp"

#include <algorithm>
#include <cstdio>

#include <ftxui/dom/elements.hpp>

namespace apg::terminal {

namespace {

const char *kKnobChars = "○◴◑◷●◶◐◵";

} // namespace

std::size_t ParameterRow::KnobIndex(double ratio) {
    return std::clamp(static_cast<std::size_t>(ratio * 7.999), std::size_t{0}, std::size_t{7});
}

double ParameterRow::ClampedValueRatio(double value, double min, double max) {
    return max > min ? std::clamp((value - min) / (max - min), 0.0, 1.0) : 0.0;
}

std::string ParameterRow::KnobSymbol(double ratio) {
    return std::string(1, kKnobChars[KnobIndex(ratio)]);
}

ftxui::Element ParameterRow::Render(const ParameterItem &parameter, ftxui::Box &box) {
    using namespace ftxui;

    const auto  ratio = ClampedValueRatio(parameter.value, parameter.min, parameter.max);
    const auto  knob  = KnobSymbol(ratio);
    char       buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.3g", parameter.value);
    return text(" " + parameter.label + " " + knob + " " + std::string(buffer) +
                (parameter.unit.empty() ? "" : " " + parameter.unit)) |
           reflect(box);
}

} // namespace apg::terminal
