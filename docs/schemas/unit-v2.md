# Unit v2 Schema

`unit.v2.yaml` describes a reusable DSP unit. It is a validated source format; the compiler lowers it into atom entries, numeric signal/param indexes, producer metadata, and a topological schedule before runtime execution.

## Required Top-Level Fields

- `kind`: Must be `apg.unit`.
- `schema`: Must be `apg.unit.v2`.
- `name`: Stable snake_case unit identifier, for example `simple_gain`.
- `version`: Unit definition version.
- `params`: Public controls.
- `ports`: External audio/control inputs and outputs.
- `graph`: Internal signal and atom graph.
- `compatibility`: Non-empty map of boolean target flags. These flags are preserved in unit and project inspect JSON for frontend compatibility checks.

Optional top-level metadata:

- `meta`: Map with optional scalar `title`, `category`, and `description` fields.
- `routing`: Declares an always-active project routing helper. See **Routing Helpers** below.
- `ui`: Optional map reserved for unit-level layout hints. It is validated as a map when present, but specific layout fields are not interpreted yet.

## Params

Params are keyed by name. Names must be unique.

- `type`: `float`, `int`, or `bool`.
- `default`: Required initial value.
- `min` / `max`: Required for `float` and `int`; omitted for `bool`.
- `smoothing_ms`: Optional hint for future runtime smoothing.
- `ui`: Optional map for UI rendering hints.

Supported param `ui` fields are scalar values:

- `label`: Display label.
- `control`: One of `knob`, `slider`, `fader`, `toggle`, `number`, or `select`.
- `unit`: Display unit such as `x`, `dB`, `Hz`, or `%`.
- `scale`: One of `linear`, `log`, or `exp`.
- `display_precision`: Non-negative integer decimal precision.

```yaml
params:
  gain:
    type: float
    default: 1.0
    min: 0.0
    max: 4.0
    smoothing_ms: 10
    ui:
      label: Gain
      control: knob
      unit: x
      scale: linear
      display_precision: 2
```

## Ports

Ports are grouped under `ports.inputs` and `ports.outputs`. Names must be unique within each group.

- `audio` ports require `channels`.
- Mono audio ports may use a graph signal with the same name.
- Multi-channel audio ports require `signals`, with one graph signal name per channel in interleaved order.
- `control` ports do not require `channels` or graph signals.
- `control` ports route only to params. Omit a target for same-name param routing, use legacy `target_param: <param>`, or prefer `target: { kind: param, name: <param> }`.
- Unsupported `target.kind` values such as graph-signal, multi-param, typed-buffer, and smoothing-lane routing are rejected until those modes are designed and implemented.

## Routing Helpers

Project-level splits and merges must use units with explicit routing metadata. A panner has one mono input and one mono
output per path; a mixer has one mono input per path and one mono output. Every path names its public port and its own
float dB level parameter:

```yaml
routing:
  role: panner
  paths:
    - port: path_1
      level_param: path_1_db
    - port: path_2
      level_param: path_2_db
```

Routing paths must be unique, reference mono audio ports on the routed side, and reference distinct float params. The
schema accepts two or more paths so future 3/4/N-way helpers do not require a metadata redesign. The current project
validator and Studio transaction intentionally support exactly two paths, supplied by `path_panner_2` and
`path_mixer_2`. Each helper exposes one knob per path with a public `-60..+6 dB` range and 10 ms smoothing.

## Graph

`graph.signals` is the complete signal namespace for explicit-binding graphs and rejects duplicates. When `graph.routes` is present and `graph.signals` is omitted, the validator synthesizes the required signal namespace from public audio ports and route edges. `graph.nodes` is source ordered in list form; keyed-map node form is also accepted for route-driven graphs.

Each node requires unique `id` and registered `atom`. Binding sections are maps:

- `in`: Atom input signals.
- `out`: Atom output signals.
- `config`: Literal values or `${params.name}` references.

Duplicate binding keys are rejected during loading. Unknown signal references, unknown param references, missing required atom bindings, and unsupported binding keys are rejected during compile.

Route-driven graph form keeps signal routing outside the node declarations:

```yaml
graph:
  nodes:
    input:
      atom: input_signal
    drive:
      atom: amplitude_clip_soft
      params:
        threshold: 0.5
        curve: 2.0
    output:
      atom: output_signal
  routes:
    - input.out -> drive.in
    - drive.out -> output.in
```

In route-driven form, keyed `nodes` map keys are node IDs. Node `params` is a shorthand for atom `config`; do not declare both `params` and `config` on the same node. The pseudo atoms `input_signal` and `output_signal` mark public audio boundaries and are removed during normalization; their node IDs map to same-named public mono audio port signals. Route endpoints use `node.field`; `in` and `out` are accepted aliases when the target atom has exactly one signal input or output field.

