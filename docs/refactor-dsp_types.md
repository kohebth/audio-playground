# DSP Type System Refactor Plan

## 1. Mission

Refactor `inc/atom/dsp_types.h` into a maintainable, portable, and verifiable DSP type system without changing DSP behavior.

The work must:

- preserve all existing atom type names;
- preserve all existing public includes during migration;
- preserve struct layouts unless a change is explicitly isolated and tested;
- avoid changing DSP algorithms in the same refactor;
- keep the legacy atom API and the v2 `*_process(..., apg_process_info_t*)` API compiling;
- leave the repository in a passing state after every phase;
- minimize changes outside headers and build/test files.

Repository baseline:

- Repository: `kohebth/audio-playground`
- Branch: `main`
- Baseline commit: `a813b26f59bfeca9b1dae854979aa424c4d93e2b`
- Baseline commit message: `feat: add host project live swap`

The local agent must begin by confirming that its checkout is at this commit or a descendant. It must not silently work against an older tree.

---

## 2. Current problem

`inc/atom/dsp_types.h` currently combines all of the following:

1. Primitive signal aliases.
2. Generic input/output structures.
3. Shared enums.
4. Input types for every atom.
5. Output types for every atom.
6. Parameter types for every atom.
7. Persistent state types for every atom.
8. Pointer ownership-sensitive buffers and lookup tables.
9. Compiler-extension empty structures.
10. Types used by both the legacy atom ABI and the v2 process ABI.

This creates several risks:

- any atom type change recompiles nearly the entire DSP implementation;
- category ownership is unclear;
- repeated mono/pair/stereo structures drift;
- generic enum names pollute the global namespace;
- empty C structures are compiler-dependent;
- pointer mutability and ownership are not documented;
- the C atom catalog, runtime registry, and TypeScript catalog may diverge;
- future embedded, WASM, Windows, and strict-C builds become harder.

The immediate goal is structural separation, not redesign.

---

## 3. Non-goals

Do not perform these changes during the initial refactor:

- Do not change any DSP equation.
- Do not change parameter meanings, defaults, ranges, or units.
- Do not replace raw buffers with containers.
- Do not add heap allocation to process functions.
- Do not remove legacy atom entry points.
- Do not remove `inc/atom/dsp_types.h`.
- Do not move atom implementation source files.
- Do not regenerate the full atom registry.
- Do not rename atom functions.
- Do not change YAML schemas.
- Do not replace `float` samples with another format.
- Do not change block-size handling.
- Do not introduce C++-only types.
- Do not combine this work with parameter smoothing or real-time queue changes.
- Do not fix unrelated warnings unless they block this refactor.

Record unrelated defects in a follow-up section rather than expanding scope.

---

## 4. Required invariants

The following must remain true through Phases 1–5:

### Source compatibility

Existing code must continue to compile with:

```c
#include <atom/dsp_atoms.h>
```

and, where currently used:

```c
#include "dsp_types.h"
```

Every existing public atom type name must remain available:

```c
<atom>_in_t
<atom>_out_t
<atom>_params_t
<atom>_state_t
```

### ABI/layout compatibility

For every existing non-empty public structure:

- `sizeof(type)` must remain unchanged;
- `_Alignof(type)` must remain unchanged;
- offsets of all fields must remain unchanged;
- field types must remain unchanged;
- field order must remain unchanged.

Empty structures are treated separately because they are a compiler extension and may have toolchain-dependent size.

### Runtime compatibility

The following must remain behaviorally unchanged:

- registry lookup;
- state allocation;
- parameter allocation;
- graph compilation;
- runtime processing;
- host project live swap;
- CLI loading and rendering;
- existing unit tests.

### Portability

All newly introduced headers must:

- be valid C11;
- support inclusion from C++;
- have independent include guards;
- include their own dependencies;
- not depend on include order;
- not define storage;
- not allocate memory;
- not include platform audio APIs;
- not introduce circular includes.

---

## 5. Target structure

Use the existing `inc/atom` public path for the first migration to minimize include churn.

