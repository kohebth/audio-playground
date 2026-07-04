# Deferred Problems

This file tracks known issues that should not block the current phase but need explicit follow-up before broader APGCore v2 rollout.

## Delay Tap Binding Model

- **Status:** Resolved
- **Context:** `delay_tap_feedback` and `delay_tap_feedforward` have atom inputs such as `buffer` and `tap_position`, not just ordinary audio signal pointers.
- **Resolution:** The v2 compiler now treats `in.tap_position` for delay tap atoms as a scalar literal or `${params.name}` binding while keeping `in.buffer` as a graph signal. The runtime binds delay tap input fields by descriptor offset and refreshes scalar input params before each process call.
- **Related Plan Item:** `plan.md` Phase H2b.

## Optional Atom Bindings

- **Status:** Resolved
- **Context:** `filter_comb_fb` has `in.signal` and an optional `in.delay`; when `in.delay` is null, the atom falls back to `config.delay_samples`.
- **Resolution:** Compiler binding metadata now supports atom-specific optional keys and validates `filter_comb_fb` with required `in.signal` plus optional `in.delay`.
- **Related Plan Item:** `plan.md` Phase H3b.

## Array and Matrix Bindings

- **Status:** Resolved
- **Context:** `mix_matrix` uses `float **signals` for inputs/outputs and `float **coefficients` plus `num_in`/`num_out` config.
- **Resolution:** The v2 loader preserves raw binding nodes, the compiler supports signal-array and float-matrix compiled binding kinds, and runtime allocates owned pointer arrays for `mix_matrix` input/output signals and coefficient rows.
- **Related Plan Item:** `plan.md` Phase H4b.

## Multi-Channel Public Ports

- **Status:** Resolved
- **Context:** v2 audio ports now support explicit `signals` arrays for channel-to-graph-signal mapping.
- **Resolution:** Mono ports may still use a same-named signal. Multi-channel ports require one mapped signal per channel, and `apg_v2_runtime_process_interleaved_ports(...)` deinterleaves/interleaves buffers through those signal mappings.
- **Related Plan Item:** `plan.md` Phase I2b.

## Non-Param Control Routing

- **Status:** Narrowed
- **Context:** `apg_v2_runtime_set_control_port(...)` maps control input ports to normalized param targets parsed from same-name routing, legacy `target_param`, or `target: { kind: param, name: <param> }`.
- **Resolution:** Phase O implemented param-only explicit routing, loader validation for unknown param targets, and rejection for unsupported `target.kind` values.
- **Remaining Design:** Graph-signal, multi-param, smoothing-lane, and typed-buffer routing remain out of scope until a future phase defines their semantics and runtime representation.
- **Related Plan Item:** `plan.md` Phase O.

## Benchmark CLI Surface

- **Status:** Resolved
- **Context:** Phase V added `apg-v2` validation and inspect JSON contracts plus golden outputs for frontend tests. Phase X3 added deterministic project render JSON through `apg-v2 render project <path>`.
- **Resolution:** Phase AH added `apg-v2 benchmark project <path>` with deterministic structural fields and `timing.available:false`.
- **Related Plan Item:** `plan.md` Phase V3.

## Full Atom Catalog Frontend Fixture

- **Status:** Resolved
- **Context:** `docs/WEB_UI_READINESS.md` lists `apg-v2 inspect atoms`, but `test/golden/` previously stored only `v2-inspect-atoms.manifest.txt` rather than the full atom catalog JSON payload.
- **Resolution:** Phase AC committed `test/golden/v2-inspect-atoms.json` and the project workbench atom palette now reads atom fields, categories, statefulness, and compatibility profiles from that frozen backend JSON. The manifest remains as byte/hash provenance.
- **Related Plan Item:** `plan.md` Phase AC.

## Browser WASM AudioWorklet Preview

- **Status:** Partially Resolved
- **Context:** Phase AF defines the web preview adapter boundary and UI state machine, but the browser runtime still uses deterministic render JSON rather than a real WASM AudioWorklet backend.
- **Status update:** `apg-v2 export --target wasm_realtime` now emits a deterministic scaffold manifest (`apg_project_wasm.json`) and a stub adapter (`apg_project_wasm.mjs`), enabling adapter wiring and deployment tests.
- **Follow-up:** Replace the scaffold with a real WASM AudioWorklet runtime that performs compile/start/stop, param/bypass commands, and meter polling in audio thread.
- **Related Plan Item:** `plan.md` Phase AF.

## Export CLI Surface

- **Status:** Partially Resolved
- **Context:** Phase AH added deterministic benchmark JSON plus `wasm_realtime` and `m7_static` export command surfaces. M7 emits bounded C11 tables for compatible projects and rejects unsupported units.
- **Follow-up:** Extend `wasm_realtime` export from the current deterministic scaffold to a runnable AudioWorklet bundle and document exact runtime artifact requirements.
- **Related Plan Item:** `plan.md` Phase AG/AH.

## STM32H7 Production Readiness

- **Status:** Partially Resolved
- **Context:** `noise_gate`, `overdrive`, `tone_stack`, `tremolo`, `delay`, and `wet_dry_mix` now declare `m7_static: true`, and `projects-v2/guitar-pedalboard.project.v2.yaml` now includes `m7_static` in `targets.export`. The project now exports as M7-compatible at toolchain level.
- **Status update:** `apg-v2 export --target m7_static projects-v2/guitar-pedalboard.project.v2.yaml` now succeeds after enabling these compat flags, and fixture output (`test/golden/v2-inspect-project-guitar-pedalboard.json`) is updated accordingly.
- **Remaining Work:** STM32H7 hardware production is still blocked on real board-side integration (SAI/I2S + DMA callback ownership, cache invalidation/clean, board memory-map placement, fixed block adaptation, and measured CPU/RAM budgeting).
- **Related Plan Item:** New production pass to add concrete STM32H7 integration hardware readiness gates (targeted in `plan.md`/`task.md`).
