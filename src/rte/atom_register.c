#include "atom/atom_capability.h"
#include "atom/atom_field_descriptors.h"
#include "atom/dsp_atoms.h"
#include <atom_registry.h>
#include <atom_thunk.h>
#include <stddef.h>
#include <string.h>

#define REGISTRY_FIELDS(atom_name, kind, count)        REGISTRY_FIELDS_EXPAND(atom_name, kind, count)
#define REGISTRY_FIELDS_EXPAND(atom_name, kind, count) REGISTRY_FIELDS_##count(atom_name, kind)
#define REGISTRY_FIELDS_0(atom_name, kind)             .kind##_fields = NULL, .n_##kind##_fields = 0
#define REGISTRY_FIELDS_1(atom_name, kind)             .kind##_fields = atom_name##_##kind##_fields, .n_##kind##_fields = 1
#define REGISTRY_FIELDS_2(atom_name, kind)             .kind##_fields = atom_name##_##kind##_fields, .n_##kind##_fields = 2
#define REGISTRY_FIELDS_3(atom_name, kind)             .kind##_fields = atom_name##_##kind##_fields, .n_##kind##_fields = 3
#define REGISTRY_FIELDS_4(atom_name, kind)             .kind##_fields = atom_name##_##kind##_fields, .n_##kind##_fields = 4
#define REGISTRY_FIELDS_5(atom_name, kind)             .kind##_fields = atom_name##_##kind##_fields, .n_##kind##_fields = 5

#define ATOM_FLAGS_COMMON (APG_ATOM_RT_SAFE | APG_ATOM_NO_HEAP | APG_ATOM_BOUNDED_CPU)
#define ATOM_FLAGS_PORTABLE (ATOM_FLAGS_COMMON | APG_ATOM_WASM_SAFE | APG_ATOM_M7_SAFE)
#define ATOM_FLAGS_WASM (ATOM_FLAGS_COMMON | APG_ATOM_WASM_SAFE)
#define ATOM_FLAGS_EXPERIMENTAL (APG_ATOM_EXPERIMENTAL | APG_ATOM_LEGACY)

