# Canonical Atom Definitions

`inc/atom/atom_definitions.h` is the authoritative atom inventory. Each `APG_ATOM_DEFINITIONS` row defines:

```text
name, category, input descriptor count, config descriptor count,
state descriptor count, capability flags, maturity, dispatch kind
```

The list generates the public atom declarations, thunk declarations and implementations, registry rows, field-table
declarations, capability/profile data, and the registry metadata consumed by the JSON/UI catalog. `dsp_types.h` remains
the C ABI definition for the structures named by each row. Its compact type table has one row per atom with reusable output
and input layout profiles followed by parameter and state field bodies. Each row still generates distinct atom-specific
structures. Family `*_field_descriptors.c` files remain the definitions of member offsets and buffer defaults.

`APG_ATOM_CONTRACT_DEFINITIONS` maps contract-bearing atoms to reusable input, output, and configuration profiles. The
compiler validator and JSON/UI catalog consume the generated contract table from `atom_catalog.c`.

To add an atom:

1. Add one ABI row to the type table in `dsp_types.h`, reusing an exact I/O layout profile or defining one when the member
   layout is new, and preserve the `out`, `in`, `params`, and `state` column order.
2. Define any input, config, or state field arrays in the family descriptor source.
3. Add one `APG_ATOM_DEFINITIONS` row with matching descriptor counts and the appropriate dispatch kind.
4. Add a contract-profile row only when the YAML compiler exposes a structured binding contract.

Compilation fails when a canonical descriptor count has no matching fixed-size declaration. The atom registry contract test
also verifies every generated row and rejects duplicate or non-canonical contract names.