```text
inc/
└── atom/
    ├── dsp_atoms.h
    ├── dsp_types.h                 # compatibility umbrella
    └── types/
        ├── dsp_primitives.h
        ├── dsp_enums.h
        ├── dsp_ports.h
        ├── dsp_type_macros.h       # only when useful; keep small
        ├── generation_types.h
        ├── amplitude_types.h
        ├── delay_types.h
        ├── filter_types.h
        ├── detect_types.h
        ├── modulation_types.h
        ├── interpolation_types.h
        ├── src_types.h
        ├── frequency_types.h
        ├── mix_types.h
        └── nonlinear_types.h
```

Do not move these immediately to `inc/apgcore/...`; that would create unnecessary include-path churn. A later public-API migration may introduce an `apgcore` path after the split is stable.

### Compatibility umbrella

`inc/atom/dsp_types.h` must become an ordered include-only umbrella:

```c
#ifndef AUDIO_PLAYGROUND_DSP_TYPES_H
#define AUDIO_PLAYGROUND_DSP_TYPES_H

#include <atom/types/dsp_primitives.h>
#include <atom/types/dsp_enums.h>
#include <atom/types/dsp_ports.h>

#include <atom/types/generation_types.h>
#include <atom/types/amplitude_types.h>
#include <atom/types/delay_types.h>
#include <atom/types/filter_types.h>
#include <atom/types/detect_types.h>
#include <atom/types/modulation_types.h>
#include <atom/types/interpolation_types.h>
#include <atom/types/src_types.h>
#include <atom/types/frequency_types.h>
#include <atom/types/mix_types.h>
#include <atom/types/nonlinear_types.h>

#endif
```

It must contain no atom struct definitions after the migration.

---

## 6. Type ownership rules

### `dsp_primitives.h`

Own only fundamental DSP aliases and fixed-width utility types.

Initial contents should preserve current aliases:

```c
typedef float *Signal;
typedef float *Spectrum;
typedef float *Buffer;
```

Optionally introduce namespaced aliases without removing the legacy aliases:

```c
typedef float apg_sample_t;
typedef apg_sample_t *apg_signal_t;
typedef apg_sample_t *apg_spectrum_t;
typedef apg_sample_t *apg_buffer_t;

typedef apg_signal_t Signal;
typedef apg_spectrum_t Spectrum;
typedef apg_buffer_t Buffer;
```

Before adding these aliases, search for places where `Signal`, `Spectrum`, or `Buffer` are expected to be exactly `float *`. Preserve that property.

Do not add buffer lengths here. A pointer-plus-length representation is a later API change.

### `dsp_enums.h`

Own shared enum definitions:

- waveform;
- normalize mode;
- interpolation type;
- window type;
- any additional shared enum currently found in the lower half of `dsp_types.h`.

Phase 1 should preserve existing enumerator values exactly.

Preferred long-term naming:

```c
typedef enum apg_waveform_type {
    WAVEFORM_SINE = 0,
    ...
} apg_waveform_type_t;

typedef apg_waveform_type_t WaveformType;
```

However, do not force enum renaming in the initial split if it creates broad changes. Moving the definitions unchanged is acceptable for Phase 1.

Add compile-time assertions for numeric values where those values are serialized, loaded from YAML, or passed through registries.

### `dsp_ports.h`

Own only genuinely shared IO shapes, such as:

```c
typedef struct { float *signal; } atom_mono_t;
typedef struct { float *signal_a; float *signal_b; } atom_pair_t;
typedef struct { float *left; float *right; } atom_stereo_t;
typedef struct { float *real; float *imag; } atom_complex_t;
typedef struct { float *mid; float *side; } atom_ms_t;
typedef struct { float *dry; float *wet; } atom_wet_dry_t;
typedef struct { float *numerator; float *denominator; } atom_div_t;
```

Do not alias atom-specific types to these shared types in Phase 1. First move the exact existing atom structures by category. Aliasing repeated layouts belongs to Phase 4 because it can affect type compatibility, debugging output, and strict aliasing assumptions.

### Category headers

Each category header owns all four type groups for atoms in that category:

```text
<atom>_out_t
<atom>_in_t
<atom>_params_t
<atom>_state_t
```

Keep each atom’s four types adjacent and in the existing order.

Each header must include only what it directly needs, typically:

```c
#include <stdint.h>
#include <atom/types/dsp_primitives.h>
#include <atom/types/dsp_enums.h>
#include <atom/types/dsp_ports.h>
```

