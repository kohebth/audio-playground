# Deferred Problems

This file tracks known issues that should not block the current phase but need explicit follow-up before broader APGCore v2 rollout.

## Delay Tap Binding Model

- **Status:** Open
- **Context:** `delay_tap_feedback` and `delay_tap_feedforward` have atom inputs such as `buffer` and `tap_position`, not just ordinary audio signal pointers.
- **Problem:** The current v2 compiler/runtime treats all `in` bindings as graph signal buffers and binds them by storage order. Adding compiler contracts for tap atoms now would either reject useful graphs or incorrectly model scalar/buffer inputs as normal signals.
- **Later Fix:** Add explicit binding kinds or descriptor-aware binding validation for signal buffers, state buffers, scalar inputs, and config fields before validating delay tap atoms.
- **Related Plan Item:** `plan.md` Phase H2b.

## Optional Atom Bindings

- **Status:** Open
- **Context:** `filter_comb_fb` has `in.signal` and an optional `in.delay`; when `in.delay` is null, the atom falls back to `config.delay_samples`.
- **Problem:** Compiler binding metadata currently marks every listed key as required. Adding `filter_comb_fb` metadata now would force an optional delay signal for all units.
- **Later Fix:** Extend atom binding schemas with optional keys before validating atoms that support optional inputs.
- **Related Plan Item:** `plan.md` Phase H3b.

## Array and Matrix Bindings

- **Status:** Open
- **Context:** `mix_matrix` uses `float **signals` for inputs/outputs and `float **coefficients` plus `num_in`/`num_out` config.
- **Problem:** The current compiler/runtime binds named fields to individual graph signal buffers. It does not model arrays of signal pointers or matrix-valued config data.
- **Later Fix:** Add array-valued binding support and structured config parsing before validating or executing `mix_matrix` in v2.
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
