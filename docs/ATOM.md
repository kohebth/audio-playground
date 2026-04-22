# DSP Atomic Functions

## 🔷 Signal Generation

```c++
void generation_oscillator(
    generation_oscillator_out_t      out,      // { float *signal; }
    void *                           in,      // { /* no members */ }
    generation_oscillator_params_t   params,      // { float frequency; int waveform; float phase_offset; float sample_rate; }
    generation_oscillator_state_t *  state       // { float phase; }
);

void generation_noise(
    generation_noise_out_t           out,      // { float *signal; }
    void *                           in,      // { /* no members */ }
    generation_noise_params_t        params,      // { float amplitude; int color; }
    generation_noise_state_t *       state       // { uint32_t seed; float prev_value; }
);

void generation_envelope(
    generation_envelope_out_t        out,      // { float *signal; }
    generation_envelope_in_t         in,      // { float *gate; }
    generation_envelope_params_t     params,      // { float attack; float decay; float sustain; float release; float sample_rate; }
    generation_envelope_state_t *    state       // { float current_level; int stage; }
);

void generation_lfo(
    generation_lfo_out_t             out,      // { float *signal; }
    void *                           in,      // { /* no members */ }
    generation_lfo_params_t          params,      // { float frequency; int waveform; float phase_offset; float sample_rate; }
    generation_lfo_state_t *         state       // { float phase; }
);

void generation_impulse(
    generation_impulse_out_t         out,      // { float *signal; }
    void *                           in,      // { /* no members */ }
    generation_impulse_params_t      params,      // { float interval; float sample_rate; }
    generation_impulse_state_t *     state       // { int counter; }
);

void generation_dc(
    generation_dc_out_t              out,      // { float *signal; }
    void *                           in,      // { /* no members */ }
    generation_dc_params_t           params,      // { float value; }
    void *                           state       // { /* no members */ }
);
```

---

## 🔷 Gain & Amplitude

```c++
void amplitude_multiply(
    amplitude_multiply_out_t         out,      // { float *signal; }
    amplitude_multiply_in_t          in,      // { float *signal_a; float *signal_b; }
    void *                           params,      // { /* no members */ }
    void *                           state       // { /* no members */ }
);

void amplitude_divide(
    amplitude_divide_out_t           out,      // { float *signal; }
    amplitude_divide_in_t            in,      // { float *numerator; float *denominator; }
    amplitude_divide_params_t        params,      // { float epsilon; }
    void *                           state       // { /* no members */ }
);

void amplitude_smooth(
    amplitude_smooth_out_t           out,      // { float *signal; }
    amplitude_smooth_in_t            in,      // { float *signal; }
    amplitude_smooth_params_t        params,      // { float attack; float release; float sample_rate; }
    amplitude_smooth_state_t *       state       // { float prev_value; }
);

void amplitude_clip_hard(
    amplitude_clip_hard_out_t        out,      // { float *signal; }
    amplitude_clip_hard_in_t         in,      // { float *signal; }
    amplitude_clip_hard_params_t     params,      // { float threshold; }
    void *                           state       // { /* no members */ }
);

void amplitude_clip_soft(
    amplitude_clip_soft_out_t        out,      // { float *signal; }
    amplitude_clip_soft_in_t         in,      // { float *signal; }
    amplitude_clip_soft_params_t     params,      // { float threshold; int curve; }
    void *                           state       // { /* no members */ }
);

void amplitude_normalize(
    amplitude_normalize_out_t        out,      // { float *signal; }
    amplitude_normalize_in_t         in,      // { float *signal; }
    amplitude_normalize_params_t     params,      // { float target_level; int mode; }
    amplitude_normalize_state_t *    state       // { float running_peak; }
);

void amplitude_add(
    amplitude_add_out_t              out,      // { float *signal; }
    amplitude_add_in_t               in,      // { float *signal_a; float *signal_b; }
    void *                           params,      // { /* no members */ }
    void *                           state       // { /* no members */ }
);

void amplitude_subtract(
    amplitude_subtract_out_t         out,      // { float *signal; }
    amplitude_subtract_in_t          in,      // { float *signal_a; float *signal_b; }
    void *                           params,      // { /* no members */ }
    void *                           state       // { /* no members */ }
);

void amplitude_accumulate(
    amplitude_accumulate_out_t       out,      // { float *signal; }
    amplitude_accumulate_in_t        in,      // { float *signal; }
    void *                           params,      // { /* no members */ }
    amplitude_accumulate_state_t *   state       // { float accumulator; }
);
```

