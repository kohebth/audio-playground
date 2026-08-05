#include "apg_terminal/runtime/runtime.hpp"

extern "C" {
#include <apgcore/compiler/project_compiler_v2.h>
#include <apgcore/registry/registry_builder_v2.h>
#include <apgcore/runtime/runtime_v2.h>
#include <apgcore/validator/project_v2.h>
#include <yaml/arena.h>
#include <yaml/error.h>
}

#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace apg::terminal {
namespace {

constexpr std::uint32_t kSwapCrossfadeFrames = 64;

std::uint32_t float_bits(float value) { return std::bit_cast<std::uint32_t>(value); }
float         bits_float(std::uint32_t value) { return std::bit_cast<float>(value); }

struct AtomicControl {
    std::atomic<std::uint32_t> bits{float_bits(0.0f)};
    std::atomic<std::uint64_t> sequence{1};

    void set(float value) {
        bits.store(float_bits(value), std::memory_order_relaxed);
        sequence.fetch_add(1, std::memory_order_release);
    }

    [[nodiscard]] float get() const { return bits_float(bits.load(std::memory_order_relaxed)); }
};

class ControlStore {
  public:
    AtomicControl *ensure_param(const std::string &name, float value) { return ensure(params_, name, value); }

    AtomicControl *ensure_bypass(const std::string &name, bool value) {
        return ensure(bypass_, name, value ? 1.0f : 0.0f);
    }

    bool set_param(const std::string &name, float value) {
        const auto found = params_.find(name);
        if (found == params_.end())
            return false;
        found->second->set(value);
        return true;
    }

    bool set_bypass(const std::string &name, bool value) {
        const auto found = bypass_.find(name);
        if (found == bypass_.end())
            return false;
        found->second->set(value ? 1.0f : 0.0f);
        return true;
    }

    void                         set_mute(bool value) { mute_.set(value ? 1.0f : 0.0f); }
    [[nodiscard]] bool           muted() const { return mute_.get() >= 0.5f; }
    [[nodiscard]] AtomicControl *mute() { return &mute_; }

    void synchronize(const RuntimeProjectSpec &spec) {
        for (const auto &param : spec.params) {
            ensure_param(param.path, param.value)->set(param.value);
        }
        for (const auto &instance : spec.instances) {
            const auto bypassed = spec.bypass.find(instance);
            ensure_bypass(instance, bypassed != spec.bypass.end() && bypassed->second)
                ->set(bypassed != spec.bypass.end() && bypassed->second ? 1.0f : 0.0f);
        }
    }

  private:
    using ControlMap = std::unordered_map<std::string, std::unique_ptr<AtomicControl>>;

    static AtomicControl *ensure(ControlMap &controls, const std::string &name, float value) {
        const auto found = controls.find(name);
        if (found != controls.end())
            return found->second.get();
        auto control = std::make_unique<AtomicControl>();
        control->bits.store(float_bits(value), std::memory_order_relaxed);
        auto *result = control.get();
        controls.emplace(name, std::move(control));
        return result;
    }

