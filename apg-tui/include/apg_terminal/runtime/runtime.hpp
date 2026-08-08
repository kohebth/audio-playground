#ifndef APG_TERMINAL_RUNTIME_HPP
#define APG_TERMINAL_RUNTIME_HPP

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace apg::terminal {

enum class AudioDeviceKind {
    Capture,
    Playback,
};

struct AudioDeviceInfo {
    std::string     id;
    std::string     name;
    AudioDeviceKind kind       = AudioDeviceKind::Capture;
    bool            is_default = false;
};

struct AudioDeviceConfig {
    std::string   capture_device;
    std::string   playback_device;
    std::uint32_t sample_rate    = 48000;
    std::uint32_t period_frames  = 256;
    std::uint32_t maximum_frames = 1024;
};

struct MeterSnapshot {
    float         input_peak       = 0.0f;
    float         input_rms        = 0.0f;
    float         output_peak      = 0.0f;
    float         output_rms       = 0.0f;
    std::uint64_t frames           = 0;
    std::uint64_t process_failures = 0;
    bool          clipped          = false;
};

struct RuntimeProjectParam {
    std::string path;
    float       value = 0.0f;
};

// The compiled-input read model that the realtime engine consumes. It is built
// by the application/io layer from the domain aggregate; the runtime never
// touches ApgPackageDocument, YAML, or the filesystem beyond reading the
// already-materialized project entry.
struct RuntimeProjectSpec {
    std::string                      root_path;
    std::string                      entry_path;
    AudioDeviceConfig                config;
    std::vector<RuntimeProjectParam> params;
    std::vector<std::string>         instances;
    std::map<std::string, bool>      bypass;
};

class ApgEngine {
  public:
    explicit ApgEngine(AudioDeviceConfig config);
    ~ApgEngine();

    ApgEngine(const ApgEngine &)            = delete;
    ApgEngine &operator=(const ApgEngine &) = delete;
    ApgEngine(ApgEngine &&)                 = delete;
    ApgEngine &operator=(ApgEngine &&)      = delete;

    bool                            configure(const AudioDeviceConfig &config);
    [[nodiscard]] AudioDeviceConfig config() const;

    bool synchronize(const RuntimeProjectSpec &spec, bool structural, std::string &diagnostic);

    bool               set_param(const std::string &path, float value);
    bool               set_bypass(const std::string &instance, bool bypassed);
    void               set_mute(bool muted);
    [[nodiscard]] bool muted() const;
    void               set_running(bool running);
    [[nodiscard]] bool running() const;

    bool process(const float *input, float *output, std::uint32_t frames) noexcept;
    void service();
    void settle_stopped();

    [[nodiscard]] MeterSnapshot meter() const;
    [[nodiscard]] bool          swap_in_flight() const;
    [[nodiscard]] bool          has_active_graph() const;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace apg::terminal

#endif
