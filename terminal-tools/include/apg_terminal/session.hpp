#ifndef APG_TERMINAL_SESSION_HPP
#define APG_TERMINAL_SESSION_HPP

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace apg::terminal {

struct MeterSnapshot {
    float         peak      = 0.0f;
    float         rms       = 0.0f;
    std::uint64_t frames    = 0;
    std::uint64_t underruns = 0;
};

class AudioSession {
  public:
    virtual ~AudioSession()                                                      = default;
    virtual bool          start()                                                = 0;
    virtual void          stop()                                                 = 0;
    virtual bool          running() const                                        = 0;
    virtual void          set_mute(bool muted)                                   = 0;
    virtual void          set_bypass(const std::string &instance, bool bypassed) = 0;
    virtual MeterSnapshot meter() const                                          = 0;
    virtual std::string   diagnostic() const                                     = 0;
};

class NullAudioSession final : public AudioSession {
  public:
    bool          start() override;
    void          stop() override;
    bool          running() const override { return running_.load(); }
    void          set_mute(bool muted) override { muted_.store(muted); }
    void          set_bypass(const std::string &, bool) override {}
    MeterSnapshot meter() const override;
    std::string   diagnostic() const override;

  private:
    std::atomic<bool> running_ = false;
    std::atomic<bool> muted_   = false;
};

} // namespace apg::terminal

#endif
