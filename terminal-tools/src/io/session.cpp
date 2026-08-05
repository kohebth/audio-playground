#include "apg_terminal/io/session.hpp"

#include <miniaudio.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace apg::terminal {
namespace {

constexpr unsigned kNotificationUnexpectedStop = 1u << 0;
constexpr unsigned kNotificationRerouted       = 1u << 1;
constexpr unsigned kNotificationInterrupted    = 1u << 2;
constexpr unsigned kNotificationResumed        = 1u << 3;

bool default_device_id(const std::string &id, AudioDeviceKind kind) {
    return id.empty() || id == (kind == AudioDeviceKind::Capture ? "capture:default" : "playback:default");
}

std::string temporary_suffix() {
    static std::atomic<std::uint64_t> counter{0};
    std::random_device                random;
    return std::to_string(random()) + "-" + std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
}

class AudioWorkspace {
  public:
    explicit AudioWorkspace(const ApgPackageDocument &document) {
        for (int attempt = 0; attempt < 32; ++attempt) {
            root_ = std::filesystem::temp_directory_path() / ("apg-tui-audio-" + temporary_suffix());
            std::error_code error;
            if (!std::filesystem::create_directory(root_, error))
                continue;
            std::filesystem::permissions(
                root_, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace, error
            );
            if (error) {
                std::filesystem::remove_all(root_, error);
                continue;
            }
            try {
                entry_ = document.materialize_to(root_);
            } catch (...) {
                std::filesystem::remove_all(root_, error);
                throw;
            }
            return;
        }
        throw std::runtime_error("Unable to create a private audio compilation workspace.");
    }

    AudioWorkspace(const AudioWorkspace &)            = delete;
    AudioWorkspace &operator=(const AudioWorkspace &) = delete;

    ~AudioWorkspace() {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    [[nodiscard]] const std::filesystem::path &root() const { return root_; }
    [[nodiscard]] const std::filesystem::path &entry() const { return entry_; }

  private:
    std::filesystem::path root_;
    std::filesystem::path entry_;
};

struct SessionCore {
    explicit SessionCore(AudioDeviceConfig config) : engine(config), config(std::move(config)) {}

    bool synchronize(
        const ApgPackageDocument &next_document, const std::map<std::string, bool> &next_bypass, bool structural
    ) {
        const bool                    needs_compile = structural || !engine.has_active_graph();
        std::optional<AudioWorkspace> workspace;
        RuntimeProjectSpec            spec;
        spec.config = config;
        spec.bypass = next_bypass;
        if (needs_compile) {
            workspace.emplace(next_document);
            spec.root_path  = workspace->root().string();
            spec.entry_path = workspace->entry().string();
        }
        for (const auto &node : next_document.nodes()) {
            for (const auto &parameter : node.parameter_specs)
                spec.params.push_back({node.id + "." + parameter.name, static_cast<float>(parameter.value)});
            if (!node.routing_helper())
                spec.instances.push_back(node.id);
        }
        if (!engine.synchronize(spec, structural, diagnostic))
            return false;
        document   = next_document;
        bypass     = next_bypass;
        diagnostic = "runtime ready";
        return true;
    }

    RealtimeProjectEngine             engine;
    AudioDeviceConfig                 config;
    std::optional<ApgPackageDocument> document;
    std::map<std::string, bool>       bypass;
    std::string                       diagnostic = "no project loaded";
};

struct DeviceRecord {
    AudioDeviceInfo info;
    ma_device_id    id{};
};

} // namespace

class MiniaudioSession::Impl {
  public:
    Impl() : core(AudioDeviceConfig{}) {
        const ma_result result = ma_context_init(nullptr, 0, nullptr, &context);
        if (result != MA_SUCCESS) {
            context_diagnostic = std::string("miniaudio context failed: ") + ma_result_description(result);
            diagnostic         = context_diagnostic;
            return;
        }
        context_ready = true;
        refresh_devices();
    }

    ~Impl() {
        stop();
        if (context_ready)
            ma_context_uninit(&context);
    }

