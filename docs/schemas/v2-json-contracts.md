# APGCore v2 JSON Contracts

`apg-v2` emits compact JSON for frontend validation, inspection, and fixture tests.

## Commands

```sh
apg-v2 validate unit test/fixtures/units-v2/simple_gain.unit.v2.yaml
apg-v2 validate project test/fixtures/projects-v2/two-gain-chain.project.v2.yaml
apg-v2 inspect atoms
apg-v2 inspect unit test/fixtures/units-v2/simple_gain.unit.v2.yaml
apg-v2 inspect project test/fixtures/projects-v2/two-gain-chain.project.v2.yaml
apg-v2 render project test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml
apg-v2 benchmark project test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml
apg-v2 export --target wasm_realtime test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml dist/web/
apg-v2 export --target m7_static test/fixtures/projects-v2/two-gain-chain.project.v2.yaml build/m7/
```

## Validation

Validation output uses `apg.validation.v2` and always includes stable `ok`, `file`, `errors`, and `warnings` fields.

```json
{"schema":"apg.validation.v2","ok":true,"file":"test/fixtures/units-v2/simple_gain.unit.v2.yaml","errors":[],"warnings":[]}
```

Errors use this shape:

```json
{
  "code": "APG_IO_ERROR",
  "file": "test/fixtures/projects-v2/example.project.v2.yaml",
  "path": "$.project",
  "message": "cannot resolve unit file 'missing.unit.v2.yaml'"
}
```

Current paths are coarse (`$.unit` or `$.project`) and stable. More granular YAML paths can be added without changing the top-level contract.

## Inspection

- `inspect atoms` returns `apg.atom_catalog.v2` from the atom catalog writer.
- `inspect unit` returns `apg.unit.inspect.v2` with unit metadata, params, ports, signals, and graph node summaries.
- `inspect project` returns `apg.project.inspect.v2` with unit refs, chain nodes, routes, targets, and compiled plan counts.

Golden fixtures for frontend tests live under `test/golden/`.

## Render

`render project` returns `apg.project.render.v2` with deterministic mono input metadata, frame count, peak/RMS/sum, and sample output. It is the current browser preview fixture contract.

## Benchmark

`benchmark project` returns `apg.project.benchmark.v2` with deterministic structural fields and `timing.available:false`. Timing fields are intentionally absent from the stable contract until a non-flaky benchmark runner exists.

## Export

`export --target wasm_realtime` currently returns `apg.project.export.v2` with `ok:true` and a generated scaffold bundle in `apg_project_wasm.json`, `apg_project_wasm.mjs`, `apg_project_wasm_adapter.mjs`, and `apg_project_wasm_processor.js`. The manifest preserves the existing scaffold fields (`sample_rate`, `block_frames`, `runtime`, `layout`, and `status`) and adds an `artifacts` object naming the future `apg_project_wasm.wasm` module, adapter JS, entry JS, and AudioWorklet processor JS. `wasm_module_available:false` means the bundle remains a deterministic adapter shell, not a full AudioWorklet DSP executable yet.

`export --target m7_static` validates target compatibility. Compatible projects emit `apg_project_m7.h` and `apg_project_m7.c` with bounded C11 tables and no runtime YAML parser. Export JSON and the generated header include block-frame, byte-count memory manifests, signal-array pointer bytes, and static atom-call workload fields derived from registry layout metadata. Use `--max-static-ram <bytes>` to reject bundles whose static RAM manifest exceeds a board budget. The header declares `APG_M7_PROJECT_USES_RUNTIME_YAML 0u` and `APG_M7_PROJECT_USES_DYNAMIC_ALLOCATION 0u`; CTest rejects generated source that contains allocation, YAML, loader, or runtime-init symbols. Generated source declares section-placed RAM buffers for signal buffers, params, atom calls, typed atom storage, signal-array pointers, and state buffers, per-node atom thunk pointers, atom process symbol names, and `apg_m7_project_init`/`apg_m7_project_refresh_params`/`apg_m7_project_process_block` for firmware startup, control updates, and fixed-block execution. Configure CMake with `-DAPG_M7_C_COMPILER=/path/to/arm-none-eabi-gcc` to syntax-check generated bundles as freestanding Cortex-M7 C and check generated-runner stack usage; add `-DAPG_M7_LINKER_SCRIPT=/path/to/stm32h7.ld` to link the bundle with a target memory map. CTest links and runs the generated bundle against real atom thunks for the two-gain fixture, verifies generated object sections with `objdump` when available, measures host block time, and can run a BSP timing command via `APG_M7_BOARD_TIMING_COMMAND`. `docs/STM32H7_M7_BOARD_INTEGRATION.md` defines the board callback, DMA, cache, and measure boundary expected from firmware integration. Unsupported units or compiled atoms return stable diagnostics.
