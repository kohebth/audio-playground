#include "atom/dsp_atoms.h"
#include <atom_registry.h>
#include <string.h>

// ─────────────────────────────────────────────
// Thunk generation — one wrapper per atom
// ─────────────────────────────────────────────

#define THUNK(_)                                                                 \
    void _##_thunk(atom_call_t *call) {                                          \
        _((_##_out_t *)call->out, (_##_in_t *)call->in,                          \
          (_##_params_t *)call->config, (_##_state_t *)call->state);             \
    }

#define DECLARE_THUNK_ALL(_)        \
    _(generation_oscillator)        \
    _(generation_noise)             \
    _(generation_envelope)          \
    _(generation_lfo)               \
    _(generation_impulse)           \
    _(generation_dc)                \
    _(amplitude_multiply)           \
    _(amplitude_divide)             \
    _(amplitude_smooth)             \
    _(amplitude_clip_hard)          \
    _(amplitude_clip_soft)          \
    _(amplitude_normalize)          \
    _(amplitude_add)                \
    _(amplitude_subtract)           \
    _(amplitude_accumulate)         \
    _(delay_unit)                   \
    _(delay_line)                   \
    _(delay_fractional)             \
    _(delay_tap_feedback)           \
    _(delay_tap_feedforward)        \
    _(filter_fir)                   \
    _(filter_biquad)                \
    _(filter_dc_block)              \
    _(filter_comb_ff)               \
    _(filter_comb_fb)               \
    _(filter_allpass)               \
    _(filter_integrate)             \
    _(filter_differentiate)         \
    _(detect_peak)                  \
    _(detect_envelope)              \
    _(detect_threshold)             \
    _(detect_rms)                   \
    _(detect_zero_crossing)         \
    _(detect_slope)                 \
    _(detect_autocorrelate)         \
    _(modulation_phase)             \
    _(modulation_ring)              \
    _(modulation_amplitude)         \
    _(modulation_frequency)         \
    _(modulation_scrub)             \
    _(interpolation_linear)         \
    _(interpolation_cubic)          \
    _(interpolation_sinc)           \
    _(interpolation_lagrange)       \
    _(src_upsample)                 \
    _(src_downsample)               \
    _(src_antialias)                \
    _(src_antiimage)                \
    _(src_convert_format)           \
    _(freq_fft)                     \
    _(freq_ifft)                    \
    _(freq_window)                  \
    _(freq_multiply)                \
    _(freq_overlap_add)             \
    _(freq_overlap_save)            \
    _(mix_crossfade)                \
    _(mix_wet_dry)                  \
    _(mix_matrix)                   \
    _(mix_pan_stereo)               \
    _(mix_encode_ms)                \
    _(mix_decode_ms)                \
    _(nonlinear_waveshape)          \
    _(nonlinear_bitcrush)           \
    _(nonlinear_samplerate_reduce)

DECLARE_THUNK_ALL(THUNK)

// ─────────────────────────────────────────────
// Registry table — reuse X-macro for entries
// ─────────────────────────────────────────────

#define REGISTRY_ENTRY(_) { \
    .name       = #_,       \
    .thunk      = _##_thunk, \
    .out_size   = sizeof(_##_out_t),    \
    .in_size    = sizeof(_##_in_t),     \
    .config_size= sizeof(_##_params_t), \
    .state_size = sizeof(_##_state_t),  \
},

static atom_registry_entry_t g_registry[] = {
    DECLARE_THUNK_ALL(REGISTRY_ENTRY)
};

static const int g_registry_count = sizeof(g_registry) / sizeof(g_registry[0]);

// ─────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────

void atom_registry_init(void) {
    // Registry is statically initialized, nothing to do
}

const atom_registry_entry_t *atom_registry_find(const char *name) {
    for (int i = 0; i < g_registry_count; i++) {
        if (strcmp(g_registry[i].name, name) == 0) {
            return &g_registry[i];
        }
    }
    return NULL;
}

int atom_registry_count(void) {
    return g_registry_count;
}

const atom_registry_entry_t *atom_registry_get(int index) {
    if (index < 0 || index >= g_registry_count) return NULL;
    return &g_registry[index];
}