#define REGISTRY_ATOMS(_)                                                                                         \
    _(amplitude_accumulate, amplitude, 0, 1, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                  \
    _(amplitude_latch, amplitude, 1, 2, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                       \
    _(amplitude_add, amplitude, 0, 0, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                         \
    _(amplitude_clip_hard, amplitude, 1, 0, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL)                       \
    _(amplitude_clip_soft, amplitude, 2, 0, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL)                           \
    _(amplitude_divide, amplitude, 1, 0, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                      \
    _(amplitude_multiply, amplitude, 0, 0, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                    \
    _(amplitude_normalize, amplitude, 2, 1, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL)                           \
    _(amplitude_smooth, amplitude, 3, 1, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL)                          \
    _(amplitude_subtract, amplitude, 0, 0, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                    \
    _(delay_fractional, delay, 2, 2, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL)                                  \
    _(delay_line, delay, 1, 2, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL)                                    \
    _(delay_tap_feedback, delay, 1, 0, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL)                                \
    _(delay_tap_feedforward, delay, 1, 0, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL)                             \
    _(delay_unit, delay, 0, 1, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                                \
    _(detect_autocorrelate, detect, 1, 2, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL)                        \
    _(detect_pitch, detect, 2, 2, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL)                                \
    _(detect_envelope, detect, 3, 1, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL)                              \
    _(detect_peak, detect, 3, 1, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL)                                  \
    _(detect_rms, detect, 1, 3, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL)                                   \
    _(detect_slope, detect, 0, 1, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                             \
    _(detect_threshold, detect, 1, 0, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                         \
    _(detect_zero_crossing, detect, 0, 1, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                     \
    _(filter_allpass, filter, 2, 2, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL)                                   \
    _(filter_biquad_coefficients, filter, 5, 2, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL)                   \
    _(filter_biquad, filter, 5, 4, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL)                                \
    _(filter_comb_fb, filter, 2, 2, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL)                                   \
    _(filter_comb_ff, filter, 2, 2, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL)                                   \
    _(filter_dc_block, filter, 1, 2, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL)                              \
    _(filter_differentiate, filter, 0, 1, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                     \
    _(filter_fir, filter, 2, 2, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL)                                       \
    _(filter_integrate, filter, 0, 1, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                         \
    _(freq_fft, freq, 1, 0, ATOM_FLAGS_EXPERIMENTAL, APG_ATOM_MATURITY_EXPERIMENTAL)                              \
    _(freq_ifft, freq, 1, 0, ATOM_FLAGS_EXPERIMENTAL, APG_ATOM_MATURITY_EXPERIMENTAL)                             \
    _(freq_multiply, freq, 1, 0, ATOM_FLAGS_EXPERIMENTAL, APG_ATOM_MATURITY_EXPERIMENTAL)                         \
    _(freq_overlap_add, freq, 2, 1, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL)                              \
    _(freq_overlap_save, freq, 2, 2, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL)                             \
    _(freq_window, freq, 2, 0, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_SAFE_SCALAR)                                    \
    _(freq_shift, freq, 1, 5, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL)                                    \
    _(generation_dc, generation, 1, 0, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                        \
    _(generation_envelope, generation, 5, 2, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL)                      \
    _(generation_impulse, generation, 2, 1, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                   \
    _(generation_lfo, generation, 4, 1, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL)                           \
    _(generation_noise, generation, 2, 2, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                     \
    _(generation_oscillator, generation, 4, 1, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL)                        \
    _(interpolation_cubic, interpolation, 0, 0, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL)                       \
    _(interpolation_lagrange, interpolation, 1, 2, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL)               \
    _(interpolation_linear, interpolation, 0, 0, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)              \
    _(interpolation_sinc, interpolation, 1, 1, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL)                   \
    _(mix_crossfade, mix, 1, 0, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                               \
    _(mix_decode_ms, mix, 0, 0, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                               \
    _(mix_encode_ms, mix, 0, 0, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                               \
    _(mix_matrix, mix, 2, 0, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_SAFE_SCALAR)                                      \
    _(mix_pan_stereo, mix, 1, 0, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                              \
    _(mix_wet_dry, mix, 1, 0, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                                 \
    _(modulation_amplitude, modulation, 1, 0, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL)                     \
    _(modulation_frequency, modulation, 1, 3, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL)                    \
    _(modulation_phase, modulation, 1, 2, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL)                             \
    _(modulation_ring, modulation, 0, 0, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL)                          \
    _(modulation_scrub, modulation, 1, 0, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL)                        \
    _(nonlinear_bitcrush, nonlinear, 1, 0, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL)                        \
    _(nonlinear_sample_hold, nonlinear, 1, 2, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL)                     \
    _(nonlinear_waveshape, nonlinear, 2, 0, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL)                           \
    _(src_antialias, src, 2, 2, ATOM_FLAGS_WASM | APG_ATOM_ANTIALIASED, APG_ATOM_MATURITY_MUSICAL)                \
    _(src_antiimage, src, 2, 2, ATOM_FLAGS_WASM | APG_ATOM_ANTIALIASED, APG_ATOM_MATURITY_MUSICAL)                \
    _(src_convert_format, src, 2, 0, ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR)                          \
    _(src_downsample, src, 1, 0, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_SAFE_SCALAR)                                  \
    _(src_upsample, src, 1, 0, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_SAFE_SCALAR)                                    \
    _(freq_quantize, freq, 0, 0, ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL)

#define REGISTRY_ATOM(atom_name, category_name, config_count, state_count, atom_flags, atom_maturity) \
    {                                                                                                 \
        .name        = #atom_name,                                                                    \
        .category    = #category_name,                                                                \
        .thunk       = atom_name##_thunk,                                                             \
        .out_size    = sizeof(atom_name##_out_t),                                                     \
        .in_size     = sizeof(atom_name##_in_t),                                                      \
        .config_size = sizeof(atom_name##_params_t),                                                  \
        .state_size  = sizeof(atom_name##_state_t),                                                   \
        REGISTRY_FIELDS(atom_name, config, config_count),                                             \
        REGISTRY_FIELDS(atom_name, state, state_count),                                               \
        .flags    = atom_flags,                                                                       \
        .maturity = atom_maturity,                                                                    \
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
    if (strcmp(name, "filter_biquad") == 0) {
        if (out_len)
            *out_len = sizeof(filter_biquad_in_fields) / sizeof(filter_biquad_in_fields[0]);
        return filter_biquad_in_fields;
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
