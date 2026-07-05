# Host Module Finish Plan

## Goal

Host orchestrates parser, validator, compiler, runtime image, runtime, and measure without exposing their internals publicly.

## Current Status

Usable but not production-closed.

Done:

- Host APIs orchestrate resolved project load, compile, runtime-image build, runtime init, and mono processing.
- CLI validate/render/export paths exist.
- Host/project loading uses explicit runtime-image initialization.

## Remaining Implementation

- [ ] Audit `host_v2.h` for exposed compiler/runtime-image/runtime internals.
- [ ] If public host structs still expose module internals, replace them with opaque handles and move struct definitions into implementation/private headers.
- [ ] Preserve simple usage: load file, set param, process mono ports, read diagnostics through measure/host, destroy.
- [ ] Keep host orchestration-only: no atom behavior, graph scheduling, metadata compatibility, or meter math.
- [ ] Add include-only public-header tests for host/runtime/measure/parser/validator headers if header dependencies remain unclear.

## Tests

- Existing host/project tests should need mechanical updates only if host handles become opaque.
- Add public-header compile tests when hiding host structs.

## Exit Criteria

- Host can load, validate, compile, image, run, measure, and export without reaching into runtime internals.
- Public host users cannot mutate compiler/runtime-image/runtime fields directly.
- Host remains orchestration-only.