    void refresh_devices() {
        if (!context_ready)
            return;
        ma_device_info *playback_infos = nullptr;
        ma_uint32       playback_count = 0;
        ma_device_info *capture_infos  = nullptr;
        ma_uint32       capture_count  = 0;
        const ma_result result =
            ma_context_get_devices(&context, &playback_infos, &playback_count, &capture_infos, &capture_count);
        if (result != MA_SUCCESS) {
            diagnostic = std::string("device enumeration failed: ") + ma_result_description(result);
            return;
        }
        devices.clear();
        devices.push_back({
            {"capture:default", "System default capture", AudioDeviceKind::Capture, true},
            {},
        });
        for (ma_uint32 index = 0; index < capture_count; ++index) {
            devices.push_back({
                {"capture:" + std::to_string(index), capture_infos[index].name, AudioDeviceKind::Capture,
                 capture_infos[index].isDefault != 0},
                capture_infos[index].id,
            });
        }
        devices.push_back({
            {"playback:default", "System default playback", AudioDeviceKind::Playback, true},
            {},
        });
        for (ma_uint32 index = 0; index < playback_count; ++index) {
            devices.push_back({
                {"playback:" + std::to_string(index), playback_infos[index].name, AudioDeviceKind::Playback,
                 playback_infos[index].isDefault != 0},
                playback_infos[index].id,
            });
        }
    }

    const ma_device_id *device_id(const std::string &id, AudioDeviceKind kind) const {
        if (default_device_id(id, kind))
            return nullptr;
        const auto found = std::find_if(devices.begin(), devices.end(), [&](const DeviceRecord &record) {
            return record.info.id == id && record.info.kind == kind;
        });
        return found == devices.end() ? nullptr : &found->id;
    }

    bool start() {
        if (running)
            return true;
        if (!context_ready) {
            diagnostic = context_diagnostic;
            return false;
        }
        if (!core.document) {
            diagnostic = "no project loaded";
            return false;
        }
        if (!core.synchronize(*core.document, core.bypass, true)) {
            diagnostic = core.diagnostic;
            return false;
        }

        stop_requested.store(false, std::memory_order_release);
        notification_flags.store(0, std::memory_order_release);
        auto config                 = ma_device_config_init(ma_device_type_duplex);
        config.playback.format      = ma_format_f32;
        config.playback.channels    = 1;
        config.playback.pDeviceID   = device_id(core.config.playback_device, AudioDeviceKind::Playback);
        config.capture.format       = ma_format_f32;
        config.capture.channels     = 1;
        config.capture.pDeviceID    = device_id(core.config.capture_device, AudioDeviceKind::Capture);
        config.sampleRate           = core.config.sample_rate;
        config.periodSizeInFrames   = core.config.period_frames;
        config.dataCallback         = data_callback;
        config.notificationCallback = notification_callback;
        config.pUserData            = this;

        ma_result result = ma_device_init(&context, &config, &device);
        if (result != MA_SUCCESS) {
            diagnostic = std::string("audio device initialization failed: ") + ma_result_description(result);
            return false;
        }
        device_ready = true;
        core.engine.set_running(true);
        result = ma_device_start(&device);
        if (result != MA_SUCCESS) {
            core.engine.set_running(false);
            core.engine.settle_stopped();
            ma_device_uninit(&device);
            device_ready = false;
            notification_flags.store(0, std::memory_order_release);
            diagnostic = std::string("audio device start failed: ") + ma_result_description(result);
            return false;
        }
        running    = true;
        diagnostic = core.engine.muted() ? "monitoring started (muted)" : "monitoring started";
        return true;
    }

    void stop() {
        stop_requested.store(true, std::memory_order_release);
        if (device_ready) {
            (void)ma_device_stop(&device);
            core.engine.set_running(false);
            core.engine.settle_stopped();
            ma_device_uninit(&device);
            device_ready = false;
        }
        running = false;
        notification_flags.store(0, std::memory_order_release);
        if (context_ready)
            diagnostic = "audio stopped";
    }

    void service() {
        core.engine.service();
        const unsigned notifications = notification_flags.exchange(0, std::memory_order_acq_rel);
        if ((notifications & kNotificationUnexpectedStop) != 0u) {
            stop_requested.store(true, std::memory_order_release);
            core.engine.set_running(false);
            core.engine.settle_stopped();
            if (device_ready) {
                ma_device_uninit(&device);
                device_ready = false;
            }
            running    = false;
            diagnostic = "audio device stopped or was disconnected";
            refresh_devices();
            return;
        }
        if ((notifications & kNotificationRerouted) != 0u) {
            refresh_devices();
            diagnostic = "audio device rerouted";
        }
        if ((notifications & kNotificationInterrupted) != 0u)
            diagnostic = "audio device interrupted";
        if ((notifications & kNotificationResumed) != 0u)
            diagnostic = core.engine.muted() ? "monitoring resumed (muted)" : "monitoring resumed";
    }

