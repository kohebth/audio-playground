#include <apgcore/measure/atom_cost.h>

#include <atom/dsp_types.h>

#include <limits.h>
#include <string.h>

#define APG_DEFAULT_DYNAMIC_ITEMS       8u
#define APG_DEFAULT_FIR_TAPS            64u
#define APG_DEFAULT_SINC_TAPS           31u
#define APG_DEFAULT_AUTOCORRELATION_LAG 256u
#define APG_MAX_COST_ITEMS              65536u

static uint64_t sat_add_u64(uint64_t a, uint64_t b) { return UINT64_MAX - a < b ? UINT64_MAX : a + b; }

static uint64_t sat_mul_u64(uint64_t a, uint64_t b) {
    if (a == 0u || b == 0u)
        return 0u;
    return a > UINT64_MAX / b ? UINT64_MAX : a * b;
}

static uint32_t clamp_items_i32(int value, uint32_t fallback, uint32_t maximum) {
    if (value <= 0)
        return fallback;
    return (uint32_t)value > maximum ? maximum : (uint32_t)value;
}

static uint64_t state_buffer_bytes(const atom_registry_entry_t *entry) {
    uint64_t total = entry ? (uint64_t)entry->state_size : 0u;
    if (!entry || !entry->state_fields)
        return total;
    for (int i = 0; i < entry->n_state_fields; ++i) {
        if (entry->state_fields[i].type != FIELD_BUFFER)
            continue;
        total = sat_add_u64(total, sat_mul_u64((uint64_t)entry->state_fields[i].buffer_samples, sizeof(float)));
    }
    return total;
}

apg_atom_cost_class_t apg_cost_classify(uint64_t cpu_acu) {
    if (cpu_acu < 1000u)
        return APG_COST_TRIVIAL;
    if (cpu_acu < 5000u)
        return APG_COST_LOW;
    if (cpu_acu < 25000u)
        return APG_COST_MEDIUM;
    if (cpu_acu < 100000u)
        return APG_COST_HIGH;
    return APG_COST_EXTREME;
}

const char *apg_cost_class_name(apg_atom_cost_class_t cost_class) {
    switch (cost_class) {
    case APG_COST_TRIVIAL:
        return "trivial";
    case APG_COST_LOW:
        return "low";
    case APG_COST_MEDIUM:
        return "medium";
    case APG_COST_HIGH:
        return "high";
    case APG_COST_EXTREME:
        return "extreme";
    }
    return "unknown";
}

static bool atom_is(const atom_registry_entry_t *entry, const char *name) {
    return entry && entry->name && strcmp(entry->name, name) == 0;
}

static bool atom_has_prefix(const atom_registry_entry_t *entry, const char *prefix) {
    return entry && entry->name && prefix && strncmp(entry->name, prefix, strlen(prefix)) == 0;
}

static uint64_t linear_cost(uint64_t fixed, uint64_t frames, uint64_t per_frame) {
    return sat_add_u64(fixed, sat_mul_u64(frames, per_frame));
}

static uint32_t fft_size_from(const void *config, const apg_spectral_info_t *spectral_info) {
    if (apg_spectral_info_valid(spectral_info))
        return spectral_info->fft_size;
    if (config) {
        const freq_fft_params_t *params = (const freq_fft_params_t *)config;
        uint32_t                 n      = clamp_items_i32(params->block_size, 512u, 2048u);
        if (apg_spectral_fft_size_supported(n))
            return n;
    }
    return 512u;
}

static uint32_t integer_log2(uint32_t value) {
    uint32_t result = 0u;
    while (value > 1u) {
        value >>= 1u;
        ++result;
    }
    return result;
}

