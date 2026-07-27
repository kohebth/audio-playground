#include "apg_terminal/editor.hpp"
#include "apg_terminal/project_document.hpp"
#include "apg_terminal/session.hpp"
#include "apg_terminal/studio.hpp"

#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {

int self_test() {
    apg::terminal::FakeAudioSession audio;
    if (audio.devices().size() != 2 || !audio.muted() || audio.running())
        return 1;
    auto config        = audio.config();
    config.sample_rate = 44100;
    if (!audio.configure(config) || audio.config().sample_rate != 44100)
        return 1;
    audio.set_mute(false);
    return audio.muted() ? 1 : 0;
}

void usage(std::ostream &output) {
    output << "Usage: apg-tui [--no-audio] PROJECT.apg\n"
              "       apg-tui --version\n"
              "       apg-tui --self-test\n";
}

} // namespace

int main(int argc, char **argv) {
    bool                  no_audio = false;
    std::filesystem::path package_path;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--version") {
            std::cout << "apg-tui 0.2.0\n";
            return 0;
        }
        if (argument == "--help" || argument == "-h") {
            usage(std::cout);
            return 0;
        }
        if (argument == "--self-test")
            return self_test();
        if (argument == "--no-audio") {
            no_audio = true;
            continue;
        }
        if (argument.starts_with('-')) {
            std::cerr << "apg-tui: unknown option \"" << argument << "\"\n";
            usage(std::cerr);
            return 2;
        }
        if (!package_path.empty()) {
            usage(std::cerr);
            return 2;
        }
        package_path = argument;
    }
    if (package_path.empty()) {
        usage(std::cerr);
        return 2;
    }

    std::optional<apg::terminal::ApgPackageDocument> document;
    try {
        document.emplace(apg::terminal::ApgPackageDocument::load(package_path));
    } catch (const std::exception &error) {
        std::cerr << "apg-tui: " << error.what() << '\n';
        return 1;
    }

    apg::terminal::ProjectEditor                 editor(std::move(*document));
    std::unique_ptr<apg::terminal::AudioSession> audio =
        no_audio ? std::unique_ptr<apg::terminal::AudioSession>(std::make_unique<apg::terminal::FakeAudioSession>())
                 : std::unique_ptr<apg::terminal::AudioSession>(std::make_unique<apg::terminal::MiniaudioSession>());

    auto              screen = ftxui::ScreenInteractive::Fullscreen();
    std::atomic<bool> finished{false};
    auto              studio = apg::terminal::studio_component(editor, *audio, [&] {
        finished.store(true, std::memory_order_release);
        screen.Exit();
    });
    std::jthread      refresh([&](std::stop_token stop) {
        while (!stop.stop_requested() && !finished.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if (!stop.stop_requested())
                screen.PostEvent(ftxui::Event::Custom);
        }
    });

    screen.Loop(studio);
    finished.store(true, std::memory_order_release);
    refresh.request_stop();
    audio->stop();
    return 0;
}