    ControlMap    params_;
    ControlMap    bypass_;
    AtomicControl mute_;
};

struct ParamBinding {
    std::size_t    index            = 0;
    AtomicControl *control          = nullptr;
    std::uint64_t  applied_sequence = 0;
};

struct BypassBinding {
    std::size_t    index            = 0;
    AtomicControl *control          = nullptr;
    std::uint64_t  applied_sequence = 0;
};

class CompiledProjectGraph {
  public:
    static std::unique_ptr<CompiledProjectGraph>
    create(const RuntimeProjectSpec &spec, ControlStore &controls, std::string &diagnostic) {
        auto result = std::unique_ptr<CompiledProjectGraph>(new CompiledProjectGraph());
        // The compiled plan retains names owned by the resolved project, so both
        // arenas must outlive registry and runtime construction and processing.
        if (uc_arena_init(&result->resolved_arena_, 16u * 1024u * 1024u) != 0) {
            diagnostic = "Unable to allocate resolved-project arena.";
            return nullptr;
        }

        apg_project_v2_resolved_t resolved{};
        uc_error                  error{};
        uc_status                 status = apg_project_v2_load_resolved_file_with_root(
            spec.entry_path.c_str(), spec.root_path.c_str(), &result->resolved_arena_, &resolved, &error
        );
        if (status == UC_OK) {
            if (uc_arena_init(&result->compiled_arena_, 16u * 1024u * 1024u) != 0) {
                diagnostic = "Unable to allocate compiled-project arena.";
                return nullptr;
            }
            status = apg_project_v2_compile(&resolved, &result->compiled_arena_, &result->compiled_, &error);
        }
        if (status != UC_OK) {
            diagnostic = error.msg[0] != '\0' ? error.msg : uc_status_str(status);
            return nullptr;
        }

        const apg_prepare_context_t context = {
            .maximum_frames = spec.config.maximum_frames,
            .sample_rate    = static_cast<float>(spec.config.sample_rate),
        };
        status = apg_v2_registry_build_with_growth(
            &result->compiled_.plan, &context, &result->registry_arena_, &result->registry_, &error
        );
        if (status == UC_OK)
            status = apg_v2_runtime_create_from_registry(&result->registry_, &result->runtime_, &error);
        if (status != UC_OK) {
            diagnostic = error.msg[0] != '\0' ? error.msg : uc_status_str(status);
            return nullptr;
        }

        for (std::size_t index = 0; index < result->registry_.params_len; ++index) {
            const char *name = result->registry_.param_names[index];
            if (!name)
                continue;
            auto *control = controls.ensure_param(name, result->registry_.param_defaults[index]);
            if (!apg_v2_runtime_set_param_index(result->runtime_, index, control->get())) {
                diagnostic = "Unable to initialize runtime parameter " + std::string(name) + ".";
                return nullptr;
            }
            result->params_.push_back({
                index,
                control,
                control->sequence.load(std::memory_order_acquire),
            });
        }
        for (std::size_t index = 0; index < result->registry_.bypassed_instances_len; ++index) {
            const auto &entry = result->registry_.bypass_instances[index];
            if (!entry.instance_id)
                continue;
            const std::string name(entry.instance_id, entry.instance_id_len);
            auto             *control = controls.ensure_bypass(name, false);
            if (!apg_v2_runtime_set_instance_bypass_index(result->runtime_, index, control->get() >= 0.5f)) {
                diagnostic = "Unable to initialize runtime bypass " + name + ".";
                return nullptr;
            }
            result->bypass_.push_back({
                index,
                control,
                control->sequence.load(std::memory_order_acquire),
            });
        }
        result->mute_                  = controls.mute();
        result->applied_mute_sequence_ = result->mute_->sequence.load(std::memory_order_acquire);
        (void)apg_v2_runtime_set_project_mute(result->runtime_, result->mute_->get() >= 0.5f);

        for (std::size_t index = 0; index < result->registry_.input_audio_ports_len; ++index) {
            if (result->registry_.input_audio_ports[index].channel_count == 1) {
                result->input_port_ = index;
                break;
            }
        }
        for (std::size_t index = 0; index < result->registry_.output_audio_ports_len; ++index) {
            if (result->registry_.output_audio_ports[index].channel_count == 1) {
                result->output_port_ = index;
                break;
            }
        }
        if (!result->input_port_ || !result->output_port_) {
            diagnostic = "Project preview requires one public mono input and output.";
            return nullptr;
        }
        diagnostic.clear();
        return result;
    }

    CompiledProjectGraph(const CompiledProjectGraph &)            = delete;
    CompiledProjectGraph &operator=(const CompiledProjectGraph &) = delete;

    ~CompiledProjectGraph() {
        if (runtime_)
            apg_v2_runtime_destroy_owned(&runtime_);
        if (registry_arena_.base)
            uc_arena_free(&registry_arena_);
        if (compiled_arena_.base)
            uc_arena_free(&compiled_arena_);
        if (resolved_arena_.base)
            uc_arena_free(&resolved_arena_);
    }

    bool process(const float *input, float *output, std::uint32_t frames) noexcept {
        apply_controls();
        return apg_v2_runtime_process_mono_port_indices(
            runtime_, *input_port_, apg_const_buffer_make(input, frames), *output_port_,
            apg_buffer_make(output, frames), frames
        );
    }

  private:
    CompiledProjectGraph() = default;

