# Runtime Module Finish Plan

## Goal

Runtime is audio-callback-safe execution over prebuilt registry memory and schedule.

## Current Status

Mostly complete.

Done:

- Runtime initializes from registry.
- Runtime executes registry-owned schedule and atom thunks.
- Runtime owns audio/control execution state, not parsing, validation, compilation, or layout planning.
- Public runtime header exposes an opaque handle.

## Remaining Implementation

- [ ] Audit all functions called by `apg_v2_runtime_process*` for allocation, file I/O, YAML/parser use, compiler use, string parsing, metadata lookup, locks, and platform calls.
- [ ] Keep bypass and mute in runtime because they alter output samples.
- [ ] If the audit finds a real-time-path lookup or allocation bug, fix that root cause and add one focused regression test.
- [ ] Keep multi-channel helpers thin over registry-owned audio-port maps.

## Tests

- Runtime tests must cover schedule execution, params, smoothing, bypass, mute, meters, state buffers, and cleanup.
- Generated M7 runner tests must continue rejecting allocation, parser, loader, and runtime-init symbols.

## Exit Criteria

- Audio callback path walks prebuilt arrays and calls atom thunks only.
- No runtime-time YAML, graph traversal, allocation, or metadata resolution.
- Runtime tests and M7 generated runner tests pass.
