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
