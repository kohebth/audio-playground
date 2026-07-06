# Host Live Swap Plan

## Goal

Handle live project topology edits in the host layer while audio is playing. Runtime remains an immutable, compiled,
index-driven schedule.

## Current Status

Runtime supports live parameter changes, project mute, instance bypass, meters, and fixed-schedule processing. Host now
keeps active and prepared project bundles separate, swaps only fully built replacement runtimes, and crossfades mono
preview output after commit.

## Remaining Implementation

- [x] Add a host-owned project bundle that keeps project arena, compiled plan, registry arena, registry, and runtime
      alive together.
- [x] Add prepared project swap APIs sourced from `apg_project_v2_resolved_t`.
- [x] Preserve host control shadow values for matching params, bypass instances, and project mute across swaps.
- [x] Commit valid prepared swaps at the host boundary and leave the active runtime untouched on prepare failure.
- [x] Crossfade mono project output from the previous active runtime to the new runtime after commit.
- [x] Keep runtime unchanged: no route mutation, text lookup, allocation, compiler, registry, or locks in the runtime
      process path.

## Tests

- [x] Host can bypass/unbypass a project instance while processing.
- [x] Host can prepare and commit a replacement project.
- [x] Invalid prepare leaves the active runtime output unchanged.
- [x] Matching parameter and bypass values survive a swap.
- [x] Crossfade output starts between old and new runtime outputs and finishes on the new runtime.

## Exit Criteria

- [x] `./build-and-test.sh` passes.
- [x] The slice is committed separately from unrelated local changes.
- [x] Docs identify live topology edits as host compile/swap behavior, not runtime graph mutation.
