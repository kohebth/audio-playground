#ifndef DSP_ATOMS_H
#define DSP_ATOMS_H

#include <apgcore/runtime/process.h>
#include <apgcore/runtime/spectral.h>

#include "dsp_types.h"

#define ATOM(_) void _(_##_out_t *, _##_in_t *, _##_params_t *, _##_state_t *);

#define DECLARE_ALL(_)            \
    _(generation_oscillator)      \
    _(generation_noise)           \
    _(generation_envelope)        \
    _(generation_lfo)             \
    _(generation_impulse)         \
    _(generation_dc)              \
    _(amplitude_multiply)         \
    _(amplitude_divide)           \
    _(amplitude_smooth)           \
    _(amplitude_clip_hard)        \
    _(amplitude_clip_soft)        \
    _(amplitude_normalize)        \
    _(amplitude_add)              \
    _(amplitude_subtract)         \
    _(amplitude_accumulate)       \
    _(amplitude_latch)            \
    _(delay_unit)                 \
    _(delay_line)                 \
    _(delay_fractional)           \
    _(delay_tap_feedback)         \
    _(delay_tap_feedforward)      \
    _(filter_fir)                 \
    _(filter_biquad_coefficients) \
    _(filter_biquad)              \
    _(filter_dc_block)            \
    _(filter_comb_ff)             \
    _(filter_comb_fb)             \
    _(filter_allpass)             \
    _(filter_integrate)           \
    _(filter_differentiate)       \
    _(detect_peak)                \
    _(detect_envelope)            \
    _(detect_threshold)           \
    _(detect_rms)                 \
    _(detect_zero_crossing)       \
    _(detect_slope)               \
    _(detect_autocorrelate)       \
    _(detect_pitch)               \
    _(modulation_phase)           \
    _(modulation_ring)            \
    _(modulation_amplitude)       \
    _(modulation_frequency)       \
    _(modulation_scrub)           \
    _(interpolation_linear)       \
    _(interpolation_cubic)        \
    _(interpolation_sinc)         \
    _(interpolation_lagrange)     \
    _(src_upsample)               \
    _(src_downsample)             \
    _(src_antialias)              \
    _(src_antiimage)              \
    _(src_convert_format)         \
    _(freq_fft)                   \
    _(freq_ifft)                  \
    _(freq_window)                \
    _(freq_multiply)              \
    _(freq_overlap_add)           \
    _(freq_overlap_save)          \
    _(freq_shift)                 \
    _(freq_quantize)              \
    _(mix_crossfade)              \
    _(mix_wet_dry)                \
    _(mix_matrix)                 \
    _(mix_pan_stereo)             \
    _(mix_encode_ms)              \
    _(mix_decode_ms)              \
    _(nonlinear_waveshape)        \
    _(nonlinear_bitcrush)         \
    _(nonlinear_sample_hold)

DECLARE_ALL(ATOM);

void freq_fft_process(
    freq_fft_out_t            *out,
    freq_fft_in_t             *in,
    freq_fft_params_t         *params,
    freq_fft_state_t          *state,
    const apg_spectral_info_t *spectral_info
);
void freq_ifft_process(
    freq_ifft_out_t           *out,
    freq_ifft_in_t            *in,
    freq_ifft_params_t        *params,
    freq_ifft_state_t         *state,
    const apg_spectral_info_t *spectral_info
);
void freq_multiply_process(
    freq_multiply_out_t       *out,
    freq_multiply_in_t        *in,
    freq_multiply_params_t    *params,
    freq_multiply_state_t     *state,
    const apg_spectral_info_t *spectral_info
);

