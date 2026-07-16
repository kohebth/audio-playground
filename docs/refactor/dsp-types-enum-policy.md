# DSP Enum Compatibility Policy

## Public Names

The shared DSP enums now have namespaced tags and typedefs:

| Domain | Enum tag | Preferred typedef | Compatibility typedef |
|---|---|---|---|
| waveform/noise color | `enum apg_waveform_type` | `apg_waveform_type_t` | `WaveformType` |
| normalization | `enum apg_normalize_mode` | `apg_normalize_mode_t` | `NormalizeMode` |
| interpolation | `enum apg_interpolation_type` | `apg_interpolation_type_t` | `InterpolationType` |
| window | `enum apg_window_type` | `apg_window_type_t` | `WindowType` |

All 17 legacy enumerator names remain available. New code should use the namespaced typedefs; existing source can keep the
legacy typedefs without source or binary layout changes.

## Numeric Contract

- waveform values remain 0 through 6 from `WAVEFORM_SINE` to `WAVEFORM_NOISE_BROWN`;
- normalization values remain 0 and 1;
- interpolation values remain 0 through 3;
- window values remain 0 through 3.

Every enumerator now has an explicit assignment. Standalone C and C++ header tests assert representative type aliases
and all numeric values, while the ABI snapshot records all 17 values.

## Metadata And Persistence

Atom parameter structures continue to store waveform, normalization, interpolation, and window selections as `int`.
Field descriptors and the atom catalog continue to expose them as integer fields. V2 YAML therefore carries numeric
scalar values, such as `waveform: 0`; the parser/runtime path converts those values to `int` and does not serialize a C
enum object or depend on compiler enum width.

Changing enum tags and typedefs does not change any parameter size, field offset, catalog shape, or YAML mapping.
Renaming or namespacing the legacy enumerators themselves would be a separate public source-compatibility change and is
not part of this refactor.