bool apg_atom_estimate_cost(
    const atom_registry_entry_t *entry,
    const void                  *config,
    const apg_process_context_t *process_context,
    const apg_spectral_info_t   *spectral_info,
    apg_atom_cost_result_t      *out
) {
    if (!entry || !entry->name || !out || !apg_process_context_valid(process_context))
        return false;

    memset(out, 0, sizeof(*out));
    const uint64_t frames  = apg_process_context_frames(process_context);
    uint64_t       cpu     = linear_cost(8u, frames, 8u); /* conservative generic scalar default */
    uint64_t       scratch = 0u;
    uint32_t       latency = 0u;

    if (atom_is(entry, "generation_dc"))
        cpu = linear_cost(4u, frames, 1u);
    else if (atom_is(entry, "amplitude_add") || atom_is(entry, "amplitude_subtract") ||
             atom_is(entry, "amplitude_multiply") || atom_is(entry, "modulation_ring"))
        cpu = linear_cost(8u, frames, 4u);
    else if (atom_is(entry, "amplitude_divide"))
        cpu = linear_cost(8u, frames, 11u);
    else if (atom_is(entry, "amplitude_clip_hard"))
        cpu = linear_cost(8u, frames, 6u);
    else if (atom_is(entry, "amplitude_clip_soft"))
        cpu = linear_cost(16u, frames, 18u);
    else if (atom_is(entry, "amplitude_smooth"))
        cpu = linear_cost(16u, frames, 8u);
    else if (atom_is(entry, "detect_peak"))
        cpu = linear_cost(20u, frames, 10u);
    else if (atom_is(entry, "detect_envelope"))
        cpu = linear_cost(20u, frames, 11u);
    else if (atom_is(entry, "generation_envelope"))
        cpu = linear_cost(24u, frames, 12u);
    else if (atom_is(entry, "filter_biquad_coefficients"))
        cpu = linear_cost(40u, frames, 16u);
    else if (atom_is(entry, "filter_biquad"))
        cpu = linear_cost(80u, frames, 24u); /* includes smoothing and occasional coefficient refresh */
    else if (atom_is(entry, "delay_line")) {
        cpu                               = linear_cost(20u, frames, 9u);
        const delay_line_params_t *params = (const delay_line_params_t *)config;
        latency                           = params && params->length > 0 ? (uint32_t)params->length : 0u;
    } else if (atom_is(entry, "delay_fractional")) {
        cpu                                     = linear_cost(24u, frames, 16u);
        const delay_fractional_params_t *params = (const delay_fractional_params_t *)config;
        latency = params && params->delay_samples > 0.0f ? (uint32_t)params->delay_samples : 0u;
    } else if (atom_is(entry, "delay_tap_feedback") || atom_is(entry, "delay_tap_feedforward"))
        cpu = linear_cost(8u, frames, 5u);
    else if (atom_is(entry, "filter_fir")) {
        const filter_fir_params_t *params = (const filter_fir_params_t *)config;
        const uint32_t             taps =
            params ? clamp_items_i32(params->kernel_size, APG_DEFAULT_FIR_TAPS, 1024u) : APG_DEFAULT_FIR_TAPS;
        cpu     = sat_add_u64(linear_cost(40u, frames, 5u), sat_mul_u64(sat_mul_u64(frames, taps), 3u));
        latency = taps > 0u ? (taps - 1u) / 2u : 0u;
    } else if (atom_is(entry, "filter_comb_ff"))
        cpu = linear_cost(24u, frames, 12u);
    else if (atom_is(entry, "filter_comb_fb"))
        cpu = linear_cost(24u, frames, 16u);
    else if (atom_is(entry, "filter_allpass"))
        cpu = linear_cost(24u, frames, 18u);
    else if (atom_is(entry, "interpolation_linear"))
        cpu = linear_cost(8u, frames, 8u);
    else if (atom_is(entry, "interpolation_cubic"))
        cpu = linear_cost(16u, frames, 24u);
    else if (atom_is(entry, "interpolation_lagrange")) {
        const interpolation_lagrange_params_t *params    = (const interpolation_lagrange_params_t *)config;
        const uint32_t                         order     = params ? clamp_items_i32(params->order, 2u, 8u) : 2u;
        const uint64_t                         dimension = sat_mul_u64(order + 1u, order + 1u);
        cpu = sat_add_u64(linear_cost(24u, frames, 5u), sat_mul_u64(sat_mul_u64(frames, dimension), 3u));
    } else if (atom_is(entry, "interpolation_sinc")) {
        const interpolation_sinc_params_t *params = (const interpolation_sinc_params_t *)config;
        const uint32_t                     taps =
            params ? clamp_items_i32(params->num_taps, APG_DEFAULT_SINC_TAPS, 63u) : APG_DEFAULT_SINC_TAPS;
        cpu = sat_add_u64(32u, sat_mul_u64(sat_mul_u64(frames, taps), 30u));
    } else if (atom_is(entry, "mix_crossfade") || atom_is(entry, "mix_wet_dry"))
        cpu = linear_cost(8u, frames, 7u);
    else if (atom_is(entry, "mix_pan_stereo"))
        cpu = linear_cost(48u, frames, 8u);
    else if (atom_is(entry, "mix_matrix")) {
        const mix_matrix_params_t *params  = (const mix_matrix_params_t *)config;
        const uint32_t             num_in  = params ? clamp_items_i32(params->num_in, 2u, 8u) : 2u;
        const uint32_t             num_out = params ? clamp_items_i32(params->num_out, 2u, 8u) : 2u;
        cpu                                = sat_add_u64(
            linear_cost(24u, frames, sat_mul_u64(num_out, 2u)),
            sat_mul_u64(sat_mul_u64(sat_mul_u64(frames, num_in), num_out), 3u)
        );
    } else if (atom_is(entry, "src_upsample")) {
        const src_upsample_params_t *params = (const src_upsample_params_t *)config;
        const uint32_t               factor = params ? clamp_items_i32(params->factor, 1u, 16u) : 1u;
        cpu                                 = sat_add_u64(12u, sat_mul_u64(sat_mul_u64(frames, factor), 2u));
    } else if (atom_is(entry, "src_downsample")) {
        const src_downsample_params_t *params = (const src_downsample_params_t *)config;
        const uint32_t                 factor = params ? clamp_items_i32(params->factor, 1u, 16u) : 1u;
        cpu                                   = sat_add_u64(12u, sat_mul_u64((frames + factor - 1u) / factor, 3u));
    } else if (atom_is(entry, "src_antialias") || atom_is(entry, "src_antiimage"))
        cpu = linear_cost(40u, frames, 16u);
    else if (atom_is(entry, "detect_rms"))
        cpu = linear_cost(24u, frames, 22u);
    else if (atom_is(entry, "detect_autocorrelate")) {
        const detect_autocorrelate_params_t *params = (const detect_autocorrelate_params_t *)config;
        const uint32_t lag = params ? clamp_items_i32(params->max_lag, APG_DEFAULT_AUTOCORRELATION_LAG, 2048u)
                                    : APG_DEFAULT_AUTOCORRELATION_LAG;
        cpu                = sat_add_u64(32u, sat_mul_u64(sat_mul_u64(frames, lag), 3u));
        latency            = lag;
    } else if (atom_is(entry, "detect_pitch")) {
        const detect_pitch_params_t *params = (const detect_pitch_params_t *)config;
        const uint32_t lag = params ? clamp_items_i32(params->max_lag, APG_DEFAULT_AUTOCORRELATION_LAG, 2048u)
                                    : APG_DEFAULT_AUTOCORRELATION_LAG;
        cpu     = sat_add_u64(sat_add_u64(40u, sat_mul_u64(sat_mul_u64(frames, lag), 3u)), sat_mul_u64(lag, 4u));
        latency = lag;
    } else if (atom_is(entry, "generation_noise"))
        cpu = linear_cost(12u, frames, 10u);
    else if (atom_is(entry, "generation_oscillator"))
        cpu = linear_cost(24u, frames, 24u);
    else if (atom_is(entry, "freq_fft") || atom_is(entry, "freq_ifft")) {
        const uint32_t n = fft_size_from(config, spectral_info);
        cpu              = sat_mul_u64(sat_mul_u64(n, integer_log2(n)), 10u);
        scratch          = sat_mul_u64(sat_mul_u64(n, 2u), sizeof(float));
    } else if (atom_is(entry, "freq_multiply")) {
        const uint32_t n = fft_size_from(config, spectral_info);
        cpu              = sat_mul_u64(n / 2u + 1u, 12u);
    } else if (atom_is(entry, "freq_window")) {
        const uint32_t n = fft_size_from(config, spectral_info);
        cpu              = sat_mul_u64(n, 4u);
    } else if (atom_is(entry, "freq_overlap_add") || atom_is(entry, "freq_overlap_save")) {
        const uint32_t n = fft_size_from(config, spectral_info);
        cpu              = sat_mul_u64(n, 6u);
        if (apg_spectral_info_valid(spectral_info))
            latency = spectral_info->fft_size - spectral_info->hop_size;
    } else if (atom_has_prefix(entry, "freq_"))
        cpu = linear_cost(64u, frames, 28u);

    out->cpu_acu          = cpu;
    out->persistent_bytes = state_buffer_bytes(entry);
    out->scratch_bytes    = scratch;
    out->latency_frames   = latency;
    out->cost_class       = apg_cost_classify(cpu);
    return true;
}