void amplitude_multiply_process(
    amplitude_multiply_out_t    *out,
    amplitude_multiply_in_t     *in,
    amplitude_multiply_params_t *params,
    amplitude_multiply_state_t  *state,
    const apg_process_info_t    *info
);
void amplitude_clip_soft_process(
    amplitude_clip_soft_out_t    *out,
    amplitude_clip_soft_in_t     *in,
    amplitude_clip_soft_params_t *params,
    amplitude_clip_soft_state_t  *state,
    const apg_process_info_t     *info
);
void delay_line_process(
    delay_line_out_t         *out,
    delay_line_in_t          *in,
    delay_line_params_t      *params,
    delay_line_state_t       *state,
    const apg_process_info_t *info
);
void filter_biquad_coefficients_process(
    filter_biquad_coefficients_out_t    *out,
    filter_biquad_coefficients_in_t     *in,
    filter_biquad_coefficients_params_t *params,
    filter_biquad_coefficients_state_t  *state,
    const apg_process_info_t            *info
);
void filter_biquad_process(
    filter_biquad_out_t      *out,
    filter_biquad_in_t       *in,
    filter_biquad_params_t   *params,
    filter_biquad_state_t    *state,
    const apg_process_info_t *info
);

void generation_dc_process(
    generation_dc_out_t      *out,
    generation_dc_in_t       *in,
    generation_dc_params_t   *params,
    generation_dc_state_t    *state,
    const apg_process_info_t *info
);
void generation_lfo_process(
    generation_lfo_out_t     *out,
    generation_lfo_in_t      *in,
    generation_lfo_params_t  *params,
    generation_lfo_state_t   *state,
    const apg_process_info_t *info
);
void mix_wet_dry_process(
    mix_wet_dry_out_t        *out,
    mix_wet_dry_in_t         *in,
    mix_wet_dry_params_t     *params,
    mix_wet_dry_state_t      *state,
    const apg_process_info_t *info
);
void modulation_amplitude_process(
    modulation_amplitude_out_t    *out,
    modulation_amplitude_in_t     *in,
    modulation_amplitude_params_t *params,
    modulation_amplitude_state_t  *state,
    const apg_process_info_t      *info
);

void amplitude_smooth_process(
    amplitude_smooth_out_t    *out,
    amplitude_smooth_in_t     *in,
    amplitude_smooth_params_t *params,
    amplitude_smooth_state_t  *state,
    const apg_process_info_t  *info
);
void detect_envelope_process(
    detect_envelope_out_t    *out,
    detect_envelope_in_t     *in,
    detect_envelope_params_t *params,
    detect_envelope_state_t  *state,
    const apg_process_info_t *info
);
void detect_peak_process(
    detect_peak_out_t        *out,
    detect_peak_in_t         *in,
    detect_peak_params_t     *params,
    detect_peak_state_t      *state,
    const apg_process_info_t *info
);
void detect_threshold_process(
    detect_threshold_out_t    *out,
    detect_threshold_in_t     *in,
    detect_threshold_params_t *params,
    detect_threshold_state_t  *state,
    const apg_process_info_t  *info
);

void delay_unit_process(
    delay_unit_out_t         *out,
    delay_unit_in_t          *in,
    delay_unit_params_t      *params,
    delay_unit_state_t       *state,
    const apg_process_info_t *info
);
void delay_fractional_process(
    delay_fractional_out_t    *out,
    delay_fractional_in_t     *in,
    delay_fractional_params_t *params,
    delay_fractional_state_t  *state,
    const apg_process_info_t  *info
);
void delay_tap_feedback_process(
    delay_tap_feedback_out_t    *out,
    delay_tap_feedback_in_t     *in,
    delay_tap_feedback_params_t *params,
    delay_tap_feedback_state_t  *state,
    const apg_process_info_t    *info
);
void delay_tap_feedforward_process(
    delay_tap_feedforward_out_t    *out,
    delay_tap_feedforward_in_t     *in,
    delay_tap_feedforward_params_t *params,
    delay_tap_feedforward_state_t  *state,
    const apg_process_info_t       *info
);

