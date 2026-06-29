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

- **Status:** Open
- **Context:** `apg_v2_runtime_set_control_port(...)` maps control input ports to `target_param` when present, otherwise to same-named params.
- **Problem:** The schema does not yet define whether control ports can drive graph signals directly, multiple params, smoothing lanes, or typed control buffers.
- **Later Fix:** Define non-param control routing semantics before adding graph-control propagation beyond param updates.
- **Related Plan Item:** Future runtime/control routing phase.