    static void data_callback(ma_device *device, void *output, const void *input, ma_uint32 frame_count) {
        auto *self = static_cast<Impl *>(device->pUserData);
        if (!self || !output)
            return;
        if (!self->core.engine.process(static_cast<const float *>(input), static_cast<float *>(output), frame_count)) {
            std::fill_n(static_cast<float *>(output), frame_count, 0.0f);
        }
    }

    static void notification_callback(const ma_device_notification *notification) {
        if (!notification || !notification->pDevice)
            return;
        auto *self = static_cast<Impl *>(notification->pDevice->pUserData);
        if (!self)
            return;
        unsigned flag = 0;
        switch (notification->type) {
        case ma_device_notification_type_stopped:
            if (!self->stop_requested.load(std::memory_order_acquire))
                flag = kNotificationUnexpectedStop;
            break;
        case ma_device_notification_type_rerouted:
            flag = kNotificationRerouted;
            break;
        case ma_device_notification_type_interruption_began:
            flag = kNotificationInterrupted;
            break;
        case ma_device_notification_type_interruption_ended:
            flag = kNotificationResumed;
            break;
        case ma_device_notification_type_started:
        case ma_device_notification_type_unlocked:
            break;
        }
        if (flag != 0u)
            self->notification_flags.fetch_or(flag, std::memory_order_release);
    }

    SessionCore               core;
    ma_context                context{};
    ma_device                 device{};
    bool                      context_ready = false;
    bool                      device_ready  = false;
    bool                      running       = false;
    std::atomic<bool>         stop_requested{true};
    std::atomic<unsigned>     notification_flags{0};
    std::vector<DeviceRecord> devices;
    std::string               context_diagnostic = "miniaudio context unavailable";
    std::string               diagnostic         = context_diagnostic;
};

MiniaudioSession::MiniaudioSession() : impl_(std::make_unique<Impl>()) {}

MiniaudioSession::~MiniaudioSession() = default;

std::vector<AudioDeviceInfo> MiniaudioSession::devices() const {
    std::vector<AudioDeviceInfo> result;
    result.reserve(impl_->devices.size());
    for (const auto &device : impl_->devices)
        result.push_back(device.info);
    return result;
}

AudioDeviceConfig MiniaudioSession::config() const { return impl_->core.config; }

bool MiniaudioSession::configure(const AudioDeviceConfig &config) {
    if (impl_->running) {
        impl_->diagnostic = "Stop audio before changing device settings.";
        return false;
    }
    if (config.maximum_frames == 0 || config.period_frames == 0 || config.sample_rate == 0) {
        impl_->diagnostic = "Audio rate and frame sizes must be non-zero.";
        return false;
    }
    const auto device_exists = [&](const std::string &id, AudioDeviceKind kind) {
        return default_device_id(id, kind) ||
               std::any_of(impl_->devices.begin(), impl_->devices.end(), [&](const DeviceRecord &device) {
                   return device.info.id == id && device.info.kind == kind;
               });
    };
    if (!device_exists(config.capture_device, AudioDeviceKind::Capture) ||
        !device_exists(config.playback_device, AudioDeviceKind::Playback)) {
        impl_->diagnostic = "Selected audio device is no longer available.";
        return false;
    }
    const auto previous = impl_->core.config;
    if (!impl_->core.engine.configure(config)) {
        impl_->diagnostic = "Invalid audio configuration.";
        return false;
    }
    impl_->core.config = config;
    if (impl_->core.document && !impl_->core.synchronize(*impl_->core.document, impl_->core.bypass, true)) {
        const auto failure = impl_->core.diagnostic;
        (void)impl_->core.engine.configure(previous);
        impl_->core.config = previous;
        impl_->diagnostic  = failure;
        return false;
    }
    impl_->diagnostic = impl_->context_ready ? "audio configuration ready" : impl_->context_diagnostic;
    return true;
}

bool MiniaudioSession::synchronize(
    const ApgPackageDocument &document, const std::map<std::string, bool> &bypass, bool structural
) {
    const bool result = impl_->core.synchronize(document, bypass, structural);
    impl_->diagnostic = impl_->context_ready ? impl_->core.diagnostic : impl_->context_diagnostic;
    return result;
}

bool MiniaudioSession::start() { return impl_->start(); }
void MiniaudioSession::stop() { impl_->stop(); }
bool MiniaudioSession::running() const { return impl_->running; }
void MiniaudioSession::set_mute(bool muted) {
    impl_->core.engine.set_mute(muted);
    impl_->diagnostic =
        impl_->context_ready ? (muted ? "monitoring muted" : "monitoring live") : impl_->context_diagnostic;
}
bool MiniaudioSession::muted() const { return impl_->core.engine.muted(); }
bool MiniaudioSession::set_param(const std::string &path, float value) {
    return impl_->core.engine.set_param(path, value);
}
bool MiniaudioSession::set_bypass(const std::string &instance, bool bypassed) {
    return impl_->core.engine.set_bypass(instance, bypassed);
}
MeterSnapshot MiniaudioSession::meter() const { return impl_->core.engine.meter(); }
std::string   MiniaudioSession::diagnostic() const { return impl_->diagnostic; }
void          MiniaudioSession::service() { impl_->service(); }

class FakeAudioSession::Impl {
  public:
    Impl() : core(AudioDeviceConfig{}) {}