    void apply_controls() noexcept {
        for (auto &binding : params_) {
            const auto sequence = binding.control->sequence.load(std::memory_order_acquire);
            if (sequence == binding.applied_sequence)
                continue;
            (void)apg_v2_runtime_set_param_index(runtime_, binding.index, binding.control->get());
            binding.applied_sequence = sequence;
        }
        for (auto &binding : bypass_) {
            const auto sequence = binding.control->sequence.load(std::memory_order_acquire);
            if (sequence == binding.applied_sequence)
                continue;
            (void)apg_v2_runtime_set_instance_bypass_index(runtime_, binding.index, binding.control->get() >= 0.5f);
            binding.applied_sequence = sequence;
        }
        const auto mute_sequence = mute_->sequence.load(std::memory_order_acquire);
        if (mute_sequence != applied_mute_sequence_) {
            (void)apg_v2_runtime_set_project_mute(runtime_, mute_->get() >= 0.5f);
            applied_mute_sequence_ = mute_sequence;
        }
    }

    uc_arena                   resolved_arena_{};
    uc_arena                   compiled_arena_{};
    apg_project_v2_compiled_t  compiled_{};
    uc_arena                   registry_arena_{};
    apg_v2_registry_t          registry_{};
    apg_v2_runtime_t          *runtime_ = nullptr;
    std::optional<std::size_t> input_port_;
    std::optional<std::size_t> output_port_;
    std::vector<ParamBinding>  params_;
    std::vector<BypassBinding> bypass_;
    AtomicControl             *mute_                  = nullptr;
    std::uint64_t              applied_mute_sequence_ = 0;
};

} // namespace

class RealtimeProjectEngine::Impl {
  public:
    explicit Impl(AudioDeviceConfig config)
        : config_(std::move(config)), zero_input_(config_.maximum_frames, 0.0f),
          old_output_(config_.maximum_frames, 0.0f), new_output_(config_.maximum_frames, 0.0f) {
        controls_.set_mute(true);
    }

    ~Impl() {
        set_running(false);
        settle_stopped();
        active_.store(nullptr, std::memory_order_release);
        graphs_.clear();
    }

    bool configure(const AudioDeviceConfig &config) {
        std::scoped_lock lock(control_mutex_);
        if (running_.load(std::memory_order_acquire) || config.maximum_frames == 0 || config.sample_rate == 0 ||
            config.period_frames == 0)
            return false;
        config_ = config;
        zero_input_.assign(config_.maximum_frames, 0.0f);
        old_output_.assign(config_.maximum_frames, 0.0f);
        new_output_.assign(config_.maximum_frames, 0.0f);
        return true;
    }

    [[nodiscard]] AudioDeviceConfig config() const {
        std::scoped_lock lock(control_mutex_);
        return config_;
    }

    bool synchronize(const RuntimeProjectSpec &spec, bool structural, std::string &diagnostic) {
        std::scoped_lock lock(control_mutex_);
        collect_retired_locked();
        controls_.synchronize(spec);
        if (!structural && active_.load(std::memory_order_acquire)) {
            diagnostic.clear();
            return true;
        }
        if (swap_in_flight_.load(std::memory_order_acquire)) {
            diagnostic = "A graph swap is still finishing; wait for the next audio block.";
            return false;
        }
        auto graph = CompiledProjectGraph::create(spec, controls_, diagnostic);
        if (!graph)
            return false;
        auto *pointer = graph.get();
        graphs_.emplace(pointer, std::move(graph));

        if (!running_.load(std::memory_order_acquire)) {
            auto *old = active_.exchange(pointer, std::memory_order_acq_rel);
            if (old)
                graphs_.erase(old);
            diagnostic.clear();
            return true;
        }
        CompiledProjectGraph *expected = nullptr;
        if (!pending_.compare_exchange_strong(
                expected, pointer, std::memory_order_release, std::memory_order_relaxed
            )) {
            graphs_.erase(pointer);
            diagnostic = "A prepared graph is already waiting for the audio thread.";
            return false;
        }
        swap_in_flight_.store(true, std::memory_order_release);
        diagnostic.clear();
        return true;
    }

    bool set_param(const std::string &path, float value) {
        return std::isfinite(value) && controls_.set_param(path, value);
    }

    bool set_bypass(const std::string &instance, bool bypassed) { return controls_.set_bypass(instance, bypassed); }

    void               set_mute(bool muted) { controls_.set_mute(muted); }
    [[nodiscard]] bool muted() const { return controls_.muted(); }

    void               set_running(bool running) { running_.store(running, std::memory_order_release); }
    [[nodiscard]] bool running() const { return running_.load(std::memory_order_acquire); }

