# Runtime Image Module Finish Plan

## Goal

Runtime image is the registration and memory-layout stage. Runtime consumes it without recomputing graph or layout facts.

## Current Status

Mostly complete; finish after compiler atom-layout materialization.

Done:

- Runtime image owns compact signal, param, schedule, scalar refresh, signal binding, state buffer, control target, bypass, mute, meter, audio-port, and node layout metadata.
- Runtime image copies schedule and no longer stores the compiled-plan pointer.
- Runtime-image consumers no longer include compiler headers.

## Remaining Implementation

- [ ] After compiler atom-layout materialization, remove runtime-image raw registry dependency from normal node layout construction.
- [ ] Keep runtime-image as layout/registration only; do not add DSP execution logic.
- [ ] Confirm M7 export uses runtime-image facts instead of recalculating conflicting layout.
- [ ] Keep documented borrowed strings stable, or copy them if lifetime tests prove borrowing unsafe.

## Tests

- Runtime-image tests must cover build in a separate arena and runtime init after compiled-plan mutation.
- M7 export tests must validate generated layout sections, memory manifest, stack gates, and static RAM budget.

## Exit Criteria

- Runtime image can be built from compiler graph facts without redoing graph inference.
- Runtime image is deterministic and arena-owned.
- Runtime-image tests pass after compiled-plan mutation.