## Implemented Atom Binding Contracts

The compiler currently validates required keys for these MVP atoms:

- `generation_dc`: `out.signal`, `config.value`
- `generation_lfo`: `out.signal`, `config.frequency`, `config.waveform`, `config.phase_offset`
- `amplitude_multiply`, `amplitude_add`, `amplitude_subtract`: `in.signal_a`, `in.signal_b`, `out.signal`
- `amplitude_gain_db`: `in.signal`, `out.signal`, `config.gain_db`
- `amplitude_clip_hard`: `in.signal`, `out.signal`, `config.threshold`
- `amplitude_clip_soft`: `in.signal`, `out.signal`, `config.threshold`, `config.curve`
- `delay_unit`: `in.signal`, `out.signal`
- `delay_line`: `in.signal`, `out.signal`, `config.length`
- `delay_fractional`: `in.signal`, `out.signal`, `config.delay_samples`, `config.interpolation`
- `delay_tap_feedback`, `delay_tap_feedforward`: `in.buffer`, scalar `in.tap_position`, `out.signal`, `config.coefficient`
- `filter_biquad`: `in.signal`, optional `in.cutoff`, `out.signal`, `config.cutoff`, `config.q`, `config.mode`, `config.smoothing_ms`
- `filter_biquad_coefficients`: `in.signal`, `out.signal`, `config.b0`, `config.b1`, `config.b2`, `config.a1`, `config.a2`
- `filter_allpass`, `filter_comb_ff`: `in.signal`, `out.signal`, `config.delay_samples`, `config.coefficient`
- `filter_comb_fb`: `in.signal`, optional `in.delay`, `out.signal`, `config.delay_samples`, `config.coefficient`
- `filter_dc_block`: `in.signal`, `out.signal`, `config.coefficient`
- `detect_threshold`: `in.signal`, `out.gate`, `config.threshold`
- `modulation_amplitude`, `modulation_frequency`, `modulation_phase`: `in.signal`, `in.modulator`, `out.signal`, `config.depth`
- `modulation_ring`: `in.signal`, `in.modulator`, `out.signal`
- `modulation_scrub`: `in.buffer`, `in.position`, `out.signal`, `config.buffer_size`
- `mix_crossfade`: `in.signal_a`, `in.signal_b`, `out.signal`, `config.t`
- `mix_wet_dry`: `in.dry`, `in.wet`, `out.signal`, `config.mix`
- `mix_matrix`: `in.signals[]`, `out.signals[]`, `config.coefficients[][]`
- `mix_pan_stereo`: `in.signal`, `out.left`, `out.right`, `config.position`
- `mix_encode_ms`: `in.left`, `in.right`, `out.mid`, `out.side`
- `mix_decode_ms`: `in.mid`, `in.side`, `out.left`, `out.right`

Atoms without explicit metadata may still compile without key-level validation until their contracts are added.

`filter_biquad` is the pedal-style cutoff/Q form. It supports multiple biquad response modes through `mode`, computes stable coefficients from cutoff/Q, accepts a routed cutoff signal, and smooths coefficient changes using `smoothing_ms`. Use `filter_biquad_coefficients` only when a unit needs to provide raw normalized coefficients directly.

`filter_biquad.mode` values:

- `0`: lowpass
- `1`: highpass
- `2`: bandpass
- `3`: notch

```yaml
filter:
  atom: filter_biquad
  params:
    cutoff: 5800.0
    q: 0.707
    mode: 0
    smoothing_ms: 12.0
routes:
  - drive.out -> filter.in
  - lfo.out -> filter.cutoff
  - filter.out -> output.in
```

## Runtime MVP Limits

The v2 runtime supports internal-pool mono processing through `apg_v2_runtime_process(...)` and external buffers through pre-resolved `apg_v2_runtime_process_mono_port_indices(...)` / `apg_v2_runtime_process_interleaved_port_indices(...)`. External inputs use immutable `{data, length}` views and outputs use mutable `{data, capacity}` views; mono capacities count frames and interleaved capacities count samples across all channels. The runtime validates views before importing audio or executing the schedule, while the compiled internal plan uses resolved raw pointers. Registry and host setup require `apg_prepare_context_t {maximum_frames, sample_rate}` and never infer a default block size or sample rate. The runtime owns contiguous signal and parameter pools, param defaults, per-node atom call storage, and basic `FIELD_BUFFER` state allocation, then executes the compiled schedule. Runtime control updates use param/control indices only. Host/tooling layers may resolve names during setup, but embedded callers should use signal indices, port indices, and param/control indices in the real-time loop.
