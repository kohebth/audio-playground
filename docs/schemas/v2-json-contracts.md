# APGCore v2 JSON Contracts

`apg-v2` emits compact JSON for frontend validation, inspection, and fixture tests.

## Commands

```sh
apg-v2 validate unit units-v2/simple_gain.unit.v2.yaml
apg-v2 validate project projects-v2/two-gain-chain.project.v2.yaml
apg-v2 inspect atoms
apg-v2 inspect unit units-v2/simple_gain.unit.v2.yaml
apg-v2 inspect project projects-v2/two-gain-chain.project.v2.yaml
```

## Validation

Validation output uses `apg.validation.v1` and always includes stable `ok`, `file`, `errors`, and `warnings` fields.

```json
{"schema":"apg.validation.v1","ok":true,"file":"units-v2/simple_gain.unit.v2.yaml","errors":[],"warnings":[]}
```

Errors use this shape:

```json
{
  "code": "APG_IO_ERROR",
  "file": "projects-v2/example.project.v2.yaml",
  "path": "$.project",
  "message": "cannot resolve unit file 'missing.unit.v2.yaml'"
}
```

Current paths are coarse (`$.unit` or `$.project`) and stable. More granular YAML paths can be added without changing the top-level contract.

## Inspection

- `inspect atoms` returns `apg.atom_catalog.v1` from the atom catalog writer.
- `inspect unit` returns `apg.unit.inspect.v1` with unit metadata, params, ports, signals, and graph node summaries.
- `inspect project` returns `apg.project.inspect.v1` with unit refs, chain nodes, routes, targets, and compiled plan counts.

Golden fixtures for frontend tests live under `test/golden/`.