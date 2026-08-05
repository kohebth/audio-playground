#ifndef APG_TERMINAL_PARAMETER_ROW_HPP
#define APG_TERMINAL_PARAMETER_ROW_HPP

#include "ui.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

namespace apg::terminal {

class ParameterRow final {
  public:
    static std::size_t    KnobIndex(double ratio);
    static double         ClampedValueRatio(double value, double min, double max);
    static std::string    KnobSymbol(double ratio);
    static ftxui::Element Render(const ParameterItem &parameter, ftxui::Box &box);
};

} // namespace apg::terminal

#endif