Avoid including another category header unless there is a real cross-category type dependency.

---

## 7. Category mapping

Use the current atom declaration list and catalog as the authoritative category map.

### Generation

- `generation_oscillator`
- `generation_noise`
- `generation_envelope`
- `generation_lfo`
- `generation_impulse`
- `generation_dc`

### Amplitude

- `amplitude_multiply`
- `amplitude_divide`
- `amplitude_smooth`
- `amplitude_clip_hard`
- `amplitude_clip_soft`
- `amplitude_normalize`
- `amplitude_add`
- `amplitude_subtract`
- `amplitude_accumulate`
- `amplitude_latch`

### Delay

- `delay_unit`
- `delay_line`
- `delay_fractional`
- `delay_tap_feedback`
- `delay_tap_feedforward`

### Filter

- `filter_fir`
- `filter_biquad_coefficients`
- `filter_biquad`
- `filter_dc_block`
- `filter_comb_ff`
- `filter_comb_fb`
- `filter_allpass`
- `filter_integrate`
- `filter_differentiate`

### Detect

- `detect_peak`
- `detect_envelope`
- `detect_threshold`
- `detect_rms`
- `detect_zero_crossing`
- `detect_slope`
- `detect_autocorrelate`
- `detect_pitch`

### Modulation

- `modulation_phase`
- `modulation_ring`
- `modulation_amplitude`
- `modulation_frequency`
- `modulation_scrub`

### Interpolation

- `interpolation_linear`
- `interpolation_cubic`
- `interpolation_sinc`
- `interpolation_lagrange`

### Sample-rate conversion

- `src_upsample`
- `src_downsample`
- `src_antialias`
- `src_antiimage`
- `src_convert_format`

### Frequency domain

- `freq_fft`
- `freq_ifft`
- `freq_window`
- `freq_multiply`
- `freq_overlap_add`
- `freq_overlap_save`
- `freq_shift`
- `freq_quantize`

### Mix

- `mix_crossfade`
- `mix_wet_dry`
- `mix_matrix`
- `mix_pan_stereo`
- `mix_encode_ms`
- `mix_decode_ms`

### Nonlinear

- `nonlinear_waveshape`
- `nonlinear_bitcrush`
- `nonlinear_sample_hold`

Verify this map against the complete current `dsp_types.h`. Do not assume every lower-file definition is represented in this list; report any orphan or missing atom types.

---

## 8. Implementation phases

# Phase 0 — Baseline and inventory

## Tasks

1. Fetch/pull `main`.
2. Record:
   - commit SHA;
   - compiler and CMake versions;
   - active build options;
   - test list;
   - generated files, if any.
3. Create a clean build directory.
4. Run the complete current build and test suite.
5. Save baseline output.
6. Generate an inventory of every typedef and enum in `dsp_types.h`.
7. Search for:
   - direct includes of `dsp_types.h`;
   - use of `Signal`, `Spectrum`, and `Buffer`;
   - use of generic IO structs;
   - `sizeof(*_params_t)` and `sizeof(*_state_t)`;
   - `_Alignof`;
   - `offsetof`;
   - serialization or raw copying of parameter/state structs;
   - zero initialization with `memset`;
   - compound literals;
   - designated initializers;
   - casts between atom IO structures;
   - forward declarations of atom types.
8. Generate a machine-readable ABI snapshot.

Suggested ABI snapshot program:

```c
printf("type,size,align\n");
printf("filter_biquad_state_t,%zu,%zu\n",
       sizeof(filter_biquad_state_t),
       _Alignof(filter_biquad_state_t));
```

For every non-empty structure, also record field offsets.

## Deliverables

- `docs/refactor/dsp-types-inventory.md`
- `test/abi/dsp_types_abi_snapshot.c`
- baseline ABI output file
- baseline test result
- list of discovered risks

## Gate

Do not modify production headers until the baseline build and tests pass.

If the baseline is already failing, document exact failures and distinguish them from refactor regressions.

---

# Phase 1 — Mechanical header split

## Tasks

