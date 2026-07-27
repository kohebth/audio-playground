#include "apg_terminal/session.hpp"

extern "C" {
#include <apgcore/compiler/project_compiler_v2.h>
#include <apgcore/registry/registry_builder_v2.h>
#include <apgcore/runtime/runtime_v2.h>
#include <apgcore/validator/project_v2.h>
#include <yaml/arena.h>
#include <yaml/error.h>
}

#include <miniaudio.h>

#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace apg::terminal {
namespace {

constexpr std::uint32_t kSwapCrossfadeFrames        = 64;
constexpr unsigned      kNotificationUnexpectedStop = 1u << 0;
constexpr unsigned      kNotificationRerouted       = 1u << 1;
constexpr unsigned      kNotificationInterrupted    = 1u << 2;
constexpr unsigned      kNotificationResumed        = 1u << 3;

bool default_device_id(const std::string &id, AudioDeviceKind kind) {
    return id.empty() || id == (kind == AudioDeviceKind::Capture ? "capture:default" : "playback:default");
}

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

    void synchronize(const ApgPackageDocument &document, const std::map<std::string, bool> &live_bypass) {
        for (const auto &node : document.nodes()) {
            for (const auto &parameter : node.parameter_specs) {
                ensure_param(node.id + "." + parameter.name, static_cast<float>(parameter.value))
                    ->set(static_cast<float>(parameter.value));
            }
            if (!node.routing_helper()) {
                const auto bypassed = live_bypass.find(node.id);
                ensure_bypass(node.id, bypassed != live_bypass.end() && bypassed->second)
                    ->set(bypassed != live_bypass.end() && bypassed->second ? 1.0f : 0.0f);
            }
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
    static std::unique_ptr<CompiledProjectGraph> create(
        const ApgPackageDocument &document,
        ControlStore             &controls,
        const AudioDeviceConfig  &config,
        std::string              &diagnostic
    ) {
        auto           result = std::unique_ptr<CompiledProjectGraph>(new CompiledProjectGraph());
        AudioWorkspace workspace(document);
        // The compiled plan retains names owned by the resolved project, so both
        // arenas must outlive registry and runtime construction and processing.
        if (uc_arena_init(&result->resolved_arena_, 16u * 1024u * 1024u) != 0) {
            diagnostic = "Unable to allocate resolved-project arena.";
            return nullptr;
        }

        apg_project_v2_resolved_t resolved{};
        uc_error                  error{};
        uc_status                 status = apg_project_v2_load_resolved_file_with_root(
            workspace.entry().string().c_str(), workspace.root().string().c_str(), &result->resolved_arena_, &resolved,
            &error
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
            .maximum_frames = config.maximum_frames,
            .sample_rate    = static_cast<float>(config.sample_rate),
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

class RealtimeProjectEngine {
  public:
    explicit RealtimeProjectEngine(AudioDeviceConfig config)
        : config_(std::move(config)), zero_input_(config_.maximum_frames, 0.0f),
          old_output_(config_.maximum_frames, 0.0f), new_output_(config_.maximum_frames, 0.0f) {
        controls_.set_mute(true);
    }

    ~RealtimeProjectEngine() {
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

    bool synchronize(
        const ApgPackageDocument          &document,
        const std::map<std::string, bool> &bypass,
        bool                               structural,
        std::string                       &diagnostic
    ) {
        std::scoped_lock lock(control_mutex_);
        collect_retired_locked();
        controls_.synchronize(document, bypass);
        if (!structural && active_.load(std::memory_order_acquire)) {
            diagnostic.clear();
            return true;
        }
        if (swap_in_flight_.load(std::memory_order_acquire)) {
            diagnostic = "A graph swap is still finishing; wait for the next audio block.";
            return false;
        }
        auto graph = CompiledProjectGraph::create(document, controls_, config_, diagnostic);
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

struct SessionCore {
    explicit SessionCore(AudioDeviceConfig config) : engine(config), config(std::move(config)) {}

    bool synchronize(
        const ApgPackageDocument &next_document, const std::map<std::string, bool> &next_bypass, bool structural
    ) {
        if (!engine.synchronize(next_document, next_bypass, structural, diagnostic))
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
