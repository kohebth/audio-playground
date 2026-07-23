# Project v2 Schema

`project.v2.yaml` describes a complete APGCore session or pedalboard made from reusable `unit.v2.yaml` files. The current implementation validates the project model and preserves enough structure for the next loader, resolver, and compiler phases.

## Required Top-Level Fields

- `kind`: Must be `apg.project`.
- `schema`: Must be `apg.project.v2`.
- `name`: Stable project identifier.
- `version`: Project definition version.
- `units`: Project-local catalog of unit references. It may be empty.
- `chain`: Unit instances and routes.
- `targets`: Default and export target profiles.

`scenes` is optional and stores named parameter snapshots.

## Unit References

`units` is a sequence of maps:

```yaml
units:
  - id: gain_unit
    file: ../units-v2/simple_gain.unit.v2.yaml
```

`id` values must be unique. `file` is stored as authored by the schema loader. Every declared path must remain relative
and confined to the workspace, including catalog entries that are not currently placed. The resolved loader
canonicalizes and loads only references used by `chain.nodes`; an active reference rejects missing files and duplicate
canonical unit files. Unused references remain available to the editor catalog and may point to a draft or missing file
until a node activates them.

## Chain Nodes and Routes

`chain.nodes` creates unit instances. Each node requires a unique `id` and a `unit` that matches a declared unit reference. Optional `params` entries override public unit params for that instance.

```yaml
chain:
  nodes:
    - id: gain1
      unit: gain_unit
      params:
        gain: 2.0
```

`chain.routes` connects endpoints. Endpoints are `system.input`, `system.output`, or `<node>.<port>`. The validator checks that referenced node IDs exist; port-name validation is deferred until referenced units are resolved.

Ordinary project effects must expose exactly one mono audio input and one mono audio output. Every route source and route
target may be connected only once. Raw fan-out and raw merges are rejected; use an explicit routing section instead.

## Explicit Split/Merge Sections

A routing section pairs one panner with one mixer. Both instances declare the same `routing.section`, and their unit
metadata must expose the same ordered paths and level params:

```yaml
chain:
  nodes:
    - id: parallel_pan
      unit: path_panner_2_unit
      routing:
        section: parallel_1
    - id: drive
      unit: overdrive_unit
    - id: parallel_mix
      unit: path_mixer_2_unit
      routing:
        section: parallel_1
  routes:
    - from: system.input
      to: parallel_pan.input
    - from: parallel_pan.path_1
      to: parallel_mix.path_1
    - from: parallel_pan.path_2
      to: drive.input
    - from: drive.output
      to: parallel_mix.path_2
    - from: parallel_mix.output
      to: system.output
```

Every panner path must reach the same-named input on its paired mixer exactly once. Crossed, leaking, incomplete,
orphaned, and cyclic paths are rejected. Sections may be nested; validation treats a complete nested panner/mixer pair
as a serial macro. The metadata shape can describe N paths, but the shipped project system currently accepts exactly two.

## Scenes

`scenes` is a sequence of named snapshots:

```yaml
scenes:
  - name: Boost
    params:
      gain1.gain: 3.0
    bypass:
      gain1: false
```

Scene names must be unique. Scene param keys use `<node>.<param>` and must reference an existing node. Optional `bypass` entries map an instance ID to a boolean, where `true` means bypassed. Param-name validation is deferred until unit resolution.

Panner and mixer instances are always active and cannot appear in scene `bypass` maps. Their per-path dB params can be
stored in scene `params` like any other runtime parameter.

## Targets

`targets.default` is required. `targets.export` is optional and must not contain duplicate profiles.

Supported profiles:

- `desktop_full`
- `wasm_realtime`
- `m7_static`
- `offline_render`

## Resolved Loading

Use `apg_project_v2_load_resolved_file(...)` when the caller needs loaded unit definitions in addition to the project
schema model. `project.units` retains the full declared catalog, while `units`/`units_len` contains only active
dependencies referenced by chain nodes. The returned project, canonical active-unit paths, and loaded units are
arena-owned.

## Project Compilation

Use `apg_project_v2_compile(...)` to expand a resolved project into a synthetic v2 unit and compile it with the existing unit compiler. The compiler namespaces instance internals with `<node>.<name>`, preserves stable runtime params such as `gain1.gain`, applies node `params` as instance defaults, and lowers mono routes into a single runtime plan.

The current compiler accepts mono audio routes from `system.input` through zero or more unit instances and explicit
routing sections to exactly one `system.output` route. Inter-instance routes such as `gain1.output -> gain2.input` are
supported. Routing helper instances are compiled as non-bypassable. An empty project must declare `chain.nodes: []` and
exactly one direct `system.input -> system.output` route. It may retain unused catalog entries in `units` and still
compiles to a zero-node pass-through runtime plan.

## Current Limits

Project compilation is mono-route only. The current routing library supplies two-path panner/mixer helpers; additional
path counts, stereo/multi-channel project routes, multiple system inputs/outputs, and non-audio project routes are later
work. Structured diagnostics, routing-aware project inspect JSON, runtime product controls, meters, and deterministic
project render JSON are implemented.
