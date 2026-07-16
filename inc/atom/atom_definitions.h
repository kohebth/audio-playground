#ifndef AUDIO_PLAYGROUND_ATOM_DEFINITIONS_H
#define AUDIO_PLAYGROUND_ATOM_DEFINITIONS_H

#include <atom/atom_capability.h>

#define APG_ATOM_FLAGS_COMMON       (APG_ATOM_RT_SAFE | APG_ATOM_NO_HEAP | APG_ATOM_BOUNDED_CPU)
#define APG_ATOM_FLAGS_PORTABLE     (APG_ATOM_FLAGS_COMMON | APG_ATOM_WASM_SAFE | APG_ATOM_M7_SAFE)
#define APG_ATOM_FLAGS_WASM         (APG_ATOM_FLAGS_COMMON | APG_ATOM_WASM_SAFE)
#define APG_ATOM_FLAGS_EXPERIMENTAL (APG_ATOM_EXPERIMENTAL | APG_ATOM_LEGACY)

/* name, category, input fields, config fields, state fields, capabilities, maturity, dispatch */
#define APG_ATOM_DEFINITIONS(X)                                                                                      \
    X(amplitude_accumulate, amplitude, 0, 0, 1, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)     \
    X(amplitude_latch, amplitude, 0, 1, 2, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)          \
    X(amplitude_add, amplitude, 0, 0, 0, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)            \
    X(amplitude_clip_hard, amplitude, 0, 1, 0, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL, PROCESS)          \
    X(amplitude_clip_soft, amplitude, 0, 2, 0, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL, PROCESS)              \
    X(amplitude_divide, amplitude, 0, 1, 0, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)         \
    X(amplitude_multiply, amplitude, 0, 0, 0, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)       \
    X(amplitude_normalize, amplitude, 0, 2, 1, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL, PROCESS)              \
    X(amplitude_smooth, amplitude, 0, 3, 1, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL, PROCESS)             \
    X(amplitude_subtract, amplitude, 0, 0, 0, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)       \
    X(delay_fractional, delay, 0, 2, 3, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL, PROCESS)                     \
    X(delay_line, delay, 0, 1, 3, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL, PROCESS)                       \
    X(delay_tap_feedback, delay, 2, 1, 0, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL, PROCESS)                   \
    X(delay_tap_feedforward, delay, 2, 1, 0, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL, PROCESS)                \
    X(delay_unit, delay, 0, 0, 1, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)                   \
    X(detect_autocorrelate, detect, 0, 1, 3, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL, PROCESS)           \
    X(detect_pitch, detect, 0, 2, 3, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL, PROCESS)                   \
    X(detect_envelope, detect, 0, 3, 1, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL, PROCESS)                 \
    X(detect_peak, detect, 0, 3, 1, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL, PROCESS)                     \
    X(detect_rms, detect, 0, 1, 4, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL, PROCESS)                      \
    X(detect_slope, detect, 0, 0, 1, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)                \
    X(detect_threshold, detect, 0, 1, 0, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)            \
    X(detect_zero_crossing, detect, 0, 0, 1, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)        \
    X(filter_allpass, filter, 0, 2, 3, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL, PROCESS)                      \
    X(filter_biquad_coefficients, filter, 0, 5, 2, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL, PROCESS)      \
    X(filter_biquad, filter, 2, 5, 4, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL, PROCESS)                   \
    X(filter_comb_fb, filter, 0, 2, 3, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL, PROCESS)                      \
    X(filter_comb_ff, filter, 0, 2, 3, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL, PROCESS)                      \
    X(filter_dc_block, filter, 0, 1, 2, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL, PROCESS)                 \
    X(filter_differentiate, filter, 0, 0, 1, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)        \
    X(filter_fir, filter, 0, 2, 3, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL, PROCESS)                          \
    X(filter_integrate, filter, 0, 0, 1, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)            \
    X(freq_fft, freq, 0, 1, 2, APG_ATOM_FLAGS_EXPERIMENTAL, APG_ATOM_MATURITY_EXPERIMENTAL, FFT)                     \
    X(freq_ifft, freq, 0, 1, 2, APG_ATOM_FLAGS_EXPERIMENTAL, APG_ATOM_MATURITY_EXPERIMENTAL, IFFT)                   \
    X(freq_multiply, freq, 0, 1, 0, APG_ATOM_FLAGS_EXPERIMENTAL, APG_ATOM_MATURITY_EXPERIMENTAL, MULTIPLY)           \
    X(freq_overlap_add, freq, 0, 2, 1, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL, OVERLAP_ADD)             \
    X(freq_overlap_save, freq, 0, 2, 2, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL, OVERLAP_SAVE)           \
    X(freq_window, freq, 0, 2, 0, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_SAFE_SCALAR, WINDOW)                        \
    X(freq_shift, freq, 0, 1, 5, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL, PROCESS)                       \
    X(generation_dc, generation, 0, 1, 0, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)           \
    X(generation_envelope, generation, 0, 5, 2, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL, PROCESS)         \
    X(generation_impulse, generation, 0, 2, 1, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)      \
    X(generation_lfo, generation, 0, 4, 1, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL, PROCESS)              \
    X(generation_noise, generation, 0, 2, 2, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)        \
    X(generation_oscillator, generation, 0, 4, 1, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL, PROCESS)           \
    X(interpolation_cubic, interpolation, 0, 0, 0, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL, PROCESS)          \
    X(interpolation_lagrange, interpolation, 0, 1, 2, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL, PROCESS)  \
    X(interpolation_linear, interpolation, 0, 0, 0, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS) \
    X(interpolation_sinc, interpolation, 0, 1, 1, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL, PROCESS)      \
    X(mix_crossfade, mix, 0, 1, 0, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)                  \
    X(mix_decode_ms, mix, 0, 0, 0, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)                  \
    X(mix_encode_ms, mix, 0, 0, 0, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)                  \
    X(mix_matrix, mix, 0, 2, 0, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)                         \
    X(mix_pan_stereo, mix, 0, 1, 0, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)                 \
    X(mix_wet_dry, mix, 0, 1, 0, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)                    \
    X(modulation_amplitude, modulation, 0, 1, 0, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL, PROCESS)        \
    X(modulation_frequency, modulation, 0, 1, 4, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL, PROCESS)       \
    X(modulation_phase, modulation, 0, 1, 3, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL, PROCESS)                \
    X(modulation_ring, modulation, 0, 0, 0, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL, PROCESS)             \
    X(modulation_scrub, modulation, 0, 1, 0, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL, PROCESS)           \
    X(nonlinear_bitcrush, nonlinear, 0, 1, 0, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL, PROCESS)           \
    X(nonlinear_sample_hold, nonlinear, 0, 1, 2, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_MUSICAL, PROCESS)        \
    X(nonlinear_waveshape, nonlinear, 0, 2, 0, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_MUSICAL, PROCESS)              \
    X(src_antialias, src, 0, 2, 2, APG_ATOM_FLAGS_WASM | APG_ATOM_ANTIALIASED, APG_ATOM_MATURITY_MUSICAL, PROCESS)   \
    X(src_antiimage, src, 0, 2, 2, APG_ATOM_FLAGS_WASM | APG_ATOM_ANTIALIASED, APG_ATOM_MATURITY_MUSICAL, PROCESS)   \
    X(src_convert_format, src, 0, 2, 0, APG_ATOM_FLAGS_PORTABLE, APG_ATOM_MATURITY_SAFE_SCALAR, PROCESS)             \
    X(src_downsample, src, 0, 1, 1, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_SAFE_SCALAR, STREAM)                      \
    X(src_upsample, src, 0, 1, 1, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_SAFE_SCALAR, STREAM)                        \
    X(freq_quantize, freq, 0, 0, 0, APG_ATOM_FLAGS_WASM, APG_ATOM_MATURITY_EXPERIMENTAL, PROCESS)

