# DSP Atomic Functions - Functional Definitions

## 🔷 Signal Generation

```c
static void generation_oscillator(
    struct { float *signal;                                      } out,
    struct { /* no input signal */                               } in,
    struct { float frequency; int waveform; float phase_offset;
             float sample_rate;                                  } params,
    struct { float phase;                                        } *state
);

static void generation_noise(
    struct { float *signal;                                      } out,
    struct { /* no input signal */                               } in,
    struct { float amplitude; int color;                         } params,
    struct { uint32_t seed; float prev_value;                    } *state
);

static void generation_envelope(
    struct { float *signal;                                      } out,
    struct { float *gate;                                        } in,
    struct { float attack; float decay;
             float sustain; float release;
             float sample_rate;                                  } params,
    struct { float current_level; int stage;                     } *state
);

static void generation_lfo(
    struct { float *signal;                                      } out,
    struct { /* no input signal */                               } in,
    struct { float frequency; int waveform;
             float phase_offset; float sample_rate;              } params,
    struct { float phase;                                        } *state
);

static void generation_impulse(
    struct { float *signal;                                      } out,
    struct { /* no input signal */                               } in,
    struct { float interval; float sample_rate;                  } params,
    struct { int counter;                                        } *state
);

static void generation_dc(
    struct { float *signal;                                      } out,
    struct { /* no input signal */                               } in,
    struct { float value;                                        } params,
    struct { /* no state */                                      } *state
);
```

---

## 🔷 Gain & Amplitude

```c
static void amplitude_multiply(
    struct { float *signal;                                      } out,
    struct { float *signal_a; float *signal_b;                   } in,
    struct { /* no params */                                     } params,
    struct { /* no state */                                      } *state
);

static void amplitude_add(
    struct { float *signal;                                      } out,
    struct { float *signal_a; float *signal_b;                   } in,
    struct { /* no params */                                     } params,
    struct { /* no state */                                      } *state
);

static void amplitude_subtract(
    struct { float *signal;                                      } out,
    struct { float *signal_a; float *signal_b;                   } in,
    struct { /* no params */                                     } params,
    struct { /* no state */                                      } *state
);

static void amplitude_divide(
    struct { float *signal;                                      } out,
    struct { float *numerator; float *denominator;               } in,
    struct { float epsilon;   /* prevents div by zero */         } params,
    struct { /* no state */                                      } *state
);

static void amplitude_accumulate(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { /* no params */                                     } params,
    struct { float accumulator;                                  } *state
);

static void amplitude_smooth(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { float attack; float release; float sample_rate;     } params,
    struct { float prev_value;                                   } *state
);

static void amplitude_clip_hard(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { float threshold;                                    } params,
    struct { /* no state */                                      } *state
);

static void amplitude_clip_soft(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { float threshold; int curve;                         } params,
    struct { /* no state */                                      } *state
);

static void amplitude_normalize(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { float target_level; int mode;                       } params,
    struct { float running_peak;                                 } *state
);
```

---

## 🔷 Delay & Memory

```c
static void delay_unit(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { /* no params */                                     } params,
    struct { float prev_sample;                                  } *state
);

static void delay_line(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { int length; float sample_rate;                      } params,
    struct { float *buffer; int write_pos;                       } *state
);

static void delay_fractional(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { float delay_samples; int interpolation;             } params,
    struct { float *buffer; int write_pos;                       } *state
);

static void delay_tap_feedback(
    struct { float *signal;                                      } out,
    struct { float *buffer; int tap_position;                    } in,
    struct { float coefficient;                                  } params,
    struct { /* shared ref to delay_line state */                } *state
);

static void delay_tap_feedforward(
    struct { float *signal;                                      } out,
    struct { float *buffer; int tap_position;                    } in,
    struct { float coefficient;                                  } params,
    struct { /* no state */                                      } *state
);
```

---

## 🔷 Filtering

