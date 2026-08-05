#ifndef APG_TERMINAL_STUDIO_HPP
#define APG_TERMINAL_STUDIO_HPP

#include "apg_terminal/application/editor.hpp"
#include "apg_terminal/io/session.hpp"

#include <ftxui/component/component.hpp>

#include <functional>
#include <utility>

namespace apg::terminal {

using TerminalSizeProvider = std::function<std::pair<int, int>()>;

ftxui::Component studio_component(
    ProjectEditor        &editor,
    AudioSession         &audio,
    std::function<void()> request_exit,
    TerminalSizeProvider  size_provider = {}
);

} // namespace apg::terminal

#endif
