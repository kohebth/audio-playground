#ifndef AUDIO_PLAYGROUND_ATOM_THUNK_H
#define AUDIO_PLAYGROUND_ATOM_THUNK_H

#include <atom_registry.h>

#define DECLARE_THUNK(atom_name) extern void atom_name##_thunk(atom_call_t *call);

#define DECLARE_THUNK_ALL(_)      \
    _(amplitude_accumulate)       \
    _(amplitude_add)              \
    _(amplitude_clip_hard)        \
    _(amplitude_clip_soft)        \
    _(amplitude_divide)           \
    _(amplitude_latch)            \
    _(amplitude_multiply)         \
    _(amplitude_normalize)        \
    _(amplitude_smooth)           \
    _(amplitude_subtract)         \
    _(delay_fractional)           \
    _(delay_line)                 \
    _(delay_tap_feedback)         \
    _(delay_tap_feedforward)      \
    _(delay_unit)                 \
    _(detect_autocorrelate)       \
    _(detect_envelope)            \
    _(detect_peak)                \
    _(detect_pitch)               \
    _(detect_rms)                 \
    _(detect_slope)               \
    _(detect_threshold)           \
    _(detect_zero_crossing)       \
    _(filter_allpass)             \
    _(filter_biquad_coefficients) \
    _(filter_biquad)              \
    _(filter_comb_fb)             \
    _(filter_comb_ff)             \
    _(filter_dc_block)            \
    _(filter_differentiate)       \
    _(filter_fir)                 \
    _(filter_integrate)           \
    _(freq_overlap_add)           \
    _(freq_overlap_save)          \
    _(freq_quantize)              \
    _(freq_shift)                 \
    _(freq_window)                \
    _(generation_dc)              \
    _(generation_envelope)        \
    _(generation_impulse)         \
    _(generation_lfo)             \
    _(generation_noise)           \
    _(generation_oscillator)      \
    _(interpolation_cubic)        \
    _(interpolation_lagrange)     \
    _(interpolation_linear)       \
    _(interpolation_sinc)         \
    _(mix_crossfade)              \
    _(mix_decode_ms)              \
    _(mix_encode_ms)              \
    _(mix_matrix)                 \
    _(mix_pan_stereo)             \
    _(mix_wet_dry)                \
    _(modulation_amplitude)       \
    _(modulation_frequency)       \
    _(modulation_phase)           \
    _(modulation_ring)            \
    _(modulation_scrub)           \
    _(nonlinear_bitcrush)         \
    _(nonlinear_sample_hold)      \
    _(nonlinear_waveshape)        \
    _(src_antialias)              \
    _(src_antiimage)              \
    _(src_convert_format)         \
    _(src_downsample)             \
    _(src_upsample)               \
    _(freq_fft)                   \
    _(freq_ifft)                  \
    _(freq_multiply)

DECLARE_THUNK_ALL(DECLARE_THUNK)

#endif // AUDIO_PLAYGROUND_ATOM_THUNK_H
