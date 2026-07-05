# Compiler Module Finish Plan

## Goal

Compiler receives validated contracts plus metadata, expands projects/units, resolves graph bindings, and emits all graph facts needed by registry.

## Current Status

One remaining boundary slice.

Done:

- Project compiler expands resolved projects into a namespaced unit graph.
- Unit compiler resolves atom metadata, params, signals, binding keys, scalar literals, signal arrays, float matrices, signal producers, and topological schedule.
- Compiler emits instance I/O facts and node-to-instance indexes.
- Registry no longer infers bypass endpoints from node IDs.

## Remaining Implementation

- [ ] Add compiled atom-layout facts to each compiled node:
  - atom name
  - thunk
  - out/in/config/state storage sizes
  - input/config/state fields and field counts
- [ ] Populate atom layout in `apg_v2_compile_unit(...)` from atom registry metadata.
- [ ] Make registry consume compiled atom-layout facts instead of dereferencing `compiled_node.atom`.
- [ ] Keep `compiled_node.atom` as temporary compatibility only; do not remove it in this slice.
- [ ] Add tests proving registry still builds after `compiled_node.atom` is nulled on a normal compiled plan.

## Tests

- Extend `test_unit_v2_compile` for compiled atom layout names, thunks, sizes, and field counts.
- Extend `test_registry_v2` for registry build after nulling compiled node registry pointers.
- Keep schedule, producer, signal dependency, scalar literal, matrix, and fixture compile tests green.

## Exit Criteria

- Compiler output contains every graph and atom-layout fact registry needs.
- Registry no longer needs raw atom registry entries for normal compiled plans.
- `./build-and-test.sh` passes after the compiler slice.