/* atom, input contract profile, output contract profile, config contract profile */
#define APG_ATOM_CONTRACT_DEFINITIONS(X)                                      \
    X(generation_dc, NONE, SIGNAL, GENERATION_DC_CONFIG)                      \
    X(generation_lfo, NONE, SIGNAL, GENERATION_LFO_CONFIG)                    \
    X(amplitude_multiply, PAIR, SIGNAL, NONE)                                 \
    X(amplitude_add, PAIR, SIGNAL, NONE)                                      \
    X(amplitude_subtract, PAIR, SIGNAL, NONE)                                 \
    X(amplitude_clip_hard, SIGNAL, SIGNAL, CLIP_HARD_CONFIG)                  \
    X(amplitude_clip_soft, SIGNAL, SIGNAL, CLIP_SOFT_CONFIG)                  \
    X(delay_unit, SIGNAL, SIGNAL, NONE)                                       \
    X(delay_line, SIGNAL, SIGNAL, DELAY_LINE_CONFIG)                          \
    X(delay_fractional, SIGNAL, SIGNAL, DELAY_FRACTIONAL_CONFIG)              \
    X(delay_tap_feedback, DELAY_TAP_INPUT, SIGNAL, DELAY_TAP_CONFIG)          \
    X(delay_tap_feedforward, DELAY_TAP_INPUT, SIGNAL, DELAY_TAP_CONFIG)       \
    X(filter_biquad_coefficients, SIGNAL, SIGNAL, BIQUAD_COEFFICIENTS_CONFIG) \
    X(filter_biquad, BIQUAD_INPUT, SIGNAL, BIQUAD_CONFIG)                     \
    X(filter_allpass, SIGNAL, SIGNAL, FILTER_DELAY_CONFIG)                    \
    X(filter_comb_ff, SIGNAL, SIGNAL, FILTER_DELAY_CONFIG)                    \
    X(filter_comb_fb, COMB_FB_INPUT, SIGNAL, FILTER_DELAY_CONFIG)             \
    X(filter_dc_block, SIGNAL, SIGNAL, DC_BLOCK_CONFIG)                       \
    X(detect_threshold, SIGNAL, GATE, THRESHOLD_CONFIG)                       \
    X(modulation_amplitude, SIGNAL_MODULATOR, SIGNAL, DEPTH_CONFIG)           \
    X(modulation_frequency, SIGNAL_MODULATOR, SIGNAL, DEPTH_CONFIG)           \
    X(modulation_phase, SIGNAL_MODULATOR, SIGNAL, DEPTH_CONFIG)               \
    X(modulation_ring, SIGNAL_MODULATOR, SIGNAL, NONE)                        \
    X(modulation_scrub, SCRUB_INPUT, SIGNAL, SCRUB_CONFIG)                    \
    X(mix_crossfade, PAIR, SIGNAL, CROSSFADE_CONFIG)                          \
    X(mix_wet_dry, WET_DRY_INPUT, SIGNAL, WET_DRY_CONFIG)                     \
    X(mix_matrix, MIX_MATRIX_IO, MIX_MATRIX_IO, MIX_MATRIX_CONFIG)            \
    X(mix_pan_stereo, SIGNAL, STEREO_OUTPUT, PAN_CONFIG)                      \
    X(mix_encode_ms, STEREO_INPUT, MS_OUTPUT, NONE)                           \
    X(mix_decode_ms, MS_INPUT, STEREO_OUTPUT, NONE)

#endif // AUDIO_PLAYGROUND_ATOM_DEFINITIONS_H
