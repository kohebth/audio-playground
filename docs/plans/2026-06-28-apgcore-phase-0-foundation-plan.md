# Plan: APGCore Phase 0 Foundation

**Date:** 2026-06-28
**Goal:** Establish the first v2 `apgcore` runtime boundary by adding process metadata, compatibility atom thunks, and variable-frame tests without rewriting the whole DSP catalog.
**Approach:** Option A — Phase 0 core foundation first.
**Complexity:** Medium

---

## Context Discovered

- V2 requirements/design call for `apgcore`, `apg_process_info_t`, atom metadata, runtime `frames`, and no fixed `CHUNK_LENGTH` assumptions in supported atoms.
- Current runtime is still v1-shaped: `runtime_context_t` has `sample_rate` and `chunk_length`; `runtime_unit_process()` processes exactly `ctx.chunk_length` samples.
- Current atom ABI is `atom(out, in, params, state)` with registry thunks in `src/rte/atom_register.c`; there is no process-info parameter.
- Many atom files define local `CHUNK_LENGTH 512`. The existing focused atom test covers `amplitude_multiply`, `amplitude_clip_soft`, `delay_line`, and `filter_biquad`.
- Existing unit tests assume 512-frame chunks, so the first validation should add multi-frame atom/runtime coverage before broad migration.

---

## Approaches Considered

| # | Approach | Pros | Cons | Effort |
|---|----------|------|------|--------|
| 1 | Phase 0 core foundation first ✓ | Builds the runtime contract needed by every later v2 phase; low UI churn | Initial work is mostly internal | Medium |
| 2 | Schema/compiler first | Clarifies file formats early | Risks designing ahead of runtime reality | Medium-High |
| 3 | Guitar pedalboard prototype first | Produces a quick visible demo | Can entrench current fixed-frame/runtime coupling | Medium |

**Why option 1:** The docs identify fixed chunk size, runtime/DSP coupling, and lack of process metadata as foundational blockers. Fixing the ABI path first reduces rework for compiler, CLI, web, and adapters.

---

## Execution Steps

| # | Step | Expected Output | Depends On |
|---|------|-----------------|------------|
| 1 | Add a small `apgcore`-style public process metadata type, likely under `inc/rte/` or a new `inc/apgcore/` boundary chosen to minimize churn. | `apg_process_info_t` with sample rate, frames, and channels. | — |
| 2 | Extend the registry call path to carry process metadata while preserving existing atom implementations through compatibility thunks. | Runtime can pass frame metadata to atom thunks without converting all atoms immediately. | Step 1 |
| 3 | Migrate the first supported atom subset: `amplitude_multiply`, `amplitude_clip_soft`, `delay_line`, and `filter_biquad`. | These atoms loop over `info->frames` instead of local `CHUNK_LENGTH`. | Step 2 |
| 4 | Update `runtime_unit_process` path to build process info from `runtime_context_t` and pass it through the registry. | Existing v1 YAML units still process, now through the metadata-aware path. | Steps 1-2 |
| 5 | Add multi-frame-size tests for the migrated atom subset, using 64, 128, 256, 512, and 1024 frames. | CTest catches regressions in variable-frame atoms. | Step 3 |
| 6 | Run the C build and CTest suite; fix scoped regressions only. | `cmake --build build` and `ctest --test-dir build` pass, or failures are documented. | Steps 1-5 |

---

## Risks & Assumptions

| # | Type | Description | Mitigation |
|---|------|-------------|------------|
| 1 | Risk | Changing atom signatures globally would force a large, noisy rewrite. | Use compatibility thunks and migrate only a small subset first. |
| 2 | Risk | Existing v1 unit runtime depends on 512-frame buffers and literal signal filling. | Keep `runtime_context_t.chunk_length` for now and map it into `apg_process_info_t.frames`. |
| 3 | Assumption | Phase 0 should preserve current YAML and tests. | Do not introduce v2 schemas or graph compiler in this slice. |
| 4 | Risk | Some spectral/SRC atoms may intentionally need fixed sizes. | Leave them unchanged and mark future metadata work rather than partially converting them. |

---

## Success Criteria

- [x] A process metadata type exists and is used by the registry/runtime call path.
- [x] Existing v1 unit loading and processing still compile.
- [x] The first migrated atom subset supports 64, 128, 256, 512, and 1024 frame buffers.
- [x] Existing focused atom behavior remains covered.
- [x] C build and CTest are run, with results reported.

---

## First Action

After this plan is approved, inspect the exact atom declaration and registry macro changes needed to introduce process metadata with minimal churn.
