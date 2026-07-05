#include "atom/atom_field_descriptors.h"
#include "atom/dsp_atoms.h"
#include <atom_registry.h>
#include <stddef.h>
#include <string.h>

// ─────────────────────────────────────────────
// Thunk generation
// ─────────────────────────────────────────────

#define PROCESS_ATOMS(_)      \
    _(amplitude_accumulate)   \
    _(amplitude_add)          \
    _(amplitude_clip_hard)    \
    _(amplitude_clip_soft)    \
    _(amplitude_divide)       \
    _(amplitude_latch)        \
    _(amplitude_multiply)     \
    _(amplitude_normalize)    \
    _(amplitude_smooth)       \
    _(amplitude_subtract)     \
    _(delay_fractional)       \
    _(delay_line)             \
    _(delay_tap_feedback)     \
    _(delay_tap_feedforward)  \
    _(delay_unit)             \
    _(detect_autocorrelate)   \
    _(detect_envelope)        \
    _(detect_peak)            \
    _(detect_pitch)           \
    _(detect_rms)             \
    _(detect_slope)           \
    _(detect_threshold)       \
    _(detect_zero_crossing)   \
    _(filter_allpass)         \
    _(filter_biquad)          \
    _(filter_comb_fb)         \
    _(filter_comb_ff)         \
    _(filter_dc_block)        \
    _(filter_differentiate)   \
    _(filter_fir)             \
    _(filter_integrate)       \
    _(freq_overlap_add)       \
    _(freq_overlap_save)      \
    _(freq_quantize)          \
    _(freq_shift)             \
    _(freq_window)            \
    _(generation_dc)          \
    _(generation_envelope)    \
    _(generation_impulse)     \
    _(generation_lfo)         \
    _(generation_noise)       \
    _(generation_oscillator)  \
    _(interpolation_cubic)    \
    _(interpolation_lagrange) \
    _(interpolation_linear)   \
    _(interpolation_sinc)     \
    _(mix_crossfade)          \
    _(mix_decode_ms)          \
    _(mix_encode_ms)          \
    _(mix_matrix)             \
    _(mix_pan_stereo)         \
    _(mix_wet_dry)            \
    _(modulation_amplitude)   \
    _(modulation_frequency)   \
    _(modulation_phase)       \
    _(modulation_ring)        \
    _(modulation_scrub)       \
    _(nonlinear_bitcrush)     \
    _(nonlinear_sample_hold)  \
    _(nonlinear_waveshape)    \
    _(src_antialias)          \
    _(src_antiimage)          \
    _(src_convert_format)     \
    _(src_downsample)         \
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

#define REGISTRY_FIELDS(atom_name, kind, count)        REGISTRY_FIELDS_EXPAND(atom_name, kind, count)
#define REGISTRY_FIELDS_EXPAND(atom_name, kind, count) REGISTRY_FIELDS_##count(atom_name, kind)
#define REGISTRY_FIELDS_0(atom_name, kind)             .kind##_fields = NULL, .n_##kind##_fields = 0
#define REGISTRY_FIELDS_1(atom_name, kind)             .kind##_fields = atom_name##_##kind##_fields, .n_##kind##_fields = 1
#define REGISTRY_FIELDS_2(atom_name, kind)             .kind##_fields = atom_name##_##kind##_fields, .n_##kind##_fields = 2
#define REGISTRY_FIELDS_3(atom_name, kind)             .kind##_fields = atom_name##_##kind##_fields, .n_##kind##_fields = 3
#define REGISTRY_FIELDS_4(atom_name, kind)             .kind##_fields = atom_name##_##kind##_fields, .n_##kind##_fields = 4
#define REGISTRY_FIELDS_5(atom_name, kind)             .kind##_fields = atom_name##_##kind##_fields, .n_##kind##_fields = 5

