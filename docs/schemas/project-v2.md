# Project v2 Schema

`project.v2.yaml` describes a complete APGCore session or pedalboard made from reusable `unit.v2.yaml` files. The current implementation validates the project model and preserves enough structure for the next loader, resolver, and compiler phases.

## Required Top-Level Fields

- `kind`: Must be `apg.project`.
- `schema`: Must be `apg.project.v2`.
- `name`: Stable project identifier.
- `version`: Project definition version.
- `units`: Non-empty list of referenced unit files.
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

`id` values must be unique. `file` is stored as authored by the schema loader. The resolved loader canonicalizes each file relative to the project file directory, loads the referenced `unit.v2.yaml`, rejects absolute paths, rejects missing files, rejects references that escape the current workspace root, and rejects duplicate canonical unit files.

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

## Scenes

`scenes` is a sequence of named snapshots:

```yaml
scenes:
  - name: Boost
    params:
      gain1.gain: 3.0
```

Scene names must be unique. Scene param keys use `<node>.<param>` and must reference an existing node. Param-name validation is deferred until unit resolution.

## Targets

`targets.default` is required. `targets.export` is optional and must not contain duplicate profiles.

Supported profiles:

- `desktop_full`
- `wasm_realtime`
- `m7_static`
- `offline_render`

## Resolved Loading

Use `apg_project_v2_load_resolved_file(...)` when the caller needs loaded unit definitions in addition to the project schema model. The returned project, canonical unit paths, and loaded units are arena-owned.

## Current Limits

The project loader does not yet check unit port names, validate instance param names against unit schemas, or compile routes into a single runtime plan. Those behaviors are tracked in Phase U.