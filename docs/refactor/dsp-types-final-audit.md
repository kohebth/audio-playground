# DSP Type Refactor Final Audit

> This living audit records the type-header split completed at `2f84b17` and the subsequent execution-context,
> state-capacity, and production-generation convergence. Historical phase-1 evidence remains frozen separately.

## Inventory Comparison

| Surface | Baseline | Final | Result |
|---|---:|---:|---|
| Canonical atoms | 69 | 69 | Exact name set retained |
| Atom ABI typedefs | 276 | 276 | All four names retained per atom |
| Total public ABI type records | 286 | 286 | Exact name set retained |
| Non-empty public type records | 240 | 240 | Size, alignment, and fields unchanged |
| Field records | 370 | 405 | Reserved fields added; parameter sample-rate duplication removed |
| Zero-size GNU structures | 46 | 0 | Replaced by standard one-byte C11 layouts |
| Shared enum values | 17 | 17 | Names and numeric values unchanged |
| Public atom function symbols | 141 | 72 | Legacy non-context symbols removed; 69 primary and 3 spectral variants remain |

The immutable GNU-layout reference is `test/abi/dsp_types_abi_baseline_lp64.csv`; the first C11 result is
`test/abi/dsp_types_abi_phase1_lp64.csv`; and `test/abi/dsp_types_abi_c11_lp64.csv` tracks the current ABI. CTest
checks the current snapshot exactly and separately proves that the historical GNU-to-phase-1 delta contains only the
46 documented empty-layout transitions.

The family type tables, descriptor sources, canonical rows, public declarations, backend catalog contracts,
TypeScript catalog, and atom-binding JSON Schema are now generated from `schema/atoms/atoms.json`. DSP algorithms
remain handwritten. The frozen catalog reports all 69 atoms with current ports and fields, and the unit-editor consumes
the generated TypeScript counterpart.

## Verification Matrix

| Environment | Verification | Result |
|---|---|---|
| GCC 13.3.0, Linux x86_64 C11 | `./build-and-test.sh` | 72/72 passed |
| GCC 13.3.0, ASan + UBSan Debug | clean `/tmp` configure/build and CTest | 72/72 passed |
| GCC 13.3.0, 32-bit syntax mode | strict `dsp_atoms.h` freestanding compile | Passed |
| G++ 13.3.0 | public `extern "C"` include and typed symbol smoke | Passed |
| Arm GNU 13.2.1, Cortex-M7 | strict freestanding `dsp_atoms.h` and symbol-TU compile | Passed |
| M7 generated bundle | syntax, stack, object, section, host/ARM link tests | Passed in CTest |
| Native WASM contracts | image/processor/export/source boundary tests | Passed in CTest |
| Unit editor TypeScript/Vite | `npm run build` | Passed; existing chunk-size warning only |
| Unit editor ESLint | `npm run lint` | Passed |
| `check_v2` | labeled registry/runtime/catalog/export suite | 68/68 passed |
| Clang C11 | Compiler not installed in workspace | Not run |
| Emscripten | `emcc` not installed in workspace | Not run |
| MinGW/MSVC | Windows compiler not installed in workspace | Not run |

## Acceptance Criteria

- [x] `dsp_types.h` is an include-only compatibility umbrella.
- [x] Shared primitives, enums, ports, and 11 family ABI headers have explicit ownership.
- [x] Every new header compiles standalone with strict C11 warnings as errors.
- [x] Existing umbrella and `dsp_atoms.h` includes compile in C and C++.
- [x] All public atom type names are retained; the 69 context-only entry points and three spectral variants link.
- [x] Intentional ABI changes are isolated in the current snapshot; historical empty-layout evidence remains frozen.
- [x] Include-order tests pass and no circular include was introduced.
- [x] Canonical coverage rejects missing, duplicate-count, and orphan family rows.
- [x] DSP algorithms remain handwritten and no generated output contains behavior.
- [x] Registry, unit runtime, CLI/export, live-swap, WASM, M7, and sanitizer tests pass.
- [x] Type ownership, enum policy, empty layouts, new-atom workflow, and ABI rules are documented.
- [x] Production generation is deterministic and stale checked across C, TypeScript, and JSON Schema outputs.
- [x] C catalog golden data and the TypeScript consumer build remain synchronized.

## Commits

| Commit | Slice |
|---|---|
| `e19c2be` | Baseline inventory and LP64 ABI capture |
| `e4c40ff` | Family/category header split and umbrella conversion |
| `0b83224` | Standalone C/C++ and permanent ABI tests |
| `627c46b` | Standard C11 empty-layout transition |
| `1ef703f` | I/O alias-deduplication decision |
| `93380fa` | Namespaced enums and explicit values |
| `13afc07` | Canonical atom declarations and 141-symbol link test |
| `465618e` | Candidate schema generation and deterministic equivalence gate |
| `1cab3a7` | Context-only public atom execution API |
| `66ecd02` | Separate fixed-rate process and variable-rate stream contexts |
| `9aeaff0` | Context-only sample rate and explicit runtime state capacities |

## Follow-Ups

- Run the permanent header tests under Clang, Emscripten, and a Windows compiler when those toolchains are added to CI.
- Add editor visibility and rich parameter metadata to the authoritative schema and generated catalog.
- Consolidate oscillator/LFO, difference/slope, integration/accumulation, and wet/dry/crossfade implementation pairs.
- Resolve the pre-existing `freq_quantize` zero-descriptor versus four-byte `unused` params/state mismatch as a separate
  metadata/API decision.
- Keep atom-specific I/O structures distinct; alias deduplication was rejected because profile macros already remove
  maintained layout duplication without changing C type compatibility.