    SessionCore core;
    bool        running = false;
};

FakeAudioSession::FakeAudioSession() : impl_(std::make_unique<Impl>()) {}

FakeAudioSession::~FakeAudioSession() { stop(); }

std::vector<AudioDeviceInfo> FakeAudioSession::devices() const {
    return {
        { "capture:fake",  "Deterministic fake capture",  AudioDeviceKind::Capture, true},
        {"playback:fake", "Deterministic fake playback", AudioDeviceKind::Playback, true},
    };
}

AudioDeviceConfig FakeAudioSession::config() const { return impl_->core.config; }

bool FakeAudioSession::configure(const AudioDeviceConfig &config) {
    if (impl_->running) {
        impl_->core.diagnostic = "Stop fake audio before changing device settings.";
        return false;
    }
    if (!default_device_id(config.capture_device, AudioDeviceKind::Capture) &&
        config.capture_device != "capture:fake") {
        impl_->core.diagnostic = "Selected fake capture device is unavailable.";
        return false;
    }
    if (!default_device_id(config.playback_device, AudioDeviceKind::Playback) &&
        config.playback_device != "playback:fake") {
        impl_->core.diagnostic = "Selected fake playback device is unavailable.";
        return false;
    }
    const auto previous = impl_->core.config;
    if (!impl_->core.engine.configure(config)) {
        impl_->core.diagnostic = "Invalid fake audio configuration.";
        return false;
    }
    impl_->core.config = config;
    if (!impl_->core.document || impl_->core.synchronize(*impl_->core.document, impl_->core.bypass, true))
        return true;
    (void)impl_->core.engine.configure(previous);
    impl_->core.config = previous;
    return false;
}

bool FakeAudioSession::synchronize(
    const ApgPackageDocument &document, const std::map<std::string, bool> &bypass, bool structural
) {
    return impl_->core.synchronize(document, bypass, structural);
}

bool FakeAudioSession::start() {
    if (!impl_->core.document)
        return false;
    impl_->running = true;
    impl_->core.engine.set_running(true);
    impl_->core.diagnostic = impl_->core.engine.muted() ? "fake audio running (muted)" : "fake audio running";
    return true;
}

void FakeAudioSession::stop() {
    impl_->running = false;
    impl_->core.engine.set_running(false);
    impl_->core.engine.settle_stopped();
    impl_->core.diagnostic = "fake audio stopped";
}

bool FakeAudioSession::running() const { return impl_->running; }
void FakeAudioSession::set_mute(bool muted) { impl_->core.engine.set_mute(muted); }
bool FakeAudioSession::muted() const { return impl_->core.engine.muted(); }
bool FakeAudioSession::set_param(const std::string &path, float value) {
    return impl_->core.engine.set_param(path, value);
}
bool FakeAudioSession::set_bypass(const std::string &instance, bool bypassed) {
    return impl_->core.engine.set_bypass(instance, bypassed);
}
MeterSnapshot FakeAudioSession::meter() const { return impl_->core.engine.meter(); }
std::string   FakeAudioSession::diagnostic() const { return impl_->core.diagnostic; }
void          FakeAudioSession::service() { impl_->core.engine.service(); }

bool FakeAudioSession::process_block(const float *input, float *output, std::uint32_t frames) {
    if (!impl_->running)
        return false;
    return impl_->core.engine.process(input, output, frames);
}

bool FakeAudioSession::swap_in_flight() const { return impl_->core.engine.swap_in_flight(); }

} // namespace apg::terminal