    bool process(const float *input, float *output, std::uint32_t frames) noexcept {
        if (!output)
            return false;
        adopt_pending();
        float         input_peak  = 0.0f;
        float         input_sum   = 0.0f;
        float         output_peak = 0.0f;
        float         output_sum  = 0.0f;
        bool          ok          = true;
        std::uint32_t offset      = 0;
        while (offset < frames) {
            const auto  count        = std::min(config_.maximum_frames, frames - offset);
            const auto *chunk_input  = input ? input + offset : zero_input_.data();
            auto       *chunk_output = output + offset;
            auto       *active       = active_.load(std::memory_order_acquire);
            if (!active) {
                std::fill_n(chunk_output, count, 0.0f);
                ok = false;
            } else if (!fade_old_) {
                if (!active->process(chunk_input, chunk_output, count)) {
                    std::fill_n(chunk_output, count, 0.0f);
                    ok = false;
                }
            } else {
                const bool old_ok = fade_old_->process(chunk_input, old_output_.data(), count);
                const bool new_ok = active->process(chunk_input, new_output_.data(), count);
                if (!old_ok || !new_ok) {
                    std::fill_n(chunk_output, count, 0.0f);
                    ok = false;
                } else {
                    const auto mixed = std::min(count, crossfade_remaining_);
                    for (std::uint32_t index = 0; index < count; ++index) {
                        if (index < mixed) {
                            const float position = static_cast<float>(crossfade_offset_ + index + 1) /
                                                   static_cast<float>(kSwapCrossfadeFrames);
                            chunk_output[index] =
                                old_output_[index] * (1.0f - position) + new_output_[index] * position;
                        } else {
                            chunk_output[index] = new_output_[index];
                        }
                    }
                    crossfade_offset_ += mixed;
                    crossfade_remaining_ -= mixed;
                    if (crossfade_remaining_ == 0)
                        retire_fade_graph();
                }
            }

            for (std::uint32_t index = 0; index < count; ++index) {
                const float input_value  = chunk_input[index];
                const float output_value = chunk_output[index];
                input_peak               = std::max(input_peak, std::abs(input_value));
                output_peak              = std::max(output_peak, std::abs(output_value));
                input_sum += input_value * input_value;
                output_sum += output_value * output_value;
            }
            offset += count;
        }

        meter_input_peak_.store(float_bits(input_peak), std::memory_order_relaxed);
        meter_output_peak_.store(float_bits(output_peak), std::memory_order_relaxed);
        meter_input_rms_.store(
            float_bits(frames == 0 ? 0.0f : std::sqrt(input_sum / static_cast<float>(frames))),
            std::memory_order_relaxed
        );
        meter_output_rms_.store(
            float_bits(frames == 0 ? 0.0f : std::sqrt(output_sum / static_cast<float>(frames))),
            std::memory_order_relaxed
        );
        processed_frames_.fetch_add(frames, std::memory_order_relaxed);
        clipped_.store(output_peak >= 1.0f, std::memory_order_relaxed);
        if (!ok)
            process_failures_.fetch_add(1, std::memory_order_relaxed);
        return ok;
    }

    void service() {
        std::scoped_lock lock(control_mutex_);
        collect_retired_locked();
    }

    void settle_stopped() {
        std::scoped_lock lock(control_mutex_);
        auto            *pending = pending_.exchange(nullptr, std::memory_order_acq_rel);
        if (pending) {
            auto *old = active_.exchange(pending, std::memory_order_acq_rel);
            if (old)
                graphs_.erase(old);
        }
        if (fade_old_) {
            graphs_.erase(fade_old_);
            fade_old_ = nullptr;
        }
        crossfade_remaining_ = 0;
        crossfade_offset_    = 0;
        collect_retired_locked();
        swap_in_flight_.store(false, std::memory_order_release);
    }

    [[nodiscard]] MeterSnapshot meter() const {
        return {
            .input_peak       = bits_float(meter_input_peak_.load(std::memory_order_relaxed)),
            .input_rms        = bits_float(meter_input_rms_.load(std::memory_order_relaxed)),
            .output_peak      = bits_float(meter_output_peak_.load(std::memory_order_relaxed)),
            .output_rms       = bits_float(meter_output_rms_.load(std::memory_order_relaxed)),
            .frames           = processed_frames_.load(std::memory_order_relaxed),
            .process_failures = process_failures_.load(std::memory_order_relaxed),
            .clipped          = clipped_.load(std::memory_order_relaxed),
        };
    }

    [[nodiscard]] bool swap_in_flight() const { return swap_in_flight_.load(std::memory_order_acquire); }

    [[nodiscard]] bool has_active_graph() const { return active_.load(std::memory_order_acquire) != nullptr; }