void amplitude_add_process(
    amplitude_add_out_t      *out,
    amplitude_add_in_t       *in,
    amplitude_add_params_t   *params,
    amplitude_add_state_t    *state,
    const apg_process_info_t *info
);
void amplitude_subtract_process(
    amplitude_subtract_out_t    *out,
    amplitude_subtract_in_t     *in,
    amplitude_subtract_params_t *params,
    amplitude_subtract_state_t  *state,
    const apg_process_info_t    *info
);
void amplitude_divide_process(
    amplitude_divide_out_t    *out,
    amplitude_divide_in_t     *in,
    amplitude_divide_params_t *params,
    amplitude_divide_state_t  *state,
    const apg_process_info_t  *info
);
void mix_crossfade_process(
    mix_crossfade_out_t      *out,
    mix_crossfade_in_t       *in,
    mix_crossfade_params_t   *params,
    mix_crossfade_state_t    *state,
    const apg_process_info_t *info
);
void amplitude_clip_hard_process(
    amplitude_clip_hard_out_t    *out,
    amplitude_clip_hard_in_t     *in,
    amplitude_clip_hard_params_t *params,
    amplitude_clip_hard_state_t  *state,
    const apg_process_info_t     *info
);
void amplitude_accumulate_process(
    amplitude_accumulate_out_t    *out,
    amplitude_accumulate_in_t     *in,
    amplitude_accumulate_params_t *params,
    amplitude_accumulate_state_t  *state,
    const apg_process_info_t      *info
);
void amplitude_latch_process(
    amplitude_latch_out_t    *out,
    amplitude_latch_in_t     *in,
    amplitude_latch_params_t *params,
    amplitude_latch_state_t  *state,
    const apg_process_info_t *info
);
void amplitude_normalize_process(
    amplitude_normalize_out_t    *out,
    amplitude_normalize_in_t     *in,
    amplitude_normalize_params_t *params,
    amplitude_normalize_state_t  *state,
    const apg_process_info_t     *info
);
void freq_window_process(
    freq_window_out_t        *out,
    freq_window_in_t         *in,
    freq_window_params_t     *params,
    freq_window_state_t      *state,
    const apg_process_info_t *info
);
void freq_quantize_process(
    freq_quantize_out_t      *out,
    freq_quantize_in_t       *in,
    freq_quantize_params_t   *params,
    freq_quantize_state_t    *state,
    const apg_process_info_t *info
);
void freq_shift_process(
    freq_shift_out_t         *out,
    freq_shift_in_t          *in,
    freq_shift_params_t      *params,
    freq_shift_state_t       *state,
    const apg_process_info_t *info
);
void freq_overlap_add_process(
    freq_overlap_add_out_t    *out,
    freq_overlap_add_in_t     *in,
    freq_overlap_add_params_t *params,
    freq_overlap_add_state_t  *state,
    const apg_process_info_t  *info
);
void freq_overlap_save_process(
    freq_overlap_save_out_t    *out,
    freq_overlap_save_in_t     *in,
    freq_overlap_save_params_t *params,
    freq_overlap_save_state_t  *state,
    const apg_process_info_t   *info
);
void interpolation_linear_process(
    interpolation_linear_out_t    *out,
    interpolation_linear_in_t     *in,
    interpolation_linear_params_t *params,
    interpolation_linear_state_t  *state,
    const apg_process_info_t      *info
);
void interpolation_cubic_process(
    interpolation_cubic_out_t    *out,
    interpolation_cubic_in_t     *in,
    interpolation_cubic_params_t *params,
    interpolation_cubic_state_t  *state,
    const apg_process_info_t     *info
);
void interpolation_lagrange_process(
    interpolation_lagrange_out_t    *out,
    interpolation_lagrange_in_t     *in,
    interpolation_lagrange_params_t *params,
    interpolation_lagrange_state_t  *state,
    const apg_process_info_t        *info
);
void interpolation_sinc_process(
    interpolation_sinc_out_t    *out,
    interpolation_sinc_in_t     *in,
    interpolation_sinc_params_t *params,
    interpolation_sinc_state_t  *state,
    const apg_process_info_t    *info
);
void src_convert_format_process(
    src_convert_format_out_t    *out,
    src_convert_format_in_t     *in,
    src_convert_format_params_t *params,
    src_convert_format_state_t  *state,
    const apg_process_info_t    *info
);
void src_upsample_process(
    src_upsample_out_t       *out,
    src_upsample_in_t        *in,
    src_upsample_params_t    *params,
    src_upsample_state_t     *state,
    const apg_process_info_t *info
);
void src_downsample_process(
    src_downsample_out_t     *out,
    src_downsample_in_t      *in,
    src_downsample_params_t  *params,
    src_downsample_state_t   *state,
    const apg_process_info_t *info
);
void src_antialias_process(
    src_antialias_out_t      *out,
    src_antialias_in_t       *in,
    src_antialias_params_t   *params,
    src_antialias_state_t    *state,
    const apg_process_info_t *info
);
void src_antiimage_process(
    src_antiimage_out_t      *out,
    src_antiimage_in_t       *in,
    src_antiimage_params_t   *params,
    src_antiimage_state_t    *state,
    const apg_process_info_t *info
);

