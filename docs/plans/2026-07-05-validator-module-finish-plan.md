# Validator Module Finish Plan

## Goal

Validator owns semantic contract correctness using metadata, while preserving the same graph shape for compiler input.

## Current Status

Complete.

Done:

- Unit/project validators own schema, metadata-reference, known profile, compatibility, port, param, route, and scene checks.
- Unknown unit compatibility profiles are rejected.
- Project target profiles use shared metadata profile knowledge.
- Atom binding field validation stays in compiler.

## Remaining Implementation

- [ ] Do not move compiler-owned atom binding field resolution into validator.
- [ ] Keep validator output runtime-free: no schedule, storage, atom-call, or runtime-image fields.
- [ ] If project route bugs appear, fix them in project validator before compiler receives the graph.

## Tests

- Unit validator tests cover schema, params, ports, compatibility, known atoms, duplicate names, and control target semantics.
- Project validator tests cover duplicate units/nodes, missing refs, bad routes, unsafe unit files, scenes, and targets.
- Contract validator tests cover parser-accepted/validator-rejected boundaries.

## Exit Criteria

- Validator receives raw graph and returns validated graph or diagnostics.
- Compiler does not need to reject basic project route shape errors.
- Validator does not allocate or plan runtime resources.