1. Create `inc/atom/types/`.
2. Create independent guarded headers.
3. Copy primitive aliases into `dsp_primitives.h`.
4. Copy shared enums into `dsp_enums.h`.
5. Copy shared generic port structs into `dsp_ports.h`.
6. Move atom type definitions into category headers without changing:
   - spelling;
   - field order;
   - field types;
   - comments that explain semantics;
   - enum values.
7. Replace `dsp_types.h` contents with umbrella includes.
8. Keep `dsp_atoms.h` include behavior unchanged.
9. Build after each category migration or in small category groups.
10. Run ABI comparison after each group.

Recommended migration order:

1. generation;
2. amplitude;
3. delay;
4. filter;
5. detect;
6. mix;
7. modulation;
8. nonlinear;
9. interpolation;
10. SRC;
11. frequency.

Move frequency last because its structures are more likely to contain non-trivial arrays, frame sizes, or complex buffers.

## Rules

- No atom source modifications unless needed to fix an include dependency.
- No field renames.
- No typedef alias consolidation.
- No empty-struct change.
- No `const` additions.
- No formatting of unrelated files.
- No changes to catalog metadata.
- No API deprecation.

## Gate

Pass all of:

- clean configure;
- clean build;
- complete tests;
- ABI snapshot equality for all non-empty types;
- successful compile of each category header in isolation;
- successful C++ include smoke test;
- no circular includes;
- `dsp_types.h` contains only includes and guards.

Commit suggestion:

```text
refactor(types): split dsp_types into category headers
```

---

# Phase 2 — Header hygiene and standalone compilation

## Tasks

1. Add a test translation unit for each header:

```c
#include <atom/types/filter_types.h>
int main(void) { return 0; }
```

2. Add a C++ smoke test:

```cpp
extern "C" {
#include <atom/dsp_atoms.h>
}
int main() { return 0; }
```

3. Ensure every header directly includes:
   - `<stdint.h>` when using fixed-width integers;
   - `<stddef.h>` when using `size_t`;
   - shared enum or primitive headers when needed.
4. Remove accidental transitive include dependencies.
5. Verify include order permutations for representative headers.
6. Run strict compiler modes where available:
   - GCC/Clang C11;
   - `-Wall -Wextra -Wpedantic`;
   - optionally `-Werror` only for the new header tests;
   - MSVC-compatible C compilation if CI supports it.
7. Add `extern "C"` handling only at function declaration headers, not around plain C struct headers unless the project convention requires it.

## Gate

- All category headers compile standalone.
- Umbrella header compiles standalone.
- C++ smoke test passes.
- No new warnings in production targets.
- ABI snapshot unchanged.

Commit suggestion:

```text
test(types): add standalone header and ABI checks
```

---

# Phase 3 — Empty-structure portability

Current zero-member structs are not portable standard C. Handle them as a separate, explicit compatibility decision.

## Discovery tasks

1. List every empty `*_in_t`, `*_out_t`, `*_params_t`, and `*_state_t`.
2. Record their current size and alignment under each supported compiler.
3. Find all allocations, arrays, pointer arithmetic, serialization, and registry sizing involving those types.
4. Determine whether zero-size behavior is relied upon.

## Preferred representation

Use a common placeholder:

```c
typedef struct {
    uint8_t _reserved;
} apg_empty_t;
```

Then either:

```c
typedef apg_empty_t amplitude_add_params_t;
```

or preserve a distinct named structure:

```c
typedef struct {
    uint8_t _reserved;
} amplitude_add_params_t;
```

Distinct structures preserve better type separation. Aliases reduce repetition. Choose based on current registry and debugger needs.

## Compatibility strategy

Because size may change from a compiler extension’s zero bytes to one byte:

- do not claim ABI preservation for empty types;
- update allocation-size tests deliberately;
- verify no persisted binary format contains these structures;
- verify registry state/param offsets remain valid;
- ensure at least one byte is allocated only where an address is required;
- consider representing absent params/state as `NULL` in a later redesign, but not in this phase.

## Gate

- strict C11 build passes without empty-structure extensions;
- all registry and runtime tests pass;
- state and parameter arena calculations pass sanitizer checks;
- no binary serialization regression;
- documented ABI exception exists.

Commit suggestion:

```text
fix(types): replace non-standard empty structs
```

---

# Phase 4 — Safe deduplication of IO shapes

Only begin after Phases 1–3 are stable.

