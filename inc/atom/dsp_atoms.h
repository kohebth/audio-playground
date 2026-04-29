#ifndef DSP_ATOMS_H
#define DSP_ATOMS_H

#include "dsp_types.h"

#define ATOM(_) void _(_##_out_t *, _##_in_t *, _##_params_t *, _##_state_t *);

#define DECLARE_ALL(_)        \
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

DECLARE_ALL(ATOM);

#endif // DSP_ATOMS_H