```c
static void filter_fir(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { float *kernel; int kernel_size;                     } params,
    struct { float *buffer; int write_pos;                       } *state
);

static void filter_biquad(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { float b0; float b1; float b2;
             float a1; float a2;                                 } params,
    struct { float z1; float z2;                                 } *state
);

static void filter_comb_ff(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { int delay_samples; float coefficient;               } params,
    struct { float *buffer; int write_pos;                       } *state
);

static void filter_comb_fb(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { int delay_samples; float coefficient;               } params,
    struct { float *buffer; int write_pos;                       } *state
);

static void filter_allpass(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { int delay_samples; float coefficient;               } params,
    struct { float *buffer; int write_pos;                       } *state
);

static void filter_dc_block(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { float coefficient;                                  } params,
    struct { float prev_input; float prev_output;                } *state
);

static void filter_integrate(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { /* no params */                                     } params,
    struct { float accumulator;                                  } *state
);

static void filter_differentiate(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { /* no params */                                     } params,
    struct { float prev_sample;                                  } *state
);
```

---

## 🔷 Detection & Analysis

```c
static void detect_peak(
    struct { float *level;                                       } out,
    struct { float *signal;                                      } in,
    struct { float attack; float release; float sample_rate;     } params,
    struct { float prev_peak;                                    } *state
);

static void detect_rms(
    struct { float *level;                                       } out,
    struct { float *signal;                                      } in,
    struct { int window_size;                                    } params,
    struct { float *buffer; int write_pos; float sum;            } *state
);

static void detect_envelope(
    struct { float *envelope;                                    } out,
    struct { float *signal;                                      } in,
    struct { float attack; float release; float sample_rate;     } params,
    struct { float prev_envelope;                                } *state
);

static void detect_threshold(
    struct { float *gate;                                        } out,
    struct { float *signal;                                      } in,
    struct { float threshold;                                    } params,
    struct { /* no state */                                      } *state
);

static void detect_zero_crossing(
    struct { float *trigger;                                     } out,
    struct { float *signal;                                      } in,
    struct { /* no params */                                     } params,
    struct { float prev_sample;                                  } *state
);

static void detect_slope(
    struct { float *slope;                                       } out,
    struct { float *signal;                                      } in,
    struct { /* no params */                                     } params,
    struct { float prev_sample;                                  } *state
);

static void detect_autocorrelate(
    struct { float *correlation;                                 } out,
    struct { float *signal;                                      } in,
    struct { int max_lag;                                        } params,
    struct { float *buffer; int write_pos;                       } *state
);
```

---

## 🔷 Modulation

```c
static void modulation_ring(
    struct { float *signal;                                      } out,
    struct { float *signal; float *modulator;                    } in,
    struct { /* no params */                                     } params,
    struct { /* no state */                                      } *state
);

static void modulation_amplitude(
    struct { float *signal;                                      } out,
    struct { float *signal; float *modulator;                    } in,
    struct { float depth;                                        } params,
    struct { /* no state */                                      } *state
);

static void modulation_phase(
    struct { float *signal;                                      } out,
    struct { float *signal; float *modulator;                    } in,
    struct { float depth; float sample_rate;                     } params,
    struct { float phase;                                        } *state
);

static void modulation_frequency(
    struct { float *signal;                                      } out,
    struct { float *signal; float *modulator;                    } in,
    struct { float depth; float sample_rate;                     } params,
    struct { float phase;                                        } *state
);

static void modulation_scrub(
    struct { float *signal;                                      } out,
    struct { float *buffer; float *position_signal;              } in,
    struct { int interpolation;                                  } params,
    struct { /* no state */                                      } *state
);
```

---

## 🔷 Interpolation

```c
static void interpolation_linear(
    struct { float *sample;                                      } out,
    struct { float *sample_a; float *sample_b; float t;          } in,
    struct { /* no params */                                     } params,
    struct { /* no state */                                      } *state
);

static void interpolation_cubic(
    struct { float *sample;                                      } out,
    struct { float *sample_n1; float *sample_a;
             float *sample_b;  float *sample_c; float t;         } in,
    struct { /* no params */                                     } params,
    struct { /* no state */                                      } *state
);

static void interpolation_sinc(
    struct { float *sample;                                      } out,
    struct { float *buffer; float position;                      } in,
    struct { int num_taps;                                       } params,
    struct { float *window_coeffs;                               } *state
);

static void interpolation_lagrange(
    struct { float *sample;                                      } out,
    struct { float *samples; float t;                            } in,
    struct { int order;                                          } params,
    struct { /* no state */                                      } *state
);
```

---

## 🔷 Sample Rate Conversion

