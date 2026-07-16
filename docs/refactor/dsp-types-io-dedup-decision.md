# DSP I/O Deduplication Decision

## Decision

Phase 4 does not alias atom-specific input/output typedefs to the generic `atom_mono_t`, `atom_pair_t`,
`atom_stereo_t`, `atom_complex_t`, `atom_ms_t`, `atom_wet_dry_t`, or `atom_div_t` structures.

The category headers already select shared `APG_IO_FIELDS_*` profiles from `dsp_type_macros.h`. Each field layout is
therefore maintained once while `APG_DECLARE_DSP_TYPES` continues to emit a distinct anonymous structure for every
public atom type. Aliasing those structures would remove generated type declarations, not duplicated source-of-truth
field definitions.

## Audit

- The generic `atom_*_t` port types have no production consumers. They are retained public compatibility names and are
  covered by the ABI snapshot.
- No structure tags or forward declarations refer to atom input/output layouts.
- No `_Generic`, compiler type-compatibility builtin, C++ type dispatch, or exact-type macro dispatch exists in the C
  implementation.
- Existing source uses atom-specific typedef names, member access, designated initializers, `sizeof`, and `offsetof`.
- Snapshot and binding tooling operates on the atom-specific names and family profile tables.
- Specialized profiles keep their semantic member names and are not candidates for layout-only aliasing.

Despite the absence of a current exact-type dispatcher, aliasing would make types such as `amplitude_add_out_t` and
`generation_dc_out_t` C-compatible where they are currently distinct. That broadens assignment and pointer
compatibility, weakens debugger type identity, and may affect downstream FFI generators. It is a public type-system
change even when size, alignment, and offsets are identical.

## Revisit Criteria

Reconsider aliases only as part of an explicitly versioned public API change with downstream FFI coverage and a clear
consumer benefit. Until then, shared field-profile macros provide the maintenance benefit while preserving existing
type separation and ABI records.

