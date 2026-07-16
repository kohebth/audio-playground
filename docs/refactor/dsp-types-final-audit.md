# DSP Type Refactor Final Audit

> This audit records the first type-header refactor as completed at `2f84b17`. The subsequent execution-API
> convergence intentionally removes the 69 legacy function symbols and makes process input/config pointers const.

## Inventory Comparison

| Surface | Baseline | Final | Result |
|---|---:|---:|---|
| Canonical atoms | 69 | 69 | Exact name set retained |
| Atom ABI typedefs | 276 | 276 | All four names retained per atom |
| Total public ABI type records | 286 | 286 | Exact name set retained |
| Non-empty public type records | 240 | 240 | Size, alignment, and fields unchanged |
| Field records | 370 | 416 | 46 documented `_reserved` fields added |
| Zero-size GNU structures | 46 | 0 | Replaced by standard one-byte C11 layouts |
| Shared enum values | 17 | 17 | Names and numeric values unchanged |
| Public atom function symbols | 141 | 141 | Typed link test covers every symbol |

The immutable GNU-layout reference is `test/abi/dsp_types_abi_baseline_lp64.csv`; the current C11 reference is
`test/abi/dsp_types_abi_c11_lp64.csv`. CTest rejects any difference except exactly 46 size-0/alignment-1 to
size-1/alignment-1 transitions and their offset-zero `_reserved` fields.

No file under `src/atom/` changed during the refactor. The only production C behavior adjustment keeps atom-catalog
`stateful` semantics independent of one-byte reserved storage. The frozen catalog now reports actual C sizes and the
unit-editor build consumes that updated contract.

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
- [x] All public atom type names and all 141 function symbols are retained.
- [x] Non-empty ABI is unchanged; the 46 empty-layout exceptions are isolated and documented.
- [x] Include-order tests pass and no circular include was introduced.
- [x] Canonical coverage rejects missing, duplicate-count, and orphan family rows.
- [x] No DSP algorithm source changed and no new production warning was observed.
- [x] Registry, unit runtime, CLI/export, live-swap, WASM, M7, and sanitizer tests pass.
- [x] Type ownership, enum policy, empty layouts, new-atom workflow, and ABI rules are documented.
- [x] Candidate generation is deterministic and byte-equivalent without replacing production headers.
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

## Follow-Ups

- Run the permanent header tests under Clang, Emscripten, and a Windows compiler when those toolchains are added to CI.
- Expand the candidate schema next to a pointer-owning state family before considering generated production ownership.
- Resolve the pre-existing `freq_quantize` zero-descriptor versus four-byte `unused` params/state mismatch as a separate
  metadata/API decision.
- Keep atom-specific I/O structures distinct; alias deduplication was rejected because profile macros already remove
  maintained layout duplication without changing C type compatibility.