## Objective

Reduce repeated structures while preserving atom-specific public type names.

Example:

```c
typedef atom_mono_t amplitude_clip_hard_in_t;
typedef atom_mono_t amplitude_clip_hard_out_t;
typedef atom_pair_t amplitude_add_in_t;
```

## Risks to inspect

- C type compatibility changes;
- code using `struct` tags rather than typedef names;
- debugger clarity;
- distinct-type expectations;
- strict aliasing;
- `_Generic` dispatch;
- macros matching exact type names;
- generated bindings or FFI tooling;
- designated initializer compatibility.

## Procedure

1. Start with output-only mono types.
2. Build and run ABI checks.
3. Continue with simple pair/stereo/complex types.
4. Keep specialized IO structures distinct.
5. Never alias structures whose field names differ, even if their layouts match.
6. Do not alias states or params solely because they happen to have the same layout.
7. Preserve atom-specific typedef names.

## Decision rule

Skip this phase entirely if it creates more conceptual complexity than it removes. The header split provides most of the maintainability benefit without alias deduplication.

## Gate

- all builds/tests pass;
- no public name removed;
- ABI size/alignment/offsets remain equivalent;
- binding or registry generation remains correct.

Commit suggestion:

```text
refactor(types): reuse common atom port layouts
```

---

# Phase 5 — Enum namespacing and fixed underlying semantics

## Objective

Reduce namespace pollution without breaking existing code or serialized numeric values.

## Tasks

1. Add explicit numeric values to all shared enum members.
2. Introduce namespaced enum typedefs.
3. Keep compatibility typedefs for old enum type names.
4. Keep existing enumerator names initially.
5. Add tests asserting values.
6. Audit YAML parsing and catalog mappings.
7. Do not assume C enum storage width in persisted data.
8. Where structs store enum-like values as `int`, preserve the `int` fields during this refactor.

Example:

```c
typedef enum apg_window_type {
    WINDOW_HANN = 0,
    WINDOW_HAMMING = 1,
    WINDOW_BLACKMAN = 2,
    WINDOW_RECTANGULAR = 3
} apg_window_type_t;

typedef apg_window_type_t WindowType;
```

## Gate

- all value assertions pass;
- existing units load identically;
- no parameter struct layout changes;
- no warnings from enum/int conversions added.

Commit suggestion:

```text
refactor(types): namespace shared dsp enums
```

---

# Phase 6 — Atom declaration cleanup

This phase touches `dsp_atoms.h` but must not remove compatibility.

The current design contains:

- legacy functions declared through `DECLARE_ALL(ATOM)`;
- explicit v2 `*_process` declarations with `apg_process_info_t`.

## Objective

Create one declaration mechanism while keeping both APIs available.

## Tasks

1. Introduce declaration macros:

```c
#define APG_DECLARE_LEGACY_ATOM(name) \
    void name(name##_out_t *, name##_in_t *, name##_params_t *, name##_state_t *)

#define APG_DECLARE_PROCESS_ATOM(name) \
    void name##_process( \
        name##_out_t *, \
        name##_in_t *, \
        name##_params_t *, \
        name##_state_t *, \
        const apg_process_info_t *)
```

2. Create one authoritative atom list per category or one X-macro catalog.
3. Generate declarations from that list.
4. Compare generated declarations against the current set.
5. Preserve symbol names.
6. Do not add `const` to existing parameter types in this phase because it changes function types.
7. Add a link test that references every declared atom symbol.

## Gate

- symbol list before and after is equal;
- no missing or duplicate declaration;
- legacy and v2 runtime tests pass;
- registry compile-time mappings pass.

Commit suggestion:

```text
refactor(atom): centralize atom process declarations
```

---

# Phase 7 — Source-of-truth generation design

Do not immediately generate production headers. First design and validate the schema.

## Problem

The following currently overlap:

- C type definitions;
- C atom declaration list;
- runtime registry metadata;
- TypeScript atom catalog;
- YAML parameters and ports.

Manual duplication can drift.

## Proposed source

Create a machine-readable atom schema, for example:

```text
schema/atoms/
├── generation.yaml
├── amplitude.yaml
├── delay.yaml
...
```

Example:

