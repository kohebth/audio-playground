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

- `audio` ports require `channels` and a graph signal with the same name.
- `control` ports do not require `channels` or graph signals in the current MVP.

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
- `mix_wet_dry`: `in.dry`, `in.wet`, `out.signal`, `config.mix`

Atoms without explicit metadata may still compile without key-level validation until their contracts are added.

## Runtime MVP Limits

The v2 runtime currently supports mono processing through `apg_v2_runtime_process_mono(...)`. It owns signal buffers, param defaults, and per-node atom call storage, then executes the compiled schedule. Multi-channel ports, state buffer descriptors, and generalized runtime I/O mapping remain future work.