---

## 🔷 Delay & Memory

```c++
void delay_unit(
    delay_unit_out_t                 out,      // { float *signal; }
    delay_unit_in_t                  in,      // { float *signal; }
    void *                           params,      // { /* no members */ }
    delay_unit_state_t *             state       // { float prev_sample; }
);

void delay_line(
    delay_line_out_t                 out,      // { float *signal; }
    delay_line_in_t                  in,      // { float *signal; }
    delay_line_params_t              params,      // { int length; float sample_rate; }
    delay_line_state_t *             state       // { float *buffer; int write_pos; }
);

void delay_fractional(
    delay_fractional_out_t           out,      // { float *signal; }
    delay_fractional_in_t            in,      // { float *signal; }
    delay_fractional_params_t        params,      // { float delay_samples; int interpolation; }
    delay_fractional_state_t *       state       // { float *buffer; int write_pos; }
);

void delay_tap_feedback(
    delay_tap_feedback_out_t         out,      // { float *signal; }
    delay_tap_feedback_in_t          in,      // { float *buffer; int tap_position; }
    delay_tap_feedback_params_t      params,      // { float coefficient; }
    void *                           state       // { /* no members */ }
);

void delay_tap_feedforward(
    delay_tap_feedforward_out_t      out,      // { float *signal; }
    delay_tap_feedforward_in_t       in,      // { float *buffer; int tap_position; }
    delay_tap_feedforward_params_t   params,      // { float coefficient; }
    void *                           state       // { /* no members */ }
);
```

---

## 🔷 Filtering

```c++
void filter_fir(
    filter_fir_out_t                 out,      // { float *signal; }
    filter_fir_in_t                  in,      // { float *signal; }
    filter_fir_params_t              params,      // { float *kernel; int kernel_size; }
    filter_fir_state_t *             state       // { float *buffer; int write_pos; }
);

void filter_biquad(
    filter_biquad_out_t              out,      // { float *signal; }
    filter_biquad_in_t               in,      // { float *signal; }
    filter_biquad_params_t           params,      // { float b0; float b1; float b2; float a1; float a2; }
    filter_biquad_state_t *          state       // { float z1; float z2; }
);

void filter_dc_block(
    filter_dc_block_out_t            out,      // { float *signal; }
    filter_dc_block_in_t             in,      // { float *signal; }
    filter_dc_block_params_t         params,      // { float coefficient; }
    filter_dc_block_state_t *        state       // { float prev_input; float prev_output; }
);

void filter_comb_ff(
    filter_comb_ff_out_t             out,      // { float *signal; }
    filter_comb_ff_in_t              in,      // { float *signal; }
    filter_comb_ff_params_t          params,      // { int delay_samples; float coefficient; }
    filter_comb_ff_state_t *         state       // { float *buffer; int write_pos; }
);

void filter_comb_fb(
    filter_comb_fb_out_t             out,      // { float *signal; }
    filter_comb_fb_in_t              in,      // { float *signal; }
    filter_comb_fb_params_t          params,      // { int delay_samples; float coefficient; }
    filter_comb_fb_state_t *         state       // { float *buffer; int write_pos; }
);

void filter_allpass(
    filter_allpass_out_t             out,      // { float *signal; }
    filter_allpass_in_t              in,      // { float *signal; }
    filter_allpass_params_t          params,      // { int delay_samples; float coefficient; }
    filter_allpass_state_t *         state       // { float *buffer; int write_pos; }
);

void filter_integrate(
    filter_integrate_out_t           out,      // { float *signal; }
    filter_integrate_in_t            in,      // { float *signal; }
    void *                           params,      // { /* no members */ }
    filter_integrate_state_t *       state       // { float accumulator; }
);

void filter_differentiate(
    filter_differentiate_out_t       out,      // { float *signal; }
    filter_differentiate_in_t        in,      // { float *signal; }
    void *                           params,      // { /* no members */ }
    filter_differentiate_state_t *   state       // { float prev_sample; }
);
```

---

## 🔷 Detection & Analysis