void generation_impulse_process(
    generation_impulse_out_t    *out,
    generation_impulse_in_t     *in,
    generation_impulse_params_t *params,
    generation_impulse_state_t  *state,
    const apg_process_info_t    *info
);
void generation_noise_process(
    generation_noise_out_t    *out,
    generation_noise_in_t     *in,
    generation_noise_params_t *params,
    generation_noise_state_t  *state,
    const apg_process_info_t  *info
);
void generation_envelope_process(
    generation_envelope_out_t    *out,
    generation_envelope_in_t     *in,
    generation_envelope_params_t *params,
    generation_envelope_state_t  *state,
    const apg_process_info_t     *info
);
void generation_oscillator_process(
    generation_oscillator_out_t    *out,
    generation_oscillator_in_t     *in,
    generation_oscillator_params_t *params,
    generation_oscillator_state_t  *state,
    const apg_process_info_t       *info
);

void modulation_ring_process(
    modulation_ring_out_t    *out,
    modulation_ring_in_t     *in,
    modulation_ring_params_t *params,
    modulation_ring_state_t  *state,
    const apg_process_info_t *info
);
void modulation_frequency_process(
    modulation_frequency_out_t    *out,
    modulation_frequency_in_t     *in,
    modulation_frequency_params_t *params,
    modulation_frequency_state_t  *state,
    const apg_process_info_t      *info
);
void modulation_phase_process(
    modulation_phase_out_t    *out,
    modulation_phase_in_t     *in,
    modulation_phase_params_t *params,
    modulation_phase_state_t  *state,
    const apg_process_info_t  *info
);
void modulation_scrub_process(
    modulation_scrub_out_t    *out,
    modulation_scrub_in_t     *in,
    modulation_scrub_params_t *params,
    modulation_scrub_state_t  *state,
    const apg_process_info_t  *info
);

void detect_slope_process(
    detect_slope_out_t       *out,
    detect_slope_in_t        *in,
    detect_slope_params_t    *params,
    detect_slope_state_t     *state,
    const apg_process_info_t *info
);
void detect_rms_process(
    detect_rms_out_t         *out,
    detect_rms_in_t          *in,
    detect_rms_params_t      *params,
    detect_rms_state_t       *state,
    const apg_process_info_t *info
);
void detect_zero_crossing_process(
    detect_zero_crossing_out_t    *out,
    detect_zero_crossing_in_t     *in,
    detect_zero_crossing_params_t *params,
    detect_zero_crossing_state_t  *state,
    const apg_process_info_t      *info
);
void detect_autocorrelate_process(
    detect_autocorrelate_out_t    *out,
    detect_autocorrelate_in_t     *in,
    detect_autocorrelate_params_t *params,
    detect_autocorrelate_state_t  *state,
    const apg_process_info_t      *info
);
void detect_pitch_process(
    detect_pitch_out_t       *out,
    detect_pitch_in_t        *in,
    detect_pitch_params_t    *params,
    detect_pitch_state_t     *state,
    const apg_process_info_t *info
);

