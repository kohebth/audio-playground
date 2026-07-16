# DSP Type Generation Design

## Status

Production generation is active for all 69 atoms and all 11 families. `schema/atoms/atoms.json` uses
`apg.atom-definitions.v1` and is authoritative; the former `src`-only candidate schema and generator have been removed.

JSON keeps generation dependency-free through Perl's core `JSON::PP` parser. Generated files are checked in so C,
TypeScript, package builds, and downstream target exports do not require generation at consumer build time.

## Source Model

The schema contains three ordered collections:

- families: generated paths, guards, table macros, categories, and reusable capacity constants;
- I/O profiles: exact C member layouts plus binding type and required/optional metadata;
- atoms: family/profile selection, params, state, ownership, descriptors, capability, maturity, dispatch, and catalog
  overrides.

The generator rejects unsupported C and contract types, duplicate names, parent-path traversal, sample-rate params,
unowned pointers, state buffers without explicit `buffer_len`, unknown capacity constants, invalid descriptor counts,
and unsupported dispatch/capability values. Pointer ownership is one of `borrowed`, `runtime_owned`, or `external`;
scalars use `value`.

## Generated Surfaces

One invocation generates:

1. reusable I/O macros and every family ABI header;
2. canonical atom rows and context-correct public declarations;
3. family input/config/state descriptor definitions;
4. backend catalog contracts for all atoms;
5. the unit-editor TypeScript catalog;
6. a draft-2020-12 JSON Schema for atom bindings.

The generator does not emit DSP algorithms, runtime thunks, allocation code, shared enums, or UI category colors.
Those surfaces contain behavior or policy beyond declarative atom metadata.

## Commands

```sh
perl tools/generate_atom_artifacts.pl schema/atoms/atoms.json .
perl tools/generate_atom_artifacts.pl --check schema/atoms/atoms.json .
cmake --build build --target generate_atom_artifacts
cmake --build build --target check_atom_artifacts
ctest --test-dir build -R '^test_atom_artifact_generation$' --output-on-failure
```

Every output carries a generated-file banner. `--check` performs an exact in-memory comparison and reports every
missing or stale path without rewriting it.

## Verification

`test_atom_artifact_generation` generates two independent trees and compares a fixed output manifest byte for byte. It
then checks the repository tree, mutates a generated declaration in one temporary tree, and requires `--check` to name
that stale path. Header smoke tests, the 69-atom registry contract, the 72-symbol link test, and the LP64 ABI snapshot
remain independent gates.

Public catalog changes also require the frozen backend JSON sample and frontend build/lint checks. Algorithm behavior
continues to be covered by the normal CTest suite.

## Change Rules

1. Change schema and generator together when introducing a new field shape or metadata capability.
2. Preserve ordered fields unless an intentional ABI change is documented and snapshotted.
3. Never hand-edit a generated file to repair drift.
4. Keep runtime allocation and process behavior outside the generator.
5. Review regenerated C, TypeScript, schema, and golden diffs as one contract change.