```c++
void detect_peak(
    detect_peak_out_t                out,      // { float *level; }
    detect_peak_in_t                 in,      // { float *signal; }
    detect_peak_params_t             params,      // { float attack; float release; float sample_rate; }
    detect_peak_state_t *            state       // { float prev_peak; }
);

void detect_envelope(
    detect_envelope_out_t            out,      // { float *envelope; }
    detect_envelope_in_t             in,      // { float *signal; }
    detect_envelope_params_t         params,      // { float attack; float release; float sample_rate; }
    detect_envelope_state_t *        state       // { float prev_envelope; }
);

void detect_threshold(
    detect_threshold_out_t           out,      // { float *gate; }
    detect_threshold_in_t            in,      // { float *signal; }
    detect_threshold_params_t        params,      // { float threshold; }
    void *                           state       // { /* no members */ }
);

void detect_rms(
    detect_rms_out_t                 out,      // { float *level; }
    detect_rms_in_t                  in,      // { float *signal; }
    detect_rms_params_t              params,      // { int window_size; }
    detect_rms_state_t *             state       // { float *buffer; int write_pos; float sum; }
);

void detect_zero_crossing(
    detect_zero_crossing_out_t       out,      // { float *trigger; }
    detect_zero_crossing_in_t        in,      // { float *signal; }
    void *                           params,      // { /* no members */ }
    detect_zero_crossing_state_t *   state       // { float prev_sample; }
);

void detect_slope(
    detect_slope_out_t               out,      // { float *slope; }
    detect_slope_in_t                in,      // { float *signal; }
    void *                           params,      // { /* no members */ }
    detect_slope_state_t *           state       // { float prev_sample; }
);

void detect_autocorrelate(
    detect_autocorrelate_out_t       out,      // { float *correlation; }
    detect_autocorrelate_in_t        in,      // { float *signal; }
    detect_autocorrelate_params_t    params,      // { int max_lag; }
    detect_autocorrelate_state_t *   state       // { float *buffer; int write_pos; }
);
```

---

## 🔷 Modulation

```c++
void modulation_phase(
    modulation_phase_out_t           out,      // { float *signal; }
    modulation_phase_in_t            in,      // { float *signal; float *modulator; }
    modulation_phase_params_t        params,      // { float depth; }
    modulation_phase_state_t *       state       // { float *buffer; int write_pos; }
);

void modulation_ring(
    modulation_ring_out_t            out,      // { float *signal; }
    modulation_ring_in_t             in,      // { float *signal; float *modulator; }
    void *                           params,      // { /* no members */ }
    void *                           state       // { /* no members */ }
);

void modulation_amplitude(
    modulation_amplitude_out_t       out,      // { float *signal; }
    modulation_amplitude_in_t        in,      // { float *signal; float *modulator; }
    modulation_amplitude_params_t    params,      // { float depth; }
    void *                           state       // { /* no members */ }
);

void modulation_frequency(
    modulation_frequency_out_t       out,      // { float *signal; }
    modulation_frequency_in_t        in,      // { float *signal; float *modulator; }
    modulation_frequency_params_t    params,      // { float depth; float sample_rate; }
    modulation_frequency_state_t *   state       // { float *buffer; int write_pos; float current_delay; }
);

void modulation_scrub(
    modulation_scrub_out_t           out,      // { float *signal; }
    modulation_scrub_in_t            in,      // { float *buffer; float *position; }
    modulation_scrub_params_t        params,      // { int buffer_size; }
    void *                           state       // { /* no members */ }
);
```

---

## 🔷 Interpolation

```c++
void interpolation_linear(
    interpolation_linear_out_t       out,      // { float *signal; }
    interpolation_linear_in_t        in,      // { float *signal_a; float *signal_b; float *t; }
    void *                           params,      // { /* no members */ }
    void *                           state       // { /* no members */ }
);

void interpolation_cubic(
    interpolation_cubic_out_t        out,      // { float *signal; }
    interpolation_cubic_in_t         in,      // { float *signal_n1; float *signal_a; float *signal_b; float *signal_c; float *t; }
    void *                           params,      // { /* no members */ }
    void *                           state       // { /* no members */ }
);

void interpolation_sinc(
    interpolation_sinc_out_t         out,      // { float *signal; }
    interpolation_sinc_in_t          in,      // { float *buffer; float *position; }
    interpolation_sinc_params_t      params,      // { int num_taps; }
    void *                           state       // { /* no members */ }
);

void interpolation_lagrange(
    interpolation_lagrange_out_t     out,      // { float *signal; }
    interpolation_lagrange_in_t      in,      // { float *samples; float *t; }
    interpolation_lagrange_params_t  params,      // { int order; }
    void *                           state       // { /* no members */ }
);
```

---

## 🔷 Sample Rate Conversion

