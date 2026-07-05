# Measure Module Finish Plan

## Goal

Measure is the only host/tooling read layer for runtime state, meters, diagnostics, and snapshots.

## Current Status

Mostly complete.

Done:

- Measure owns snapshots, meters, diagnostics, and host/tooling reads.
- Runtime read compatibility wrappers for meters and last error were removed.
- Tests read diagnostics through measure APIs.

## Remaining Implementation

- [ ] Audit host/tooling code for direct runtime internals reads that should go through measure.
- [ ] Keep measure read-only with respect to DSP execution state, except copying snapshots out.
- [ ] Add readiness snapshots only if host/UI needs runtime state already available from registry/runtime; keep export-specific JSON in CLI/export code.

## Tests

- `test_measure_v2` must cover snapshots, meters, diagnostics, and non-mutating reads.
- Add one test if an audit moves a direct runtime read into measure.

## Exit Criteria

- Host/tooling reads runtime state through measure APIs.
- Measure does not own DSP execution or mutation.
- Public measure types remain independent from runtime internals.
