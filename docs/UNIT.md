# DSP Unit

---

```yaml
# sustainer.unit.yaml

id:      "a1b2c3d4-0000-0000-0000-000000000001"
name:    sustainer
version: 1.0.0

# ─────────────────────────────────────────────
# USER-FACING PARAMETERS
# ─────────────────────────────────────────────
params:
  gain:      { min: 0.0,   max: 80.0,  default: 40.0,  unit: dB      }
  threshold: { min: -80.0, max: 0.0,   default: -40.0, unit: dBFS    }
  attack:    { min: 0.001, max: 0.500, default: 0.010, unit: seconds }
  release:   { min: 0.010, max: 2.000, default: 0.200, unit: seconds }

# ─────────────────────────────────────────────
# DESIGNER-CONTROLLED FIXED VALUES
# ─────────────────────────────────────────────
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

# ─────────────────────────────────────────────
# SIGNAL DECLARATIONS
# named signals that flow between pipeline steps
# ─────────────────────────────────────────────
signals:
  - name: input          # reserved, audio in
  - name: output         # reserved, audio out
  - name: envelope
  - name: gate
  - name: raw_gain
  - name: clamped_gain
  - name: smooth_gain
  - name: gated_gain
  - name: sustained
  - name: filtered
  - name: trimmed

# ─────────────────────────────────────────────
# PIPELINE
# each step maps to a 4-part atom function
# ─────────────────────────────────────────────
pipeline:

  - id:   detect
    fn:   detect_envelope
    out:
      envelope:     envelope
    in:
      signal:       ${signals.input}
    config:
      attack:       ${params.attack}
      release:      ${params.release}
      sample_rate:  ${context.sample_rate}
    state:
      prev_envelope: 0.0

  - id:   gate
    fn:   detect_threshold
    out:
      gate:         gate
    in:
      signal:       ${signals.envelope}
    config:
      threshold:    ${params.threshold}
    state: ~          # no state

  - id:   calc_gain
    fn:   amplitude_divide
    out:
      signal:       raw_gain
    in:
      numerator:    ${internal.target_level}
      denominator:  ${signals.envelope}
    config:
      epsilon:      0.000001
    state: ~

  - id:   clamp_gain
    fn:   amplitude_clip_hard
    out:
      signal:       clamped_gain
    in:
      signal:       ${signals.raw_gain}
    config:
      threshold:    ${params.gain}
    state: ~

  - id:   smooth
    fn:   amplitude_smooth
    out:
      signal:       smooth_gain
    in:
      signal:       ${signals.clamped_gain}
    config:
      attack:       ${internal.smooth_attack}
      release:      ${internal.smooth_release}
      sample_rate:  ${context.sample_rate}
    state:
      prev_value:   0.0

  - id:   gate_gain
    fn:   amplitude_multiply
    out:
      signal:       gated_gain
    in:
      signal_a:     ${signals.smooth_gain}
      signal_b:     ${signals.gate}
    config: ~
    state: ~

  - id:   apply_gain
    fn:   amplitude_multiply
    out:
      signal:       sustained
    in:
      signal_a:     ${signals.input}
      signal_b:     ${signals.gated_gain}
    config: ~
    state: ~

  - id:   tone
    fn:   filter_biquad
    out:
      signal:       filtered
    in:
      signal:       ${signals.sustained}
    config:
      b0:           ${context.biquad.lowpass.b0(internal.filter_freq, internal.filter_q, context.sample_rate)}
      b1:           ${context.biquad.lowpass.b1(internal.filter_freq, internal.filter_q, context.sample_rate)}
      b2:           ${context.biquad.lowpass.b2(internal.filter_freq, internal.filter_q, context.sample_rate)}
      a1:           ${context.biquad.lowpass.a1(internal.filter_freq, internal.filter_q, context.sample_rate)}
      a2:           ${context.biquad.lowpass.a2(internal.filter_freq, internal.filter_q, context.sample_rate)}
    state:
      z1:           0.0
      z2:           0.0

  - id:   trim
    fn:   amplitude_multiply
    out:
      signal:       trimmed
    in:
      signal_a:     ${signals.filtered}
      signal_b:     ${internal.output_gain}
    config: ~
    state: ~

  - id:   blend
    fn:   mix_wet_dry
    out:
      signal:       ${signals.output}
    in:
      dry:          ${signals.input}
      wet:          ${signals.trimmed}
    config:
      mix:          ${internal.mix}
    state: ~
```

---

## What Changed

| Section | What's New |
|---------|-----------|
| `signals` | explicit declaration of all named signals flowing through pipeline |
| `out` | maps function output to a named signal |
| `in` | maps named signals or values into function inputs |
| `config` | read-only parameters, set at instantiation |
| `state` | runtime memory, initialized here, mutated each sample |
| `~` | YAML null, means this part is empty for this function |

---

## Reference Scopes — Updated

| Scope | Meaning |
|-------|---------|
| `${signals.<name>}` | named signal declared in `signals` block |
| `${signals.input}` | reserved audio input |
| `${signals.output}` | reserved audio output |
| `${params.<name>}` | user-facing parameter value |
| `${internal.<name>}` | designer fixed value |
| `${context.sample_rate}` | runtime context from host |
| `${context.biquad.<type>.<coeff>(...)}`| runtime biquad coefficient calculation |

---

## State Lifecycle

```
instantiate unit
  → foreach step in pipeline
      if step.state is not null
        → allocate state struct
        → initialize with values defined in yaml
        → bind to step instance

per sample / per buffer
  → foreach step in pipeline
      → call fn(out, in, config, &state)
      → state is mutated in place

destroy unit
  → foreach step in pipeline
      → free state memory
```

---

Would you like to now define how **multiple unit instances are chained** into a patch file, or define the **context object** that carries `sample_rate`, `buffer_size`, and biquad coefficient helpers?