#include "apg_terminal/session.hpp"

namespace apg::terminal {

bool NullAudioSession::start() {
    running_.store(true);
    return true;
}

void NullAudioSession::stop() { running_.store(false); }

MeterSnapshot NullAudioSession::meter() const { return {.peak = 0.0f, .rms = 0.0f, .frames = 0, .underruns = 0}; }

std::string NullAudioSession::diagnostic() const {
    return muted_.load() ? "null audio backend (muted)" : "null audio backend";
}

} // namespace apg::terminal
