#include "atom/atom_field_descriptors.h"
#include "atom/dsp_atoms.h"
#include <atom_registry.h>
#include <stddef.h>
#include <string.h>

// ─────────────────────────────────────────────
// Thunk generation
// ─────────────────────────────────────────────

#define PROCESS_ATOMS(_)           \
    _(amplitude_accumulate)        \
    _(amplitude_add)               \
    _(amplitude_clip_hard)         \
    _(amplitude_clip_soft)         \
    _(amplitude_divide)            \
    _(amplitude_latch)             \
    _(amplitude_multiply)          \
    _(amplitude_normalize)         \
    _(amplitude_smooth)            \
    _(amplitude_subtract)          \
    _(delay_fractional)            \
    _(delay_line)                  \
    _(delay_tap_feedback)          \
    _(delay_tap_feedforward)       \
    _(delay_unit)                  \
    _(detect_autocorrelate)        \
    _(detect_envelope)             \
    _(detect_peak)                 \
    _(detect_pitch)                \
    _(detect_rms)                  \
    _(detect_slope)                \
    _(detect_threshold)            \
    _(detect_zero_crossing)        \
    _(filter_allpass)              \
    _(filter_biquad)               \
    _(filter_comb_fb)              \
    _(filter_comb_ff)              \
    _(filter_dc_block)             \
    _(filter_differentiate)        \
    _(filter_fir)                  \
    _(filter_integrate)            \
    _(freq_overlap_add)            \
    _(freq_overlap_save)           \
    _(freq_quantize)               \
    _(freq_shift)                  \
    _(freq_window)                 \
    _(generation_dc)               \
    _(generation_envelope)         \
    _(generation_impulse)          \
    _(generation_lfo)              \
    _(generation_noise)            \
    _(generation_oscillator)       \
    _(interpolation_cubic)         \
    _(interpolation_lagrange)      \
    _(interpolation_linear)        \
    _(interpolation_sinc)          \
    _(mix_crossfade)               \
    _(mix_decode_ms)               \
    _(mix_encode_ms)               \
    _(mix_matrix)                  \
    _(mix_pan_stereo)              \
    _(mix_wet_dry)                 \
    _(modulation_amplitude)        \
    _(modulation_frequency)        \
    _(modulation_phase)            \
    _(modulation_ring)             \
    _(modulation_scrub)            \
    _(nonlinear_bitcrush)          \
    _(nonlinear_samplerate_reduce) \
    _(nonlinear_waveshape)         \
    _(src_antialias)               \
    _(src_antiimage)               \
    _(src_convert_format)          \
    _(src_downsample)              \
    _(src_upsample)

#define LEGACY_ATOMS(_) \
    _(freq_fft)         \
    _(freq_ifft)        \
    _(freq_multiply)

#define PROCESS_THUNK(atom_name)                                                                                \
    void atom_name##_thunk(atom_call_t *call) {                                                                 \
        atom_name##_process(                                                                                    \
            (atom_name##_out_t *)call->out, (atom_name##_in_t *)call->in, (atom_name##_params_t *)call->config, \
            (atom_name##_state_t *)call->state, call->info                                                      \
        );                                                                                                      \
    }

