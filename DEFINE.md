# DSP Atomic Blocks - Functional Definitions

---

## 🔷 Signal Generation

```
generation_oscillator(frequency, waveform, phase, sample_rate) → signal
generation_noise(color, seed) → signal
generation_envelope(attack, decay, sustain, release, gate, sample_rate) → signal
generation_lfo(frequency, waveform, phase, sample_rate) → signal
generation_impulse(interval, sample_rate) → signal
generation_dc(value) → signal
```

---

## 🔷 Gain & Amplitude

```
amplitude_multiply(signal, scalar) → signal
amplitude_add(signal_a, signal_b) → signal
amplitude_subtract(signal_a, signal_b) → signal
amplitude_accumulate(signal) → signal
amplitude_smooth(signal, time, sample_rate) → signal
amplitude_clip_hard(signal, threshold) → signal
amplitude_clip_soft(signal, threshold, curve) → signal
amplitude_normalize(signal, target_level, mode) → signal
```

---

## 🔷 Delay & Memory

```
delay_unit(signal) → signal
delay_line(signal, length, sample_rate) → signal
delay_fractional(signal, delay_samples, interpolation) → signal
delay_tap_feedback(delay_line, tap_position, coefficient) → signal
delay_tap_feedforward(delay_line, tap_position, coefficient) → signal
```

---

## 🔷 Filtering

```
filter_fir(signal, kernel) → signal
filter_biquad(signal, b0, b1, b2, a1, a2) → signal
filter_comb_ff(signal, delay_samples, coefficient) → signal
filter_comb_fb(signal, delay_samples, coefficient) → signal
filter_allpass(signal, delay_samples, coefficient) → signal
filter_dc_block(signal, coefficient) → signal
filter_integrate(signal) → signal
filter_differentiate(signal) → signal
```

---

## 🔷 Detection & Analysis

```
detect_peak(signal, attack, release, sample_rate) → signal
detect_rms(signal, window_size) → signal
detect_envelope(signal, attack, release, sample_rate) → signal
detect_threshold(signal, threshold) → signal
detect_zero_crossing(signal) → signal
detect_slope(signal) → signal
detect_autocorrelate(signal, max_lag) → signal
```

---

## 🔷 Modulation

```
modulation_ring(signal, modulator) → signal
modulation_phase(signal, modulator, depth) → signal
modulation_frequency(signal, modulator, depth) → signal
modulation_amplitude(signal, modulator, depth) → signal
modulation_scrub(buffer, position_signal) → signal
```

---

## 🔷 Interpolation

```
interpolation_linear(sample_a, sample_b, t) → sample
interpolation_cubic(sample_n1, sample_a, sample_b, sample_c, t) → sample
interpolation_sinc(buffer, position, num_taps) → sample
interpolation_lagrange(samples[], order, t) → sample
```

---

## 🔷 Sample Rate Conversion

```
src_upsample(signal, factor) → signal
src_downsample(signal, factor) → signal
src_antialias(signal, cutoff, sample_rate) → signal
src_antiimage(signal, cutoff, sample_rate) → signal
src_convert_format(signal, from_format, to_format) → signal
```

---

## 🔷 Frequency Domain

```
freq_fft(signal, block_size, window) → spectrum
freq_ifft(spectrum, block_size) → signal
freq_window(signal, window_type, block_size) → signal
freq_multiply(spectrum_a, spectrum_b) → spectrum
freq_overlap_add(frames[], block_size, hop_size) → signal
freq_overlap_save(signal, block_size, hop_size) → frames[]
```

---

## 🔷 Mixing & Routing

```
mix_crossfade(signal_a, signal_b, t) → signal
mix_wet_dry(dry, wet, mix) → signal
mix_matrix(signals[], coefficients[][]) → signals[]
mix_pan_stereo(signal, position) → signal[2]
mix_encode_ms(signal_l, signal_r) → signal[2]
mix_decode_ms(signal_m, signal_s) → signal[2]
```

---

## 🔷 Nonlinear / Distortion

```
nonlinear_waveshape(signal, transfer_table) → signal
nonlinear_bitcrush(signal, bit_depth) → signal
nonlinear_samplerate_reduce(signal, reduction_factor) → signal
```

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

---

# DSP Unit - Configurable Definition

---

## 📦 DSP Unit
```yaml
# sustainer.unit.yaml

id:      "a1b2c3d4-0000-0000-0000-000000000001"
name:    sustainer
version: 1.0.0

params:
  gain:      { min: 0.0,   max: 80.0,  default: 40.0,  unit: dB      }
  threshold: { min: -80.0, max: 0.0,   default: -40.0, unit: dBFS    }
  attack:    { min: 0.001, max: 0.500, default: 0.010, unit: seconds }
  release:   { min: 0.010, max: 2.000, default: 0.200, unit: seconds }

internal:
  target_level:    -12.0
  threshold_close: ${params.threshold} - 20.0
  gain_min:        0.0
  smooth_attack:   0.020
  smooth_release:  0.100
  filter_freq:     8000
  filter_q:        0.707
  filter_type:     lowpass
  output_gain:     0.0
  mix:             1.0

pipeline:
  - id:       detect
    fn:       detect_envelope
    args:
      signal:  ${input}
      attack:  ${params.attack}
      release: ${params.release}
      sample_rate: ${context.sample_rate}
    out:      envelope

  - id:       compare
    fn:       detect_threshold
    args:
      signal:    ${envelope}
      threshold: ${params.threshold}
    out:      gate

  - id:       calc_gain
    fn:       amplitude_divide
    args:
      numerator:   ${internal.target_level}
      denominator: ${envelope}
    out:      raw_gain

  - id:       clamp_gain
    fn:       amplitude_clip_hard
    args:
      signal:    ${raw_gain}
      threshold: ${params.gain}
    out:      clamped_gain

  - id:       smooth
    fn:       amplitude_smooth
    args:
      signal:  ${clamped_gain}
      attack:  ${internal.smooth_attack}
      release: ${internal.smooth_release}
      sample_rate: ${context.sample_rate}
    out:      smooth_gain

  - id:       gate_gain
    fn:       amplitude_multiply
    args:
      signal_a: ${smooth_gain}
      signal_b: ${gate}
    out:      gated_gain

  - id:       apply_gain
    fn:       amplitude_multiply
    args:
      signal_a: ${input}
      signal_b: ${gated_gain}
    out:      sustained

  - id:       tone
    fn:       filter_biquad
    args:
      signal:    ${sustained}
      frequency: ${internal.filter_freq}
      q:         ${internal.filter_q}
      type:      ${internal.filter_type}
    out:      filtered

  - id:       trim
    fn:       amplitude_multiply
    args:
      signal: ${filtered}
      scalar: ${internal.output_gain}
    out:      trimmed

  - id:       blend
    fn:       mix_wet_dry
    args:
      dry:  ${input}
      wet:  ${trimmed}
      mix:  ${internal.mix}
    out:      ${output}
```

---

## 📜 User Settings
```yaml
id:    "f7e6d5c4-0000-0000-0000-000000000099"
type:  "a1b2c3d4-0000-0000-0000-000000000001"   # ref to sustainer.unit

params:
  gain:      40.0     # user set
  threshold: -40.0    # user set
  attack:    default  # use unit default → 0.010
  release:   default  # use unit default → 0.200
```