# APGCore v2 JSON Contracts

`apg-v2` emits compact JSON for frontend validation, inspection, and fixture tests.

## Commands

```sh
apg-v2 validate unit units-v2/simple_gain.unit.v2.yaml
apg-v2 validate project projects-v2/two-gain-chain.project.v2.yaml
apg-v2 inspect atoms
apg-v2 inspect unit units-v2/simple_gain.unit.v2.yaml
apg-v2 inspect project projects-v2/two-gain-chain.project.v2.yaml
apg-v2 render project projects-v2/guitar-pedalboard.project.v2.yaml
apg-v2 benchmark project projects-v2/guitar-pedalboard.project.v2.yaml
apg-v2 export --target wasm_realtime projects-v2/guitar-pedalboard.project.v2.yaml dist/web/
apg-v2 export --target m7_static projects-v2/two-gain-chain.project.v2.yaml build/m7/
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

## Render

`render project` returns `apg.project.render.v1` with deterministic mono input metadata, frame count, peak/RMS/sum, and sample output. It is the current browser preview fixture contract.

## Benchmark

`benchmark project` returns `apg.project.benchmark.v1` with deterministic structural fields and `timing.available:false`. Timing fields are intentionally absent from the stable contract until a non-flaky benchmark runner exists.

## Export

`export --target wasm_realtime` returns `apg.project.export.v1` with `ok:false` and `APG_EXPORT_BLOCKED` until the WASM AudioWorklet bundle generator is implemented.

`export --target m7_static` validates target compatibility. Compatible projects emit `apg_project_m7.h` and `apg_project_m7.c` with bounded C11 tables and no runtime YAML parser. Export JSON and the generated header include block-frame, byte-count memory manifests, and static atom-call workload fields derived from runtime-image layout metadata. Use `--max-static-ram <bytes>` to reject bundles whose static RAM manifest exceeds a board budget. The header declares `APG_M7_PROJECT_USES_RUNTIME_YAML 0u` and `APG_M7_PROJECT_USES_DYNAMIC_ALLOCATION 0u`; CTest rejects generated source that contains allocation, YAML, loader, or runtime-init symbols. Generated source declares section-placed RAM buffers for signal buffers, params, atom calls, atom storage, and state buffers, per-node atom thunk pointers, and atom process symbol names for firmware link planning. Configure CMake with `-DAPG_M7_C_COMPILER=/path/to/arm-none-eabi-gcc` to syntax-check generated bundles as freestanding Cortex-M7 C. `docs/STM32H7_M7_BOARD_INTEGRATION.md` defines the board callback, DMA, cache, and measure boundary expected from firmware integration. Unsupported units return stable diagnostics.
