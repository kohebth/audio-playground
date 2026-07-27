#include "apg_terminal/project_document.hpp"
#include "apg_terminal/session.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <new>
#include <string>
#include <vector>

namespace {

std::atomic<bool>        track_allocations{false};
std::atomic<std::size_t> callback_allocations{0};

std::string read_file(const std::string &path) {
    std::ifstream input(path, std::ios::binary);
    assert(input);
    return {(std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>()};
}

apg::terminal::ApgPackageDocument simple_package() {
    const auto             project = read_file("test/fixtures/projects-v2/simple-gain-board.project.v2.yaml");
    const auto             unit    = read_file("test/fixtures/units-v2/simple_gain.unit.v2.yaml");
    nlohmann::ordered_json package = {
        {   "schema","apg.project.package.v1"                     },
        {  "version",                               1},
        { "manifest",
         {
         {"id", "audio-test"},
         {"name", "Audio Test"},
         {"description", ""},
         {"createdAt", "2026-07-25T00:00:00.000Z"},
         {"updatedAt", "2026-07-25T00:00:00.000Z"},
         {"lastMode", "pro"},
         }                                           },
        {"workspace",
         {
         {"schema", "apg.ui.workspace.v2"},
         {"version", 2},
         {"entryProject", "projects-v2/simple-gain-board.project.v2.yaml"},
         {"files", nlohmann::ordered_json::array({
         {
         {"path", "projects-v2/simple-gain-board.project.v2.yaml"},
         {"role", "project"},
         {"content", project},
         },
         {
         {"path", "units-v2/simple_gain.unit.v2.yaml"},
         {"role", "unit"},
         {"content", unit},
         },
         })},
         }                                           },
        {    "audio", nlohmann::ordered_json::array()},
        {"readiness",
         {
         {"checkedAt", nullptr},
         {"validation", "unknown"},
         {"preview", "unknown"},
         {"targets", nlohmann::ordered_json::object()},
         {"diagnostics", nlohmann::ordered_json::array()},
         }                                           },
    };
    return apg::terminal::ApgPackageDocument::parse(package.dump(), "audio-test.apg");
}

} // namespace

void *operator new(std::size_t size) {
    if (track_allocations.load(std::memory_order_relaxed))
        callback_allocations.fetch_add(1, std::memory_order_relaxed);
    if (void *memory = std::malloc(size))
        return memory;
    throw std::bad_alloc();
}

void  operator delete(void *memory) noexcept { std::free(memory); }
void  operator delete(void *memory, std::size_t) noexcept { std::free(memory); }
void *operator new[](std::size_t size) {
    if (track_allocations.load(std::memory_order_relaxed))
        callback_allocations.fetch_add(1, std::memory_order_relaxed);
    if (void *memory = std::malloc(size))
        return memory;
    throw std::bad_alloc();
}
void operator delete[](void *memory) noexcept { std::free(memory); }
void operator delete[](void *memory, std::size_t) noexcept { std::free(memory); }

int main() {
    using apg::terminal::FakeAudioSession;
    using apg::terminal::Route;

    auto             document = simple_package();
    FakeAudioSession audio;
    assert(audio.devices().size() == 2);
    assert(audio.synchronize(
        document,
        {
            {"gain1", false}
    },
        true
    ));
    assert(audio.start());
    assert(audio.running());
    assert(audio.muted());

    std::vector<float> input(1536, 0.25f);
    std::vector<float> output(1536, 1.0f);
    assert(audio.process_block(input.data(), output.data(), 64));
    assert(std::all_of(output.begin(), output.begin() + 64, [](float value) { return value == 0.0f; }));

    audio.set_mute(false);
    for (int block = 0; block < 64; ++block)
        assert(audio.process_block(input.data(), output.data(), 64));
    assert(std::abs(output[63] - 0.5f) < 0.03f);

    assert(audio.set_param("gain1.gain", 1.0f));
    for (int block = 0; block < 64; ++block)
        assert(audio.process_block(input.data(), output.data(), 64));
    assert(std::abs(output[63] - 0.25f) < 0.03f);

    assert(audio.set_bypass("gain1", true));
    for (int block = 0; block < 8; ++block)
        assert(audio.process_block(input.data(), output.data(), 64));
    assert(std::abs(output[63] - 0.25f) < 0.01f);
    assert(!audio.set_param("missing.value", 1.0f));
    assert(!audio.set_bypass("missing", true));
    assert(!audio.process_block(input.data(), nullptr, 64));

    document.insert_on_route(Route{"gain1.output", "system.output"}, "gain_unit");
    assert(audio.synchronize(
        document,
        {
            {      "gain1", false},
            {"simple_gain", false}
    },
        true
    ));
    assert(audio.swap_in_flight());
    assert(audio.process_block(input.data(), output.data(), 32));
    assert(audio.swap_in_flight());
    assert(audio.process_block(input.data(), output.data(), 32));
    assert(!audio.swap_in_flight());
    audio.service();

    callback_allocations.store(0, std::memory_order_relaxed);
    track_allocations.store(true, std::memory_order_relaxed);
    const bool processed = audio.process_block(input.data(), output.data(), 1536);
    track_allocations.store(false, std::memory_order_relaxed);
    assert(processed);
    assert(callback_allocations.load(std::memory_order_relaxed) == 0);

    const auto meter = audio.meter();
    assert(meter.frames >= 1536);
    assert(meter.input_peak == 0.25f);
    assert(std::isfinite(meter.output_rms));
    std::fill(output.begin(), output.end(), 1.0f);
    assert(audio.process_block(nullptr, output.data(), 64));
    assert(std::all_of(output.begin(), output.begin() + 64, [](float value) { return std::abs(value) < 1e-6f; }));
    assert(!audio.configure({}));

    document.insert_on_route(Route{"simple_gain.output", "system.output"}, "gain_unit");
    assert(audio.synchronize(
        document,
        {
            {        "gain1", false},
            {  "simple_gain", false},
            {"simple_gain_2", false}
    },
        true
    ));
    assert(audio.swap_in_flight());
    audio.stop();
    assert(!audio.running());
    assert(!audio.swap_in_flight());
    assert(!audio.process_block(input.data(), output.data(), 64));

    auto config                   = audio.config();
    auto invalid_device           = config;
    invalid_device.capture_device = "playback:default";
    assert(!audio.configure(invalid_device));
    assert(audio.config().capture_device == config.capture_device);
    config.sample_rate    = 44100;
    config.maximum_frames = 256;
    assert(audio.configure(config));
    assert(audio.config().sample_rate == 44100);
    assert(audio.start());
    assert(audio.process_block(input.data(), output.data(), 1536));
    audio.stop();
    return 0;
}
