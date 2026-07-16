# Atom Catalog JSON

`apg.atom_catalog.v2` is a UI-facing metadata export for the registered atom table. It is deterministic and generated from the C registry plus the v2 compiler binding contracts where those contracts exist.

## Shape

```json
{
  "schema": "apg.atom_catalog.v2",
  "atoms": [
    {
      "name": "generation_dc",
      "category": "generation",
      "sizes": {
        "out": 8,
        "in": 1,
        "config": 4,
        "state": 1
      },
      "stateful": false,
      "profiles": {
        "desktop_full": true,
        "wasm_realtime": true,
        "m7_static": true,
        "offline_render": true
      },
      "inputs": [],
      "outputs": [
        { "name": "signal", "type": "signal" }
      ],
      "config": [
        { "name": "value", "type": "scalar" }
      ],
      "state": []
    }
  ]
}
```

## Field Types

Binding contract field types:

- `signal`: Audio/control-rate signal buffer binding.
- `signal_optional`: Optional signal buffer binding.
- `signal_array`: Array of signal buffer bindings.
- `buffer`: Stateful buffer input.
- `scalar`: Literal or `${params.name}` scalar binding.
- `float`, `int`: Literal/config scalar type.
- `float_matrix`: Matrix literal used by atoms such as `mix_matrix`.

Registry state/config field types:

- `float`, `int`, `signal`, `buffer`, `float_ptr`, `float_pp`.
- `buffer` state fields may include `buffer_samples`.

## Limits

Input/output contracts are populated for atoms covered by the v2 compiler binding metadata. Atoms outside that contract set still appear in the catalog with registry size/config/state metadata, but may have empty `inputs` and `outputs` until their v2 binding contract is added.

Profile flags are compatibility hints for UI filtering. They are conservative metadata, not a replacement for full unit/project validation.

An atom role with no semantic fields still has a standard one-byte C layout containing `_reserved`; `sizes` reports
that physical byte. Reserved storage does not create a catalog field and does not make an atom logically stateful.