```c++
void src_upsample(
    src_upsample_out_t               out,      // { float *signal; }
    src_upsample_in_t                in,      // { float *signal; }
    src_upsample_params_t            params,      // { int factor; }
    void *                           state       // { /* no members */ }
);

void src_downsample(
    src_downsample_out_t             out,      // { float *signal; }
    src_downsample_in_t              in,      // { float *signal; }
    src_downsample_params_t          params,      // { int factor; }
    void *                           state       // { /* no members */ }
);

void src_antialias(
    src_antialias_out_t              out,      // { float *signal; }
    src_antialias_in_t               in,      // { float *signal; }
    src_antialias_params_t           params,      // { float cutoff; float sample_rate; }
    src_antialias_state_t *          state       // { float z1; float z2; }
);

void src_antiimage(
    src_antiimage_out_t              out,      // { float *signal; }
    src_antiimage_in_t               in,      // { float *signal; }
    src_antiimage_params_t           params,      // { float cutoff; float sample_rate; }
    src_antiimage_state_t *          state       // { float z1; float z2; }
);

void src_convert_format(
    src_convert_format_out_t         out,      // { float *signal; }
    src_convert_format_in_t          in,      // { float *signal; }
    src_convert_format_params_t      params,      // { int from_format; int to_format; }
    void *                           state       // { /* no members */ }
);
```

---

## 🔷 Frequency Domain

```c++
void freq_fft(
    freq_fft_out_t                   out,      // { float *real; float *imag; }
    freq_fft_in_t                    in,      // { float *signal; }
    freq_fft_params_t                params,      // { int block_size; }
    void *                           state       // { /* no members */ }
);

void freq_ifft(
    freq_ifft_out_t                  out,      // { float *signal; }
    freq_ifft_in_t                   in,      // { float *real; float *imag; }
    freq_ifft_params_t               params,      // { int block_size; }
    void *                           state       // { /* no members */ }
);

void freq_window(
    freq_window_out_t                out,      // { float *signal; }
    freq_window_in_t                 in,      // { float *signal; }
    freq_window_params_t             params,      // { int window_type; int block_size; }
    void *                           state       // { /* no members */ }
);

void freq_multiply(
    freq_multiply_out_t              out,      // { float *real; float *imag; }
    freq_multiply_in_t               in,      // { float *real_a; float *imag_a; float *real_b; float *imag_b; }
    freq_multiply_params_t           params,      // { int block_size; }
    void *                           state       // { /* no members */ }
);

void freq_overlap_add(
    freq_overlap_add_out_t           out,      // { float *signal; }
    freq_overlap_add_in_t            in,      // { float *frame; }
    freq_overlap_add_params_t        params,      // { int block_size; int hop_size; }
    freq_overlap_add_state_t *       state       // { float *buffer; }
);

void freq_overlap_save(
    freq_overlap_save_out_t          out,      // { float *frame; }
    freq_overlap_save_in_t           in,      // { float *signal; }
    freq_overlap_save_params_t       params,      // { int block_size; int hop_size; }
    freq_overlap_save_state_t *      state       // { float *buffer; int write_pos; }
);
```

---

## 🔷 Mixing & Routing

```c++
void mix_crossfade(
    mix_crossfade_out_t              out,      // { float *signal; }
    mix_crossfade_in_t               in,      // { float *signal_a; float *signal_b; }
    mix_crossfade_params_t           params,      // { float t; }
    void *                           state       // { /* no members */ }
);

void mix_wet_dry(
    mix_wet_dry_out_t                out,      // { float *signal; }
    mix_wet_dry_in_t                 in,      // { float *dry; float *wet; }
    mix_wet_dry_params_t             params,      // { float mix; }
    void *                           state       // { /* no members */ }
);

void mix_matrix(
    mix_matrix_out_t                 out,      // { float **signals; }
    mix_matrix_in_t                  in,      // { float **signals; }
    mix_matrix_params_t              params,      // { float **coefficients; int num_in; int num_out; }
    void *                           state       // { /* no members */ }
);

void mix_pan_stereo(
    mix_pan_stereo_out_t             out,      // { float *left; float *right; }
    mix_pan_stereo_in_t              in,      // { float *signal; }
    mix_pan_stereo_params_t          params,      // { float position; }
    void *                           state       // { /* no members */ }
);

void mix_encode_ms(
    mix_encode_ms_out_t              out,      // { float *mid; float *side; }
    mix_encode_ms_in_t               in,      // { float *left; float *right; }
    void *                           params,      // { /* no members */ }
    void *                           state       // { /* no members */ }
);

void mix_decode_ms(
    mix_decode_ms_out_t              out,      // { float *left; float *right; }
    mix_decode_ms_in_t               in,      // { float *mid; float *side; }
    void *                           params,      // { /* no members */ }
    void *                           state       // { /* no members */ }
);
```

