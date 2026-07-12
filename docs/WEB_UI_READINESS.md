# Web UI Readiness Checklist

This checklist defines what must be true before the v2 web UI becomes the main workstream. The goal is to give the frontend stable backend contracts for visual editing, validation, preview, and export.

## Current Backend Status

- APGCore v2 loader, compiler, scheduler, runtime MVP, fixtures, host bridge, control-to-param routing, atom catalog export, project schema validation, resolved project unit loading, mono project compilation, validate/inspect JSON contracts, and runtime product controls for params, bypass, mute, and meters are implemented. Solo remains a host/UI routing concern until a real routing contract exists.
- `unit.v2.yaml` is executable and tested, and optional unit/param UI metadata is parsed and validated.
- Reusable test metadata fixtures exist in `test/fixtures/units-v2/`, including representative overdrive, delay, tremolo, tone stack, noise gate, and wet/dry mix graphs.
- Project/session schema, deterministic test metadata fixtures, referenced-unit resolution, mono project compilation, and `test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml` exist.
- The `apg-v2` CLI emits structured validation JSON, inspect JSON for atoms/units/projects, deterministic project render/benchmark JSON, and export surfaces for `wasm_realtime` and `m7_static`. Validation, unit inspect, project inspect, render, and atom catalog sample contracts are frozen under `test/golden/`.

## Readiness Declaration

The APGCore v2 backend and web MVP surfaces are ready for production hardening. Final MVP backend verification passed with `./build-and-test.sh` across all 20 CTest targets. Web verification passed with `npm run test`, `npm run build`, and `npm run lint` inside `web-tools/unit-editor/`.

This is not a hardware readiness declaration. STM32H7/M7 production deployment is not ready yet: the `m7_static` path is a bounded C11 export surface for compatible/simple projects, not proof that the full guitar-pedalboard project runs on target hardware. `wasm_realtime` now emits a deterministic scaffold (`.json` + `.mjs`) for future AudioWorklet integration.

The real browser runtime is now tracked as the separate `wasm-tools/` project. Its versioned control ABI accepts
revisioned in-memory project/unit YAML, resolves unit references without a filesystem, validates and compiles the entry
project, and reports structured diagnostics and schedule summaries. Emscripten build targets and a build gate prevent
browser dependencies from entering APGCore. Prepared runtime images and AudioWorklet processing remain pending slices;
the existing generated project-specific scaffold is not the frontend runtime authority.

## Ready To Start Web UI When

- [x] Unit metadata can render a parameter panel without frontend hardcoding.
- [x] Atom catalog metadata can render an atom palette without frontend hardcoding.
- [x] Project files can reference units, define routes between unit instances, and compile mono routes into one runtime plan.
- [x] Validation returns structured errors and warnings with stable file/path fields.
- [x] A guitar pedalboard fixture validates, compiles, runs, and renders deterministically.
- [x] Runtime supports the first live UI controls: parameter changes, bypass, and meters.
- [x] Sample JSON outputs are committed for frontend tests and UI mock data, including unit inspect, project inspect, atom catalog, and guitar pedalboard fixture contracts.

## Frozen Backend Samples

Build the CLI once through the normal C workflow, then use these exact commands as frontend fixture metadata sources:

```sh
./build/apg-v2 validate unit test/fixtures/units-v2/simple_gain.unit.v2.yaml
./build/apg-v2 validate project test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml
./build/apg-v2 inspect atoms
./build/apg-v2 inspect unit test/fixtures/units-v2/simple_gain.unit.v2.yaml
./build/apg-v2 inspect project test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml
./build/apg-v2 render project test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml
./build/apg-v2 benchmark project test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml
./build/apg-v2 export --target wasm_realtime test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml dist/web/
./build/apg-v2 export --target m7_static test/fixtures/projects-v2/two-gain-chain.project.v2.yaml build/m7/
```

Committed sample files:

- `test/golden/v2-validate-unit-simple_gain.json`
- `test/golden/v2-validate-project-two-gain-chain.json`
- `test/golden/v2-validate-project-guitar-pedalboard.json`
- `test/golden/v2-inspect-unit-simple_gain.json`
- `test/golden/v2-inspect-project-two-gain-chain.json`
- `test/golden/v2-inspect-project-guitar-pedalboard.json`
- `test/golden/v2-render-project-guitar-pedalboard.json`
- `test/golden/v2-inspect-atoms.json`
- `test/golden/v2-inspect-atoms.manifest.txt`

## Backend Contracts Needed

### Unit Inspect Contract

The UI needs a structured view of one unit:

- `name`, `version`, `meta`, `compatibility`
- public params with defaults, ranges, smoothing, and UI hints
- input/output ports and channel layout
- graph nodes and binding errors for unit-internals editing

### Atom Catalog Contract

The UI needs a structured atom palette:

- atom name and category
- input, output, config, and state fields
- supported target profiles
- constraints such as fixed-size or stateful behavior

### Project Contract

The UI needs a full pedalboard/session model:

- unit references
- unit instances
- routes between instances
- scenes and preset parameter values
- target backend settings

### Validation Contract

The UI needs stable diagnostics:

```json
{
  "ok": false,
  "errors": [
    {
      "code": "APG_ATOM_UNKNOWN",
      "file": "test/fixtures/units-v2/example.unit.v2.yaml",
      "path": "graph.nodes[2].atom",
      "message": "Unknown atom 'filter_biquadd'"
    }
  ],
  "warnings": []
}
```

### Runtime Preview Contract

The UI needs a way to drive live or offline preview:

- set parameter by stable instance path
- bypass unit instance
- read meters
- compile/swap a valid graph without replacing active audio on failure
- move or replace units by preparing a resolved project in host, committing only a valid replacement runtime, and
  crossfading mono preview output after commit

## Phase Gates

- **Phase Q:** Unit schema validates UI metadata.
- **Phase R:** Atom catalog is exportable.
- **Phase S:** Project schema and fixtures exist.
- **Phase T:** Project loader resolves multi-file units safely. Complete.
- **Phase U:** Project compiler creates a single runtime plan. Complete for mono project routes.
- **Phase V:** CLI tooling emits JSON inspect/validate output and deterministic project render output. Complete.
- **Phase AH:** CLI tooling emits deterministic benchmark JSON, deterministic `wasm_realtime` export scaffolds, and bounded C11 M7 static bundles for compatible/simple projects. Complete as an export surface, not as STM32H7 production readiness.
- **Phase W:** Runtime supports product controls and meters. Complete for params, bypass, mute, and peak/RMS meter snapshots.
- **Phase X:** Representative unit fixture metadata, the guitar pedalboard project fixture metadata, deterministic render proof, and compatibility/output capture are complete.
- **Phase Y:** Web handoff package freezes sample contracts, documents exact fixture commands, refreshes repo guidance, and declares backend readiness. Complete.

## First Web UI Scope After Gate

Start with the actual product workflow, not a landing page:

- project browser using committed sample projects
- pedalboard canvas for unit instances and routes
- inspector for validation errors and compatibility
- parameter panel from unit metadata
- atom-level unit editor after project-level workflow is usable