```yaml
name: amplitude_clip_soft
category: amplitude
inputs:
  - name: signal
    type: signal
outputs:
  - name: signal
    type: signal
params:
  - name: threshold
    c_type: float
    ui_type: float
  - name: curve
    c_type: int
    ui_type: enum
state: []
```

## Generated outputs

Potentially generate:

- atom type headers;
- declaration X-macros;
- registry metadata;
- TypeScript catalog entries;
- schema documentation;
- ABI inventory tests.

## Generation rules

- generated files must have a clear banner;
- generation must be deterministic;
- CI must fail on dirty regeneration;
- custom state structures and pointer ownership must be supported;
- generated code must remain readable;
- handwritten algorithm source remains handwritten;
- schema changes must visibly show ABI impact.

## Migration approach

1. Parse current C and TypeScript definitions into a comparison report.
2. Build schema for one low-risk category.
3. Generate into a temporary directory.
4. Diff generated output against handwritten definitions.
5. Expand only after exact equivalence.
6. Do not delete handwritten definitions until all categories match and tests pass.

## Gate

- deterministic regeneration;
- no catalog drift;
- generated ABI matches baseline;
- local developer workflow documented.

---

## 9. ABI verification design

Add durable verification rather than relying only on compilation.

### Snapshot contents

For each public enum:

- numeric value of each enumerator.

For each public struct:

- size;
- alignment;
- each field offset;
- optionally each field size.

For each function:

- symbol existence;
- expected declaration compile-check.

### Suggested files

```text
test/abi/
├── dsp_types_abi_snapshot.c
├── dsp_types_expected_<platform>.txt
├── dsp_headers_c_smoke.c
├── dsp_headers_cpp_smoke.cpp
└── dsp_atom_symbols.c
```

Avoid platform-specific expected files unless alignment genuinely differs. Prefer assertions derived from the known baseline for supported ABIs.

### Static assertions

Use `_Static_assert` for high-value stable structures:

```c
_Static_assert(
    offsetof(filter_biquad_state_t, z1) == 0,
    "filter_biquad_state_t.z1 ABI changed"
);
```

Do not hard-code pointer-containing structure sizes across 32-bit and 64-bit targets without platform conditions.

---

## 10. Testing matrix

### Required on every phase

- clean CMake configure;
- clean build;
- unit tests;
- registry v2 tests;
- unit runtime v2 tests;
- CLI build;
- runtime build;
- ABI snapshot comparison;
- header standalone tests.

### Recommended sanitizers

For host Linux builds:

- AddressSanitizer;
- UndefinedBehaviorSanitizer;
- LeakSanitizer where appropriate.

The refactor should not alter runtime memory behavior, but state/params arena mistakes caused by empty-struct changes may be detected here.

### Platform matrix

At minimum:

| Target | Purpose |
|---|---|
| GCC Linux C11 | Primary current host |
| Clang Linux C11 | Header and warning portability |
| C++ include test | Native wrappers and bindings |
| Emscripten compile smoke | WASM compatibility |
| ARM cross-compile smoke | Cortex/ARM header compatibility |
| Windows compiler smoke | Empty-struct and C portability |

Full runtime execution is not required on every cross target for this refactor; successful type/header compilation is sufficient where execution infrastructure is unavailable.

---

## 11. Build-system changes

The local agent must inspect current CMake structure before editing.

Likely required changes:

1. Add new headers to installation/export lists if headers are explicitly enumerated.
2. Add standalone header tests.
3. Add ABI test target.
4. Ensure include root exposes `<atom/types/...>`.
5. Add generated schema targets only in Phase 7.
6. Avoid globbing source files unless already used by project convention.

Do not create separate libraries for header categories. They are public type headers, not runtime components.

---

## 12. Documentation changes

Create:

```text
docs/architecture/dsp-type-system.md
```

It should document:

- purpose of each header;
- ownership rules;
- distinction among input, output, params, and state;
- real-time ownership expectations;
- pointer lifetime expectations;
- rules for adding a new atom;
- ABI compatibility policy;
- rules for enums;
- empty type policy;
- generation roadmap.

Add a concise contributor checklist:

