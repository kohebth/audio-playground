#ifndef APG_TERMINAL_SESSION_HPP
#define APG_TERMINAL_SESSION_HPP

#include "apg_terminal/project_document.hpp"

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

class AudioSession {
  public:
    virtual ~AudioSession() = default;

    virtual std::vector<AudioDeviceInfo> devices() const                            = 0;
    virtual AudioDeviceConfig            config() const                             = 0;
    virtual bool                         configure(const AudioDeviceConfig &config) = 0;
    virtual bool
    synchronize(const ApgPackageDocument &document, const std::map<std::string, bool> &bypass, bool structural) = 0;
    virtual bool          start()                                                                               = 0;
    virtual void          stop()                                                                                = 0;
    virtual bool          running() const                                                                       = 0;
    virtual void          set_mute(bool muted)                                                                  = 0;
    virtual bool          muted() const                                                                         = 0;
    virtual bool          set_param(const std::string &path, float value)                                       = 0;
    virtual bool          set_bypass(const std::string &instance, bool bypassed)                                = 0;
    virtual MeterSnapshot meter() const                                                                         = 0;
    virtual std::string   diagnostic() const                                                                    = 0;
    virtual void          service()                                                                             = 0;
};

class MiniaudioSession final : public AudioSession {
  public:
    MiniaudioSession();
    ~MiniaudioSession() override;

    MiniaudioSession(const MiniaudioSession &)            = delete;
    MiniaudioSession &operator=(const MiniaudioSession &) = delete;

    std::vector<AudioDeviceInfo> devices() const override;
    AudioDeviceConfig            config() const override;
    bool                         configure(const AudioDeviceConfig &config) override;
    bool                         synchronize(
                                const ApgPackageDocument &document, const std::map<std::string, bool> &bypass, bool structural
                            ) override;
    bool          start() override;
    void          stop() override;
    bool          running() const override;
    void          set_mute(bool muted) override;
    bool          muted() const override;
    bool          set_param(const std::string &path, float value) override;
    bool          set_bypass(const std::string &instance, bool bypassed) override;
    MeterSnapshot meter() const override;
    std::string   diagnostic() const override;
    void          service() override;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

class FakeAudioSession final : public AudioSession {
  public:
    FakeAudioSession();
    ~FakeAudioSession() override;

    FakeAudioSession(const FakeAudioSession &)            = delete;
    FakeAudioSession &operator=(const FakeAudioSession &) = delete;

    std::vector<AudioDeviceInfo> devices() const override;
    AudioDeviceConfig            config() const override;
    bool                         configure(const AudioDeviceConfig &config) override;
    bool                         synchronize(
                                const ApgPackageDocument &document, const std::map<std::string, bool> &bypass, bool structural
                            ) override;
    bool          start() override;
    void          stop() override;
    bool          running() const override;
    void          set_mute(bool muted) override;
    bool          muted() const override;
    bool          set_param(const std::string &path, float value) override;
    bool          set_bypass(const std::string &instance, bool bypassed) override;
    MeterSnapshot meter() const override;
    std::string   diagnostic() const override;
    void          service() override;

    bool               process_block(const float *input, float *output, std::uint32_t frames);
    [[nodiscard]] bool swap_in_flight() const;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace apg::terminal

#endif