bool apg_graph_estimate_cost(
    const atom_registry_entry_t *const *entries,
    const void *const                  *configs,
    const apg_spectral_info_t *const   *spectral_infos,
    size_t                              count,
    const apg_process_context_t        *process_context,
    apg_graph_cost_result_t            *out
) {
    if (!out || (count > 0u && !entries))
        return false;
    memset(out, 0, sizeof(*out));

    for (size_t i = 0u; i < count; ++i) {
        apg_atom_cost_result_t     atom_cost;
        const void                *config   = configs ? configs[i] : NULL;
        const apg_spectral_info_t *spectral = spectral_infos ? spectral_infos[i] : NULL;
        if (!apg_atom_estimate_cost(entries[i], config, process_context, spectral, &atom_cost))
            return false;
        out->cpu_acu          = sat_add_u64(out->cpu_acu, atom_cost.cpu_acu);
        out->persistent_bytes = sat_add_u64(out->persistent_bytes, atom_cost.persistent_bytes);
        if (atom_cost.scratch_bytes > out->scratch_bytes)
            out->scratch_bytes = atom_cost.scratch_bytes;
        if (UINT32_MAX - out->latency_frames < atom_cost.latency_frames)
            out->latency_frames = UINT32_MAX;
        else
            out->latency_frames += atom_cost.latency_frames;
        out->atom_count++;
    }

    out->cost_class = apg_cost_classify(out->cpu_acu);
    return true;
}