```text
[ ] Atom placed in correct category header
[ ] Input/output fields represent process-call buffer bindings
[ ] Params contain configuration/control values only
[ ] State contains persistent runtime state only
[ ] No process-time allocation introduced
[ ] Header compiles standalone
[ ] ABI test updated intentionally
[ ] C and TypeScript catalogs remain synchronized
```

---

## 13. Local agent operating instructions

The implementing agent should follow these rules:

1. Work on a dedicated branch:
   `refactor/dsp-types`.
2. Do not make a single giant commit.
3. Execute tests after each category or small logical group.
4. Preserve exact public names.
5. Prefer mechanical moves before cleanup.
6. Use `git diff --word-diff` to detect accidental field edits.
7. Use scripts to compare typedef inventories before and after.
8. Never infer a missing field from the web catalog; C usage is authoritative for current ABI.
9. Report catalog/C mismatches rather than silently selecting one.
10. Do not reformat the full 1,000-line file before splitting it; this destroys reviewability.
11. Keep moved definitions in their current order.
12. Do not modify algorithms to satisfy type cleanup.
13. Stop and report if:
    - a struct is serialized raw;
    - external consumers rely on exact struct tags;
    - empty structs participate in pointer arithmetic;
    - a category cannot compile independently due to a circular type dependency;
    - the baseline tests fail;
    - an ABI mismatch occurs unexpectedly.
14. Continue autonomously through mechanical fixes that preserve the documented invariants.
15. At completion, provide:
    - changed file list;
    - type inventory comparison;
    - ABI comparison;
    - test results;
    - known follow-up issues;
    - commit list.

---

## 14. Suggested commits

```text
test(types): capture dsp type baseline ABI
refactor(types): add dsp primitive enum and port headers
refactor(types): split generation and amplitude types
refactor(types): split delay filter and detect types
refactor(types): split modulation mix and nonlinear types
refactor(types): split interpolation src and frequency types
refactor(types): convert dsp_types to compatibility umbrella
test(types): add standalone C and C++ header checks
fix(types): replace non-standard empty structs
refactor(types): reuse common atom port layouts
refactor(types): namespace shared dsp enums
refactor(atom): centralize atom declarations
docs(types): document dsp type ownership and ABI rules
```

The agent may combine adjacent category commits if each combined commit remains reviewable and passes all tests.

---

## 15. Acceptance criteria

The refactor is complete when all mandatory criteria pass.

### Structure

- `dsp_types.h` is an umbrella only.
- Atom types are grouped by category.
- Shared primitives, enums, and port shapes have dedicated headers.
- Every header is independently guarded and self-contained.

### Compatibility

- Existing includes compile unchanged.
- Every existing public atom type name still exists.
- Every existing atom symbol still exists.
- Non-empty public struct ABI is unchanged through the mechanical split.
- Empty-struct ABI changes are isolated and documented.

### Quality

- No circular includes.
- No include-order dependency.
- No duplicate atom definitions.
- No orphan atom types.
- No algorithm changes.
- No new production warnings.
- C and C++ header smoke tests pass.

### Runtime

- registry tests pass;
- unit v2 runtime tests pass;
- CLI builds and its smoke tests pass;
- live-swap related tests pass;
- sanitizer build passes for relevant host tests.

### Documentation

- type-system architecture documented;
- new-atom checklist documented;
- ABI policy documented;
- follow-up generation design recorded.

---

## 16. Rollback strategy

Each phase must be independently revertible.

If a phase fails:

1. Revert only that phase’s commit(s).
2. Keep baseline and ABI tests.
3. Preserve completed earlier mechanical splits if they pass.
4. Do not patch runtime algorithms to hide type regressions.
5. Record the failing type and dependency path.

The compatibility umbrella makes rollback low-risk because external include paths remain stable.

---

## 17. Expected final state

```text
inc/atom/dsp_types.h
    └── stable compatibility umbrella

inc/atom/types/*.h
    ├── narrowly owned
    ├── independently compilable
    ├── C11 portable
    └── ABI tested

inc/atom/dsp_atoms.h
    ├── includes the umbrella
    ├── retains legacy declarations
    └── retains v2 process declarations

test/abi/
    ├── detects accidental layout changes
    ├── verifies enum values
    └── verifies public headers and symbols
```

The result should make adding, reviewing, generating, and porting atoms easier without forcing a disruptive rewrite of current DSP implementations.
