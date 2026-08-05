#include "apg_terminal/ui/ui.hpp"

#include <ftxui/dom/elements.hpp>

namespace apg::terminal {

ftxui::Element render_pipeline_ui(
    const ftxui::Component &pipeline,
    const ftxui::Component &controls,
    const ftxui::Component &parameters,
    const ftxui::Component &tray,
    const std::string      &selected,
    const std::string      &audio,
    const std::string      &status,
    const std::string      &validation
) {
    using namespace ftxui;

    return vbox({
               text("APG Terminal Pipeline") | bold | color(Color::Cyan),
               hbox({
                   vbox({text("Chain") | bold, pipeline->Render()}) | border,
                   vbox(
                       {text("Unit controls") | bold, parameters->Render() | frame | flex, separator(),
                        text("Actions") | bold, controls->Render()}
                   ) | border,
               }),
               separator(),
               vbox({text("Unit definitions") | bold, tray->Render() | frame}) | border,
               separator(),
               text("Selected: " + selected),
               text(
                   "Mouse: drag effect; yellow inserts before, magenta after. "
                   "Keyboard: Tab/arrows, Enter to toggle transport, q to quit."
               ),
               text("Audio: " + audio),
               text("Status: " + status),
               text("Validation: " + validation) | color(validation == "OK" ? Color::Green : Color::Red),
           }) |
           border;
}

} // namespace apg::terminal