#define REGISTRY_ATOMS(_)           \
    _(amplitude_accumulate, 0, 1)   \
    _(amplitude_latch, 1, 2)        \
    _(amplitude_add, 0, 0)          \
    _(amplitude_clip_hard, 1, 0)    \
    _(amplitude_clip_soft, 2, 0)    \
    _(amplitude_divide, 1, 0)       \
    _(amplitude_multiply, 0, 0)     \
    _(amplitude_normalize, 2, 1)    \
    _(amplitude_smooth, 3, 1)       \
    _(amplitude_subtract, 0, 0)     \
    _(delay_fractional, 2, 2)       \
    _(delay_line, 1, 2)             \
    _(delay_tap_feedback, 1, 0)     \
    _(delay_tap_feedforward, 1, 0)  \
    _(delay_unit, 0, 1)             \
    _(detect_autocorrelate, 1, 2)   \
    _(detect_pitch, 2, 2)           \
    _(detect_envelope, 3, 1)        \
    _(detect_peak, 3, 1)            \
    _(detect_rms, 1, 3)             \
    _(detect_slope, 0, 1)           \
    _(detect_threshold, 1, 0)       \
    _(detect_zero_crossing, 0, 1)   \
    _(filter_allpass, 2, 2)         \
    _(filter_biquad, 5, 2)          \
    _(filter_comb_fb, 2, 2)         \
    _(filter_comb_ff, 2, 2)         \
    _(filter_dc_block, 1, 2)        \
    _(filter_differentiate, 0, 1)   \
    _(filter_fir, 2, 2)             \
    _(filter_integrate, 0, 1)       \
    _(freq_fft, 1, 0)               \
    _(freq_ifft, 1, 0)              \
    _(freq_multiply, 1, 0)          \
    _(freq_overlap_add, 2, 1)       \
    _(freq_overlap_save, 2, 2)      \
    _(freq_overlap_add, 2, 1)       \
    _(freq_overlap_save, 2, 2)      \
    _(freq_window, 2, 0)            \
    _(freq_shift, 1, 5)             \
    _(generation_dc, 1, 0)          \
    _(generation_envelope, 5, 2)    \
    _(generation_impulse, 2, 1)     \
    _(generation_lfo, 4, 1)         \
    _(generation_noise, 2, 2)       \
    _(generation_oscillator, 4, 1)  \
    _(interpolation_cubic, 0, 0)    \
    _(interpolation_lagrange, 1, 2) \
    _(interpolation_linear, 0, 0)   \
    _(interpolation_sinc, 1, 1)     \
    _(mix_crossfade, 1, 0)          \
    _(mix_decode_ms, 0, 0)          \
    _(mix_encode_ms, 0, 0)          \
    _(mix_matrix, 2, 0)             \
    _(mix_pan_stereo, 1, 0)         \
    _(mix_wet_dry, 1, 0)            \
    _(modulation_amplitude, 1, 0)   \
    _(modulation_frequency, 1, 3)   \
    _(modulation_phase, 1, 2)       \
    _(modulation_ring, 0, 0)        \
    _(modulation_scrub, 1, 0)       \
    _(nonlinear_bitcrush, 1, 0)     \
    _(nonlinear_sample_hold, 1, 2)  \
    _(nonlinear_waveshape, 2, 0)    \
    _(src_antialias, 2, 2)          \
    _(src_antiimage, 2, 2)          \
    _(src_convert_format, 2, 0)     \
    _(src_downsample, 1, 0)         \
    _(src_upsample, 1, 0)           \
    _(freq_quantize, 0, 0)

#define REGISTRY_ATOM(atom_name, config_count, state_count) \
    {                                                       \
        .name        = #atom_name,                          \
        .thunk       = atom_name##_thunk,                   \
        .out_size    = sizeof(atom_name##_out_t),           \
        .in_size     = sizeof(atom_name##_in_t),            \
        .config_size = sizeof(atom_name##_params_t),        \
        .state_size  = sizeof(atom_name##_state_t),         \
        REGISTRY_FIELDS(atom_name, config, config_count),   \
        REGISTRY_FIELDS(atom_name, state, state_count),     \
    },

static atom_registry_entry_t g_registry[] = {REGISTRY_ATOMS(REGISTRY_ATOM)};

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