---

## 🔷 Nonlinear / Distortion

```c++
void nonlinear_waveshape(
    nonlinear_waveshape_out_t        out,      // { float *signal; }
    nonlinear_waveshape_in_t         in,      // { float *signal; }
    nonlinear_waveshape_params_t     params,      // { float *transfer_table; int table_size; }
    void *                           state       // { /* no members */ }
);

void nonlinear_bitcrush(
    nonlinear_bitcrush_out_t         out,      // { float *signal; }
    nonlinear_bitcrush_in_t          in,      // { float *signal; }
    nonlinear_bitcrush_params_t      params,      // { float bit_depth; }
    void *                           state       // { /* no members */ }
);

void nonlinear_samplerate_reduce(
    nonlinear_samplerate_reduce_out_t out,      // { float *signal; }
    nonlinear_samplerate_reduce_in_t in,      // { float *signal; }
    nonlinear_samplerate_reduce_params_t params,      // { float factor; }
    nonlinear_samplerate_reduce_state_t * state       // { float last_val; float counter; }
);
```

---

## Summary — Which Functions Have State

| Function                      | State Fields                  |
|-------------------------------|-------------------------------|
| `generation_oscillator`       | `phase`                       |
| `generation_noise`            | `seed`, `prev_value`          |
| `generation_envelope`         | `current_level`, `stage`      |
| `generation_lfo`              | `phase`                       |
| `generation_impulse`          | `counter`                     |
| `amplitude_accumulate`        | `accumulator`                 |
| `amplitude_smooth`            | `prev_value`                  |
| `amplitude_normalize`         | `running_peak`                |
| `delay_unit`                  | `prev_sample`                 |
| `delay_line`                  | `buffer[]`, `write_pos`       |
| `delay_fractional`            | `buffer[]`, `write_pos`       |
| `filter_fir`                  | `buffer[]`, `write_pos`       |
| `filter_biquad`               | `z1`, `z2`                    |
| `filter_comb_ff/fb`           | `buffer[]`, `write_pos`       |
| `filter_allpass`              | `buffer[]`, `write_pos`       |
| `filter_dc_block`             | `prev_input`, `prev_output`   |
| `filter_integrate`            | `accumulator`                 |
| `filter_differentiate`        | `prev_sample`                 |
| `detect_peak`                 | `prev_peak`                   |
| `detect_rms`                  | `buffer[]`, `write_pos`, `sum`|
| `detect_envelope`             | `prev_envelope`               |
| `detect_zero_crossing`        | `prev_sample`                 |
| `detect_slope`                | `prev_sample`                 |
| `detect_autocorrelate`        | `buffer[]`, `write_pos`       |
| `modulation_phase`            | `buffer[]`, `write_pos`       |
| `modulation_frequency`        | `buffer[]`, `write_pos`, `current_delay` |
| `src_antialias`               | `z1`, `z2`                    |
| `src_antiimage`               | `z1`, `z2`                    |
| `freq_overlap_add`            | `buffer[]`                    |
| `freq_overlap_save`           | `buffer[]`, `write_pos`       |
| `nonlinear_samplerate_reduce` | `last_val`, `counter`         |

---

## Quick Reference Table

| Group            | Modules                                                                      |
|------------------|------------------------------------------------------------------------------|
| `generation_`    | oscillator, noise, envelope, lfo, impulse, dc                                |
| `amplitude_`     | multiply, add, subtract, accumulate, smooth, clip_hard, clip_soft, normalize |
| `delay_`         | unit, line, fractional, tap_feedback, tap_feedforward                        |
| `filter_`        | fir, biquad, comb_ff, comb_fb, allpass, dc_block, integrate, differentiate   |
| `detect_`        | peak, rms, envelope, threshold, zero_crossing, slope, autocorrelate          |
| `modulation_`    | ring, phase, frequency, amplitude, scrub                                     |
| `interpolation_` | linear, cubic, sinc, lagrange                                                |
| `src_`           | upsample, downsample, antialias, antiimage, convert_format                   |
| `freq_`          | fft, ifft, window, multiply, overlap_add, overlap_save                       |
| `mix_`           | crossfade, wet_dry, matrix, pan_stereo, encode_ms, decode_ms                 |
| `nonlinear_`     | waveshape, bitcrush, samplerate_reduce                                       |
