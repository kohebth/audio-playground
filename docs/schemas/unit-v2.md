# Unit v2 Schema

`unit.v2.yaml` describes one reusable DSP unit. It is a source format for validation and graph compilation, not the runtime execution plan. The compiler must translate it into numeric node IDs, bound buffers, state blocks, params, and a stable schedule before audio processing.

## Required Top-Level Fields

- `kind`: Must be `apg.unit`.
- `schema`: Must be `apg.unit.v2` for this format version.
- `name`: Stable snake_case unit identifier, for example `simple_gain`.
- `version`: Semantic version for the unit definition.
- `params`: Public controls exposed by the unit.
- `ports`: External audio/control inputs and outputs.
- `graph`: Internal atom graph.
- `compatibility`: Target backend support flags.

## Params

Each param is keyed by name and must define:

- `type`: `float`, `int`, or `bool`.
- `default`: Initial value.
- `min` / `max`: Required for numeric params.
- `smoothing_ms`: Optional smoothing hint; default is `0`.
- `ui`: Optional display metadata such as `label`, `control`, and `unit`.

Example:

```yaml
params:
  gain:
    type: float
    default: 1.0
    min: 0.0
    max: 4.0
    smoothing_ms: 10
```

## Ports

Ports define the unit boundary. MVP port types are `audio` and `control`; audio ports must declare `channels`.

```yaml
ports:
  inputs:
    - name: input
      type: audio
      channels: 1
  outputs:
    - name: output
      type: audio
      channels: 1
```

## Graph

`graph.signals` declares internal signal names. `graph.nodes` is an ordered source representation; the compiler validates dependencies and emits the final schedule.

Each node must define:

- `id`: Unique node ID within the unit.
- `atom`: Atom registry name.
- `in`: Atom input bindings, when required.
- `out`: Atom output bindings, when required.
- `config`: Literal or `${params.name}` config bindings, when required.

## Compatibility

Compatibility flags document intended targets and guide validation.

```yaml
compatibility:
  desktop_full: true
  wasm_realtime: true
  m7_static: true
  offline_render: true
```

## MVP Validation Rules

- Unknown top-level required fields are errors only if required data is missing; extra fields should be warnings until the schema stabilizes.
- Every public audio port must map to a graph signal with the same name.
- Every node ID must be unique.
- Every atom must exist in the atom registry.
- Every `${params.name}` reference must resolve to a declared param.
- Direct zero-delay cycles are invalid; feedback must use an explicit stateful atom.
