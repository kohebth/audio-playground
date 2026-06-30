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

- **Status:** Narrowed
- **Context:** Phase V added `apg-v2` validation and inspect JSON contracts plus golden outputs for frontend tests. Phase X3 added deterministic project render JSON through `apg-v2 render project <path>`.
- **Reason:** Render output is stable and sufficient for the web UI handoff; a benchmark command still needs a non-flaky contract before it can support regression or performance checks.
- **Follow-up:** Add benchmark output with deterministic structural fields and clearly separated timing fields if performance data is needed before broader rollout.
- **Related Plan Item:** `plan.md` Phase V3.
