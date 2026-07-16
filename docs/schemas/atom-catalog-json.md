# Atom Catalog JSON

`apg.atom_catalog.v2` is a deterministic UI-facing export of the registered atom table. Atom layouts, binding
contracts, visibility, and parameter policy come from `schema/atoms/atoms.json`; runtime sizes and target profiles come
from the C registry.

## Shape

```json
{
  "schema": "apg.atom_catalog.v2",
  "atoms": [
    {
      "name": "generation_dc",
      "category": "generation",
      "visibility": "public",
      "dispatch": "process",
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
        {
          "name": "value",
          "type": "float",
          "required": true,
          "default": 0,
          "min": -4,
          "max": 4,
          "unit": "ratio",
          "realtime": true,
          "structural": false
        }
      ],
      "state": []
    }
  ]
}
```

## Field Types

Input/output binding field types:

- `signal`: Audio/control-rate signal buffer binding.
- `signal_optional`: Optional signal buffer binding.
- `signal_array`: Array of signal buffer bindings.
- `buffer`: Stateful buffer input.
- `scalar`: Scalar-valued process binding.
- `float_matrix`: Matrix literal used by atoms such as `mix_matrix`.

Config presentation types:

- `float`, `int`, `bool`: scalar controls;
- `enum`: an integer ABI field with generated `options` labels and `option_values` ordinals;
- `buffer`: a named buffer binding;
- `float_matrix`: a matrix-valued structural field.

Every config field includes `required`, `default`, `realtime`, and `structural`. Numeric fields may include `min`,
`max`, `unit`, and `scale`; smoothed controls may include `smoothing_ms`. Structural fields require recompilation or
preflight and are never marked real-time.

## Visibility

- `public`: shown in the editor palette by default;
- `advanced`: shown only when the advanced control is enabled;
- `internal`: loadable and inspectable for compatibility, but never offered for creation.

Registry state/config field types:

- `float`, `int`, `signal`, `buffer`, `float_ptr`, `float_pp`.
- `buffer` state fields may include `buffer_samples`.

## Limits

All registered atoms have generated input, output, and config contracts. Empty arrays represent an intentionally empty
logical role, not missing metadata.

Profile flags are compatibility hints for UI filtering. They are conservative metadata, not a replacement for full unit/project validation.

An atom role with no semantic fields still has a standard one-byte C layout containing `_reserved`; `sizes` reports
that physical byte. Reserved storage does not create a catalog field and does not make an atom logically stateful.