```c
static void src_upsample(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { int factor;                                         } params,
    struct { float z1; float z2;                                 } *state
);

static void src_downsample(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { int factor;                                         } params,
    struct { float z1; float z2; int phase;                      } *state
);

static void src_antialias(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { float cutoff; float sample_rate;                    } params,
    struct { float z1; float z2;                                 } *state
);

static void src_antiimage(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { float cutoff; float sample_rate;                    } params,
    struct { float z1; float z2;                                 } *state
);

static void src_convert_format(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { int from_format; int to_format;                     } params,
    struct { /* no state */                                      } *state
);
```

---

## 🔷 Frequency Domain

```c
static void freq_fft(
    struct { float *real; float *imag;                           } out,
    struct { float *signal;                                      } in,
    struct { int block_size; int window_type;                    } params,
    struct { float *window_coeffs;                               } *state
);

static void freq_ifft(
    struct { float *signal;                                      } out,
    struct { float *real; float *imag;                           } in,
    struct { int block_size;                                     } params,
    struct { /* no state */                                      } *state
);

static void freq_window(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { int window_type; int block_size;                    } params,
    struct { float *coeffs;                                      } *state
);

static void freq_multiply(
    struct { float *real; float *imag;                           } out,
    struct { float *real_a; float *imag_a;
             float *real_b; float *imag_b;                       } in,
    struct { /* no params */                                     } params,
    struct { /* no state */                                      } *state
);

static void freq_overlap_add(
    struct { float *signal;                                      } out,
    struct { float *frames;                                      } in,
    struct { int block_size; int hop_size;                       } params,
    struct { float *overlap_buffer; int write_pos;               } *state
);

static void freq_overlap_save(
    struct { float *frames;                                      } out,
    struct { float *signal;                                      } in,
    struct { int block_size; int hop_size;                       } params,
    struct { float *input_buffer; int write_pos;                 } *state
);
```

---

## 🔷 Mixing & Routing

```c
static void mix_crossfade(
    struct { float *signal;                                      } out,
    struct { float *signal_a; float *signal_b; float *t;         } in,
    struct { /* no params */                                     } params,
    struct { /* no state */                                      } *state
);

static void mix_wet_dry(
    struct { float *signal;                                      } out,
    struct { float *dry; float *wet;                             } in,
    struct { float mix;                                          } params,
    struct { /* no state */                                      } *state
);

static void mix_matrix(
    struct { float **signals; int out_count;                     } out,
    struct { float **signals; int in_count;                      } in,
    struct { float **coefficients;                               } params,
    struct { /* no state */                                      } *state
);

static void mix_pan_stereo(
    struct { float *left; float *right;                          } out,
    struct { float *signal;                                      } in,
    struct { float position;   /* -1.0 left, +1.0 right */       } params,
    struct { /* no state */                                      } *state
);

static void mix_encode_ms(
    struct { float *mid; float *side;                            } out,
    struct { float *left; float *right;                          } in,
    struct { /* no params */                                     } params,
    struct { /* no state */                                      } *state
);

static void mix_decode_ms(
    struct { float *left; float *right;                          } out,
    struct { float *mid; float *side;                            } in,
    struct { /* no params */                                     } params,
    struct { /* no state */                                      } *state
);
```

---

## 🔷 Nonlinear / Distortion

```c
static void nonlinear_waveshape(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { float *transfer_table; int table_size;              } params,
    struct { /* no state */                                      } *state
);

static void nonlinear_bitcrush(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { int bit_depth;                                      } params,
    struct { /* no state */                                      } *state
);

static void nonlinear_samplerate_reduce(
    struct { float *signal;                                      } out,
    struct { float *signal;                                      } in,
    struct { float reduction_factor;                             } params,
    struct { float held_sample; int counter;                     } *state
);
```

---

## Summary — Which Functions Have State

