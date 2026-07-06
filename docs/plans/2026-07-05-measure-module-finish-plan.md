# Measure Module Finish Plan

## Goal

Measure is the only host/tooling read layer for runtime state, meters, diagnostics, and snapshots.

## Current Status

Complete for the current host/tooling read boundary. Production host/tooling code uses runtime public APIs for execution
and measure APIs for runtime diagnostics/read snapshots; direct runtime-internal reads are limited to runtime, measure,
and focused tests.

Done:

- Measure owns snapshots, meters, diagnostics, and host/tooling reads.
- Runtime read compatibility wrappers for meters and last error were removed.
- Tests read diagnostics through measure APIs.

## Remaining Implementation

- [x] Audit host/tooling code for direct runtime internals reads that should go through measure.
- [x] Keep measure read-only with respect to DSP execution state, except copying snapshots out.
- [x] Add readiness snapshots only if host/UI needs runtime state already available from registry/runtime; keep export-specific JSON in CLI/export code.

## Tests

- `test_measure_v2` must cover snapshots, meters, diagnostics, and non-mutating reads.
- Add one test if an audit moves a direct runtime read into measure.

## Exit Criteria

- Host/tooling reads runtime state through measure APIs.
- Measure does not own DSP execution or mutation.
- Public measure types remain independent from runtime internals.
