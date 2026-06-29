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
- `compatibility`: Non-empty map of boolean target flags.

Extra metadata such as `meta` and `ui` is currently tolerated but not interpreted by the C loader.

## Params

Params are keyed by name. Names must be unique.

- `type`: `float`, `int`, or `bool`.
- `default`: Required initial value.
- `min` / `max`: Required for `float` and `int`; omitted for `bool`.
- `smoothing_ms`: Optional hint, currently parsed only as metadata.

```yaml
params:
  gain:
    type: float
    default: 1.0
    min: 0.0
    max: 4.0
```

## Ports

Ports are grouped under `ports.inputs` and `ports.outputs`. Names must be unique within each group.

- `audio` ports require `channels`.
- Mono audio ports may use a graph signal with the same name.
- Multi-channel audio ports require `signals`, with one graph signal name per channel in interleaved order.
- `control` ports do not require `channels` or graph signals.
- `control` ports currently route only to params. If `target_param` is set, it names the destination param; otherwise the runtime uses the same name as the port.
- Future control modes must be explicit. The next supported shape is `target: { kind: param, name: <param> }`; graph-signal, multi-param, typed-buffer, and smoothing-lane routing remain unsupported until Phase O implements and validates them.

## Graph

`graph.signals` is the complete signal namespace and rejects duplicates. `graph.nodes` is source ordered; the compiler reorders it when dependencies allow.

Each node requires unique `id` and registered `atom`. Binding sections are maps:

- `in`: Atom input signals.
- `out`: Atom output signals.
- `config`: Literal values or `${params.name}` references.

Duplicate binding keys are rejected during loading. Unknown signal references, unknown param references, missing required atom bindings, and unsupported binding keys are rejected during compile.

## Implemented Atom Binding Contracts

The compiler currently validates required keys for these MVP atoms:

- `generation_dc`: `out.signal`, `config.value`
- `amplitude_multiply`, `amplitude_add`, `amplitude_subtract`: `in.signal_a`, `in.signal_b`, `out.signal`
- `amplitude_clip_hard`: `in.signal`, `out.signal`, `config.threshold`
- `amplitude_clip_soft`: `in.signal`, `out.signal`, `config.threshold`, `config.curve`
- `delay_unit`: `in.signal`, `out.signal`
- `delay_line`: `in.signal`, `out.signal`, `config.length`
- `delay_fractional`: `in.signal`, `out.signal`, `config.delay_samples`, `config.interpolation`
- `delay_tap_feedback`, `delay_tap_feedforward`: `in.buffer`, scalar `in.tap_position`, `out.signal`, `config.coefficient`
- `filter_biquad`: `in.signal`, `out.signal`, `config.b0`, `config.b1`, `config.b2`, `config.a1`, `config.a2`
- `filter_allpass`, `filter_comb_ff`: `in.signal`, `out.signal`, `config.delay_samples`, `config.coefficient`
- `filter_comb_fb`: `in.signal`, optional `in.delay`, `out.signal`, `config.delay_samples`, `config.coefficient`
- `filter_dc_block`: `in.signal`, `out.signal`, `config.coefficient`
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

## Runtime MVP Limits

The v2 runtime supports mono processing through `apg_v2_runtime_process_mono(...)` and named mono ports through `apg_v2_runtime_process_mono_ports(...)`. Multi-channel audio ports use explicit per-channel signal mappings and can be processed with `apg_v2_runtime_process_interleaved_ports(...)`. The runtime owns signal buffers, param defaults, per-node atom call storage, and basic `FIELD_BUFFER` state allocation, then executes the compiled schedule. Control routing is currently param-only; non-param destinations are a Phase O design and validation task.