#define LEGACY_THUNK(atom_name)                                                                                 \
    void atom_name##_thunk(atom_call_t *call) {                                                                 \
        atom_name(                                                                                              \
            (atom_name##_out_t *)call->out, (atom_name##_in_t *)call->in, (atom_name##_params_t *)call->config, \
            (atom_name##_state_t *)call->state                                                                  \
        );                                                                                                      \
    }

PROCESS_ATOMS(PROCESS_THUNK)
LEGACY_ATOMS(LEGACY_THUNK)

// ─────────────────────────────────────────────
// Registry table
// ─────────────────────────────────────────────

#define REGISTRY_FIELDS(fields) fields, sizeof(fields) / sizeof((fields)[0])
#define REGISTRY_NO_FIELDS      NULL, 0

#define REGISTRY_ATOM(atom_name, config_fields, state_fields) \
    REGISTRY_ATOM_EXPAND(atom_name, config_fields, state_fields)

#define REGISTRY_ATOM_EXPAND(atom_name, c_fields, n_cfg, s_fields, n_st)                                \
    {                                                                                                   \
        .name = #atom_name, .thunk = atom_name##_thunk, .out_size = sizeof(atom_name##_out_t),          \
        .in_size = sizeof(atom_name##_in_t), .config_size = sizeof(atom_name##_params_t),               \
        .state_size = sizeof(atom_name##_state_t), .config_fields = c_fields, .n_config_fields = n_cfg, \
        .state_fields = s_fields, .n_state_fields = n_st,                                               \
    }

static atom_registry_entry_t g_registry[] = {
    REGISTRY_ATOM(amplitude_accumulate, REGISTRY_NO_FIELDS, REGISTRY_FIELDS(amplitude_accumulate_state_fields)),
    REGISTRY_ATOM(
        amplitude_latch, REGISTRY_FIELDS(amplitude_latch_config_fields), REGISTRY_FIELDS(amplitude_latch_state_fields)
    ),
    REGISTRY_ATOM(amplitude_add, REGISTRY_NO_FIELDS, REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(amplitude_clip_hard, REGISTRY_FIELDS(amplitude_clip_hard_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(amplitude_clip_soft, REGISTRY_FIELDS(amplitude_clip_soft_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(amplitude_divide, REGISTRY_FIELDS(amplitude_divide_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(amplitude_multiply, REGISTRY_NO_FIELDS, REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(
        amplitude_normalize,
        REGISTRY_FIELDS(amplitude_normalize_config_fields),
        REGISTRY_FIELDS(amplitude_normalize_state_fields)
    ),
    REGISTRY_ATOM(
        amplitude_smooth,
        REGISTRY_FIELDS(amplitude_smooth_config_fields),
        REGISTRY_FIELDS(amplitude_smooth_state_fields)
    ),
    REGISTRY_ATOM(amplitude_subtract, REGISTRY_NO_FIELDS, REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(
        delay_fractional,
        REGISTRY_FIELDS(delay_fractional_config_fields),
        REGISTRY_FIELDS(delay_fractional_state_fields)
    ),
    REGISTRY_ATOM(delay_line, REGISTRY_FIELDS(delay_line_config_fields), REGISTRY_FIELDS(delay_line_state_fields)),
    REGISTRY_ATOM(delay_tap_feedback, REGISTRY_FIELDS(delay_tap_feedback_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(delay_tap_feedforward, REGISTRY_FIELDS(delay_tap_feedforward_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(delay_unit, REGISTRY_NO_FIELDS, REGISTRY_FIELDS(delay_unit_state_fields)),
    REGISTRY_ATOM(
        detect_autocorrelate,
        REGISTRY_FIELDS(detect_autocorrelate_config_fields),
        REGISTRY_FIELDS(detect_autocorrelate_state_fields)
    ),
    REGISTRY_ATOM(
        detect_pitch, REGISTRY_FIELDS(detect_pitch_config_fields), REGISTRY_FIELDS(detect_pitch_state_fields)
    ),
    REGISTRY_ATOM(
        detect_envelope, REGISTRY_FIELDS(detect_envelope_config_fields), REGISTRY_FIELDS(detect_envelope_state_fields)
    ),
    REGISTRY_ATOM(detect_peak, REGISTRY_FIELDS(detect_peak_config_fields), REGISTRY_FIELDS(detect_peak_state_fields)),
    REGISTRY_ATOM(detect_rms, REGISTRY_FIELDS(detect_rms_config_fields), REGISTRY_FIELDS(detect_rms_state_fields)),
    REGISTRY_ATOM(detect_slope, REGISTRY_NO_FIELDS, REGISTRY_FIELDS(detect_slope_state_fields)),
    REGISTRY_ATOM(detect_threshold, REGISTRY_FIELDS(detect_threshold_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(detect_zero_crossing, REGISTRY_NO_FIELDS, REGISTRY_FIELDS(detect_zero_crossing_state_fields)),
    REGISTRY_ATOM(
        filter_allpass, REGISTRY_FIELDS(filter_allpass_config_fields), REGISTRY_FIELDS(filter_allpass_state_fields)
    ),
    REGISTRY_ATOM(
        filter_biquad, REGISTRY_FIELDS(filter_biquad_config_fields), REGISTRY_FIELDS(filter_biquad_state_fields)
    ),
    REGISTRY_ATOM(
        filter_comb_fb, REGISTRY_FIELDS(filter_comb_fb_config_fields), REGISTRY_FIELDS(filter_comb_fb_state_fields)
    ),
    REGISTRY_ATOM(
        filter_comb_ff, REGISTRY_FIELDS(filter_comb_ff_config_fields), REGISTRY_FIELDS(filter_comb_ff_state_fields)
    ),
    REGISTRY_ATOM(
        filter_dc_block, REGISTRY_FIELDS(filter_dc_block_config_fields), REGISTRY_FIELDS(filter_dc_block_state_fields)
    ),
    REGISTRY_ATOM(filter_differentiate, REGISTRY_NO_FIELDS, REGISTRY_FIELDS(filter_differentiate_state_fields)),
    REGISTRY_ATOM(filter_fir, REGISTRY_FIELDS(filter_fir_config_fields), REGISTRY_FIELDS(filter_fir_state_fields)),
    REGISTRY_ATOM(filter_integrate, REGISTRY_NO_FIELDS, REGISTRY_FIELDS(filter_integrate_state_fields)),
    REGISTRY_ATOM(freq_fft, REGISTRY_FIELDS(freq_fft_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(freq_ifft, REGISTRY_FIELDS(freq_ifft_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(freq_multiply, REGISTRY_FIELDS(freq_multiply_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(
        freq_overlap_add,
        REGISTRY_FIELDS(freq_overlap_add_config_fields),
        REGISTRY_FIELDS(freq_overlap_add_state_fields)
    ),
    REGISTRY_ATOM(
        freq_overlap_save,
        REGISTRY_FIELDS(freq_overlap_save_config_fields),
        REGISTRY_FIELDS(freq_overlap_save_state_fields)
    ),
    REGISTRY_ATOM(
        freq_overlap_add,
        REGISTRY_FIELDS(freq_overlap_add_config_fields),
        REGISTRY_FIELDS(freq_overlap_add_state_fields)
    ),
    REGISTRY_ATOM(
        freq_overlap_save,
        REGISTRY_FIELDS(freq_overlap_save_config_fields),
        REGISTRY_FIELDS(freq_overlap_save_state_fields)
    ),
    REGISTRY_ATOM(freq_window, REGISTRY_FIELDS(freq_window_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(freq_shift, REGISTRY_FIELDS(freq_shift_config_fields), REGISTRY_FIELDS(freq_shift_state_fields)),
    REGISTRY_ATOM(generation_dc, REGISTRY_FIELDS(generation_dc_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(
        generation_envelope,
        REGISTRY_FIELDS(generation_envelope_config_fields),
        REGISTRY_FIELDS(generation_envelope_state_fields)
    ),
    REGISTRY_ATOM(
        generation_impulse,
        REGISTRY_FIELDS(generation_impulse_config_fields),
        REGISTRY_FIELDS(generation_impulse_state_fields)
    ),
    REGISTRY_ATOM(
        generation_lfo, REGISTRY_FIELDS(generation_lfo_config_fields), REGISTRY_FIELDS(generation_lfo_state_fields)
    ),
    REGISTRY_ATOM(
        generation_noise,
        REGISTRY_FIELDS(generation_noise_config_fields),
        REGISTRY_FIELDS(generation_noise_state_fields)
    ),
    REGISTRY_ATOM(
        generation_oscillator,
        REGISTRY_FIELDS(generation_oscillator_config_fields),
        REGISTRY_FIELDS(generation_oscillator_state_fields)
    ),
    REGISTRY_ATOM(interpolation_cubic, REGISTRY_NO_FIELDS, REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(
        interpolation_lagrange,
        REGISTRY_FIELDS(interpolation_lagrange_config_fields),
        REGISTRY_FIELDS(interpolation_lagrange_state_fields)
    ),
    REGISTRY_ATOM(interpolation_linear, REGISTRY_NO_FIELDS, REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(
        interpolation_sinc,
        REGISTRY_FIELDS(interpolation_sinc_config_fields),
        REGISTRY_FIELDS(interpolation_sinc_state_fields)
    ),
    REGISTRY_ATOM(mix_crossfade, REGISTRY_FIELDS(mix_crossfade_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(mix_decode_ms, REGISTRY_NO_FIELDS, REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(mix_encode_ms, REGISTRY_NO_FIELDS, REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(mix_matrix, REGISTRY_FIELDS(mix_matrix_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(mix_pan_stereo, REGISTRY_FIELDS(mix_pan_stereo_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(mix_wet_dry, REGISTRY_FIELDS(mix_wet_dry_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(modulation_amplitude, REGISTRY_FIELDS(modulation_amplitude_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(
        modulation_frequency,
        REGISTRY_FIELDS(modulation_frequency_config_fields),
        REGISTRY_FIELDS(modulation_frequency_state_fields)
    ),
    REGISTRY_ATOM(
        modulation_phase,
        REGISTRY_FIELDS(modulation_phase_config_fields),
        REGISTRY_FIELDS(modulation_phase_state_fields)
    ),
    REGISTRY_ATOM(modulation_ring, REGISTRY_NO_FIELDS, REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(modulation_scrub, REGISTRY_FIELDS(modulation_scrub_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(nonlinear_bitcrush, REGISTRY_FIELDS(nonlinear_bitcrush_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(
        nonlinear_samplerate_reduce,
        REGISTRY_FIELDS(nonlinear_samplerate_reduce_config_fields),
        REGISTRY_FIELDS(nonlinear_samplerate_reduce_state_fields)
    ),
    REGISTRY_ATOM(nonlinear_waveshape, REGISTRY_FIELDS(nonlinear_waveshape_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(
        src_antialias, REGISTRY_FIELDS(src_antialias_config_fields), REGISTRY_FIELDS(src_antialias_state_fields)
    ),
    REGISTRY_ATOM(
        src_antiimage, REGISTRY_FIELDS(src_antiimage_config_fields), REGISTRY_FIELDS(src_antiimage_state_fields)
    ),
    REGISTRY_ATOM(src_convert_format, REGISTRY_FIELDS(src_convert_format_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(src_downsample, REGISTRY_FIELDS(src_downsample_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(src_upsample, REGISTRY_FIELDS(src_upsample_config_fields), REGISTRY_NO_FIELDS),
    REGISTRY_ATOM(freq_quantize, REGISTRY_NO_FIELDS, REGISTRY_NO_FIELDS),
};

static const int g_registry_count = sizeof(g_registry) / sizeof(g_registry[0]);

void atom_registry_init(void) {}

static const atom_field_desc_t *registry_in_fields_for_name(const char *name, size_t *out_len) {
    if (out_len)
        *out_len = 0u;
    if (!name)
        return NULL;
    if (strcmp(name, "delay_tap_feedback") == 0) {
        if (out_len)
            *out_len = sizeof(delay_tap_feedback_in_fields) / sizeof(delay_tap_feedback_in_fields[0]);
        return delay_tap_feedback_in_fields;
    }
    if (strcmp(name, "delay_tap_feedforward") == 0) {
        if (out_len)
            *out_len = sizeof(delay_tap_feedforward_in_fields) / sizeof(delay_tap_feedforward_in_fields[0]);
        return delay_tap_feedforward_in_fields;
    }
    return NULL;
}

const atom_field_desc_t *atom_registry_in_fields(const atom_registry_entry_t *atom, size_t *out_len) {
    if (!atom)
        return NULL;
    return registry_in_fields_for_name(atom->name, out_len);
}

const atom_registry_entry_t *atom_registry_find(const char *name) {
    for (int i = 0; i < g_registry_count; i++) {
        if (strcmp(g_registry[i].name, name) == 0)
            return &g_registry[i];
    }
    return NULL;
}

int atom_registry_count(void) { return g_registry_count; }

const atom_registry_entry_t *atom_registry_get(int index) {
    if (index < 0 || index >= g_registry_count)
        return NULL;
    return &g_registry[index];
}