| Function                      | State Fields                  |
|-------------------------------|-------------------------------|
| `generation_oscillator`       | `phase`                       |
| `generation_noise`            | `seed, prev_value`            |
| `generation_envelope`         | `current_level, stage`        |
| `generation_lfo`              | `phase`                       |
| `generation_impulse`          | `counter`                     |
| `amplitude_accumulate`        | `accumulator`                 |
| `amplitude_smooth`            | `prev_value`                  |
| `amplitude_normalize`         | `running_peak`                |
| `delay_unit`                  | `prev_sample`                 |
| `delay_line`                  | `buffer[], write_pos`         |
| `delay_fractional`            | `buffer[], write_pos`         |
| `filter_fir`                  | `buffer[], write_pos`         |
| `filter_biquad`               | `z1, z2`                      |
| `filter_comb_ff/fb`           | `buffer[], write_pos`         |
| `filter_allpass`              | `buffer[], write_pos`         |
| `filter_dc_block`             | `prev_input, prev_output`     |
| `filter_integrate`            | `accumulator`                 |
| `filter_differentiate`        | `prev_sample`                 |
| `detect_peak`                 | `prev_peak`                   |
| `detect_rms`                  | `buffer[], write_pos, sum`    |
| `detect_envelope`             | `prev_envelope`               |
| `detect_zero_crossing`        | `prev_sample`                 |
| `detect_slope`                | `prev_sample`                 |
| `detect_autocorrelate`        | `buffer[], write_pos`         |
| `modulation_phase`            | `phase`                       |
| `modulation_frequency`        | `phase`                       |
| `interpolation_sinc`          | `window_coeffs[]`             |
| `src_upsample`                | `z1, z2`                      |
| `src_downsample`              | `z1, z2, phase`               |
| `src_antialias`               | `z1, z2`                      |
| `src_antiimage`               | `z1, z2`                      |
| `freq_fft`                    | `window_coeffs[]`             |
| `freq_window`                 | `coeffs[]`                    |
| `freq_overlap_add`            | `overlap_buffer[], write_pos` |
| `freq_overlap_save`           | `input_buffer[], write_pos`   |
| `nonlinear_samplerate_reduce` | `held_sample, counter`        |

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

id: "a1b2c3d4-0000-0000-0000-000000000001"
name: sustainer
version: 1.0.0

params:
  gain: { min: 0.0,   max: 80.0,  default: 40.0,  unit: dB }
  threshold: { min: -80.0, max: 0.0,   default: -40.0, unit: dBFS }
  attack: { min: 0.001, max: 0.500, default: 0.010, unit: seconds }
  release: { min: 0.010, max: 2.000, default: 0.200, unit: seconds }

internal:
  target_level: -12.0
  threshold_close: ${params.threshold} - 20.0
  gain_min: 0.0
  smooth_attack: 0.020
  smooth_release: 0.100
  filter_freq: 8000
  filter_q: 0.707
  filter_type: lowpass
  output_gain: 0.0
  mix: 1.0

pipeline:
  - id: detect
    fn: detect_envelope
    args:
      signal: ${input}
      attack: ${params.attack}
      release: ${params.release}
      sample_rate: ${context.sample_rate}
    out: envelope

  - id: compare
    fn: detect_threshold
    args:
      signal: ${envelope}
      threshold: ${params.threshold}
    out: gate

  - id: calc_gain
    fn: amplitude_divide
    args:
      numerator: ${internal.target_level}
      denominator: ${envelope}
    out: raw_gain

  - id: clamp_gain
    fn: amplitude_clip_hard
    args:
      signal: ${raw_gain}
      threshold: ${params.gain}
    out: clamped_gain

  - id: smooth
    fn: amplitude_smooth
    args:
      signal: ${clamped_gain}
      attack: ${internal.smooth_attack}
      release: ${internal.smooth_release}
      sample_rate: ${context.sample_rate}
    out: smooth_gain

  - id: gate_gain
    fn: amplitude_multiply
    args:
      signal_a: ${smooth_gain}
      signal_b: ${gate}
    out: gated_gain

  - id: apply_gain
    fn: amplitude_multiply
    args:
      signal_a: ${input}
      signal_b: ${gated_gain}
    out: sustained

  - id: tone
    fn: filter_biquad
    args:
      signal: ${sustained}
      frequency: ${internal.filter_freq}
      q: ${internal.filter_q}
      type: ${internal.filter_type}
    out: filtered

  - id: trim
    fn: amplitude_multiply
    args:
      signal: ${filtered}
      scalar: ${internal.output_gain}
    out: trimmed

  - id: blend
    fn: mix_wet_dry
    args:
      dry: ${input}
      wet: ${trimmed}
      mix: ${internal.mix}
    out: ${output}
```

---

## 📜 User Settings

```yaml
id: "f7e6d5c4-0000-0000-0000-000000000099"
type: "a1b2c3d4-0000-0000-0000-000000000001"   # ref to sustainer.unit

params:
  gain: 40.0     # user set
  threshold: -40.0    # user set
  attack: default  # use unit default → 0.010
  release: default  # use unit default → 0.200
```