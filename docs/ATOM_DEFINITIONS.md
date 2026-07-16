# Canonical Atom Definitions

`schema/atoms/atoms.json` is the authoritative inventory for atom ABI and binding metadata. It records:

- family headers, reusable I/O layouts, and family capacity constants;
- ordered atom-specific params and state fields with explicit ownership;
- input/config/state descriptors and state-buffer capacities;
- category, capability, maturity, and dispatch kind;
- backend, TypeScript, and JSON binding contracts.

Field order is C ABI order. Atom order is canonical registry order. Empty roles use the standard one-byte
`uint8_t _reserved` layout, which is never emitted as a logical contract field.

`tools/generate_atom_artifacts.pl` validates that source and owns these checked-in outputs:

- `inc/atom/types/dsp_type_macros.h` and all 11 family `*_types.h` headers;
- `inc/atom/generated/atom_definitions.generated.h` and `dsp_atoms.generated.h`;
- all 11 family `*_field_descriptors.c` files;
- `src/apgcore/metadata/atom_catalog_contracts.generated.inc`;
- `web-tools/unit-editor/src/atoms/atomCatalog.generated.ts`;
- `schema/atoms/atom.schema.json`.

`inc/atom/atom_definitions.h`, `inc/atom/dsp_atoms.h`, and the TypeScript `atomCatalog.ts` module are thin handwritten
wrappers for shared flags, runtime context includes, and category colors. `dsp_types.h` remains the stable include
umbrella. DSP algorithms, shared enums, runtime allocation, and compiler behavior remain handwritten.

## Workflow

To change or add an atom:

1. Edit `schema/atoms/atoms.json`; do not edit a generated output.
2. Reuse an I/O profile only when C member names and semantics match exactly.
3. Record pointer ownership and a bounded capacity for every runtime-owned state buffer.
4. Add or update the handwritten DSP implementation and focused behavior tests.
5. Regenerate and verify:

```sh
cmake --build build --target generate_atom_artifacts
cmake --build build --target check_atom_artifacts
./build-and-test.sh
```

Update `test/golden/v2-inspect-atoms.json` and its manifest when the public catalog changes. Run `npm run build` and
`npm run lint` in `web-tools/unit-editor/` when generated TypeScript changes.

`test_atom_artifact_generation` renders every output twice, compares each file byte for byte, checks the checked-in
tree, then deliberately modifies one output and proves the stale check rejects it. The ABI snapshot and public symbol
tests independently catch unintended C layout or declaration changes.