void filter_allpass_process(
    filter_allpass_out_t     *out,
    filter_allpass_in_t      *in,
    filter_allpass_params_t  *params,
    filter_allpass_state_t   *state,
    const apg_process_info_t *info
);
void filter_comb_ff_process(
    filter_comb_ff_out_t     *out,
    filter_comb_ff_in_t      *in,
    filter_comb_ff_params_t  *params,
    filter_comb_ff_state_t   *state,
    const apg_process_info_t *info
);
void filter_comb_fb_process(
    filter_comb_fb_out_t     *out,
    filter_comb_fb_in_t      *in,
    filter_comb_fb_params_t  *params,
    filter_comb_fb_state_t   *state,
    const apg_process_info_t *info
);
void filter_dc_block_process(
    filter_dc_block_out_t    *out,
    filter_dc_block_in_t     *in,
    filter_dc_block_params_t *params,
    filter_dc_block_state_t  *state,
    const apg_process_info_t *info
);
void filter_differentiate_process(
    filter_differentiate_out_t    *out,
    filter_differentiate_in_t     *in,
    filter_differentiate_params_t *params,
    filter_differentiate_state_t  *state,
    const apg_process_info_t      *info
);
void filter_integrate_process(
    filter_integrate_out_t    *out,
    filter_integrate_in_t     *in,
    filter_integrate_params_t *params,
    filter_integrate_state_t  *state,
    const apg_process_info_t  *info
);
void filter_fir_process(
    filter_fir_out_t         *out,
    filter_fir_in_t          *in,
    filter_fir_params_t      *params,
    filter_fir_state_t       *state,
    const apg_process_info_t *info
);
void mix_matrix_process(
    mix_matrix_out_t         *out,
    mix_matrix_in_t          *in,
    mix_matrix_params_t      *params,
    mix_matrix_state_t       *state,
    const apg_process_info_t *info
);
void mix_pan_stereo_process(
    mix_pan_stereo_out_t     *out,
    mix_pan_stereo_in_t      *in,
    mix_pan_stereo_params_t  *params,
    mix_pan_stereo_state_t   *state,
    const apg_process_info_t *info
);
void mix_encode_ms_process(
    mix_encode_ms_out_t      *out,
    mix_encode_ms_in_t       *in,
    mix_encode_ms_params_t   *params,
    mix_encode_ms_state_t    *state,
    const apg_process_info_t *info
);
void mix_decode_ms_process(
    mix_decode_ms_out_t      *out,
    mix_decode_ms_in_t       *in,
    mix_decode_ms_params_t   *params,
    mix_decode_ms_state_t    *state,
    const apg_process_info_t *info
);
void nonlinear_bitcrush_process(
    nonlinear_bitcrush_out_t    *out,
    nonlinear_bitcrush_in_t     *in,
    nonlinear_bitcrush_params_t *params,
    nonlinear_bitcrush_state_t  *state,
    const apg_process_info_t    *info
);
void nonlinear_waveshape_process(
    nonlinear_waveshape_out_t    *out,
    nonlinear_waveshape_in_t     *in,
    nonlinear_waveshape_params_t *params,
    nonlinear_waveshape_state_t  *state,
    const apg_process_info_t     *info
);
void nonlinear_sample_hold_process(
    nonlinear_sample_hold_out_t    *out,
    nonlinear_sample_hold_in_t     *in,
    nonlinear_sample_hold_params_t *params,
    nonlinear_sample_hold_state_t  *state,
    const apg_process_info_t       *info
);

#endif // DSP_ATOMS_H
