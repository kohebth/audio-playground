# DSP Type Generation Design

## Status

Production type headers remain handwritten and authoritative. Phase 7 adds a candidate schema and generator for the
low-risk `src` family only; generated output is written under the build directory and is never installed over
`inc/atom/types/src_types.h`.

The prototype proves that a structured source can reproduce the current family ABI table exactly before ownership is
transferred to generated files.

## Schema

`schema/atoms/src.json` uses `apg.dsp-type-family.v1`. JSON was selected so the generator can use Perl's core
`JSON::PP` parser without a new YAML dependency or ad hoc text parsing.

Each family document records:

- category, public header guard/include, and family table macro;
- ordered atom names and input/output ABI profiles;
- ordered parameter and state fields with C type, metadata type, and ownership;
- the standard one-byte empty-layout policy;
- dispatch, capability profile, maturity, and registry field counts.

Field order is ABI order. Atom order must be lexical for deterministic output. Registry config/state counts must equal
their schema field counts. The prototype accepts the C field shapes currently used by the repository, including
scalars, `uint32_t`, `float *`, and `float **`; pointer fields require explicit `borrowed`, `runtime_owned`, or
`external` ownership instead of the scalar `value` policy. Optional capacities provide the existing buffer-bound
metadata hook.

## Verification

`tools/generate_dsp_type_family.pl` validates the schema and emits a readable candidate header with a generated-file
banner. `test_dsp_type_schema_generation` runs the generator twice, requires byte-identical outputs, verifies the
banner, strips it, and compares the remaining body byte for byte with the handwritten `src_types.h`.

The test belongs to the `types` and `v2` labels. A schema change that alters layout must therefore update the
handwritten candidate target deliberately and then pass the independent DSP ABI snapshot. Algorithm sources are never
generated.

Local equivalence check:

```sh
perl tools/generate_dsp_type_family.pl schema/atoms/src.json /tmp/src_types.candidate.h
ctest --test-dir build -R '^test_dsp_type_schema_generation$' --output-on-failure
```

## Migration Rules

1. Keep the C family header authoritative until its schema reproduces the header, canonical atom rows, field
   descriptors, and catalog contract.
2. Add one family at a time and compare generated artifacts in the build tree.
3. Add explicit schema support for any new C field shape, ownership policy, or capacity before migrating that family.
4. Generate declaration rows, registry metadata, TypeScript catalog data, and docs only after cross-output comparison
   tests exist.
5. Mark production outputs with a generated banner only when schema ownership is activated.
6. Make CI fail on dirty regeneration before deleting any handwritten source.
7. Keep DSP algorithms and custom processing logic handwritten.

The next candidate should cover a pointer-owning state family so ownership/capacity validation is exercised before
schema expansion across all categories.