  private:
    void adopt_pending() noexcept {
        if (fade_old_)
            return;
        auto *next = pending_.exchange(nullptr, std::memory_order_acq_rel);
        if (!next)
            return;
        fade_old_ = active_.exchange(next, std::memory_order_acq_rel);
        if (!fade_old_) {
            swap_in_flight_.store(false, std::memory_order_release);
            return;
        }
        crossfade_remaining_ = kSwapCrossfadeFrames;
        crossfade_offset_    = 0;
    }

    void retire_fade_graph() noexcept {
        auto *retired                  = fade_old_;
        fade_old_                      = nullptr;
        CompiledProjectGraph *expected = nullptr;
        if (!retired_.compare_exchange_strong(
                expected, retired, std::memory_order_release, std::memory_order_relaxed
            )) {
            process_failures_.fetch_add(1, std::memory_order_relaxed);
        }
        swap_in_flight_.store(false, std::memory_order_release);
    }

    void collect_retired_locked() {
        auto *retired = retired_.exchange(nullptr, std::memory_order_acq_rel);
        if (retired)
            graphs_.erase(retired);
    }

    mutable std::mutex                                                                control_mutex_;
    AudioDeviceConfig                                                                 config_;
    ControlStore                                                                      controls_;
    std::unordered_map<CompiledProjectGraph *, std::unique_ptr<CompiledProjectGraph>> graphs_;
    std::atomic<CompiledProjectGraph *>                                               active_{nullptr};
    std::atomic<CompiledProjectGraph *>                                               pending_{nullptr};
    std::atomic<CompiledProjectGraph *>                                               retired_{nullptr};
    CompiledProjectGraph                                                             *fade_old_            = nullptr;
    std::uint32_t                                                                     crossfade_remaining_ = 0;
    std::uint32_t                                                                     crossfade_offset_    = 0;
    std::vector<float>                                                                zero_input_;
    std::vector<float>                                                                old_output_;
    std::vector<float>                                                                new_output_;
    std::atomic<bool>                                                                 running_{false};
    std::atomic<bool>                                                                 swap_in_flight_{false};
    std::atomic<std::uint32_t> meter_input_peak_{float_bits(0.0f)};
    std::atomic<std::uint32_t> meter_input_rms_{float_bits(0.0f)};
    std::atomic<std::uint32_t> meter_output_peak_{float_bits(0.0f)};
    std::atomic<std::uint32_t> meter_output_rms_{float_bits(0.0f)};
    std::atomic<std::uint64_t> processed_frames_{0};
    std::atomic<std::uint64_t> process_failures_{0};
    std::atomic<bool>          clipped_{false};
};

RealtimeProjectEngine::RealtimeProjectEngine(AudioDeviceConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

RealtimeProjectEngine::~RealtimeProjectEngine() = default;

bool RealtimeProjectEngine::configure(const AudioDeviceConfig &config) { return impl_->configure(config); }

AudioDeviceConfig RealtimeProjectEngine::config() const { return impl_->config(); }

bool RealtimeProjectEngine::synchronize(const RuntimeProjectSpec &spec, bool structural, std::string &diagnostic) {
    return impl_->synchronize(spec, structural, diagnostic);
}

bool RealtimeProjectEngine::set_param(const std::string &path, float value) { return impl_->set_param(path, value); }

bool RealtimeProjectEngine::set_bypass(const std::string &instance, bool bypassed) {
    return impl_->set_bypass(instance, bypassed);
}

void RealtimeProjectEngine::set_mute(bool muted) { impl_->set_mute(muted); }

bool RealtimeProjectEngine::muted() const { return impl_->muted(); }

void RealtimeProjectEngine::set_running(bool running) { impl_->set_running(running); }

bool RealtimeProjectEngine::running() const { return impl_->running(); }

bool RealtimeProjectEngine::process(const float *input, float *output, std::uint32_t frames) noexcept {
    return impl_->process(input, output, frames);
}

void RealtimeProjectEngine::service() { impl_->service(); }

void RealtimeProjectEngine::settle_stopped() { impl_->settle_stopped(); }

MeterSnapshot RealtimeProjectEngine::meter() const { return impl_->meter(); }

bool RealtimeProjectEngine::swap_in_flight() const { return impl_->swap_in_flight(); }

bool RealtimeProjectEngine::has_active_graph() const { return impl_->has_active_graph(); }

} // namespace apg::terminal
