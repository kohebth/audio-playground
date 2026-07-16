# Plan: Flatten the Unit Editor Package

**Date:** 2026-07-17
**Goal:** Make `web-tools/` the unit-editor package root with no remaining nested `unit-editor/` layer or broken repository integration.
**Approach:** Move the complete package one level up, then update all path consumers and depth-sensitive imports before verification.
**Complexity:** Medium

---

## Context Discovered

- `web-tools/` contains only the `unit-editor/` package, so the parent has no conflicting tracked files.
- The package has local ignored dependencies, build output, test results, and generated WASM assets that should remain usable after the move.
- CI workflows, the WASM copier, atom generation checks, docs, package links, fixtures, and tests all encode the old depth.

---

## Approaches Considered

| # | Approach | Pros | Cons | Effort |
|---|----------|------|------|--------|
| 1 | Move the complete package and update all consumers ✓ | Clean final tree; preserves local generated assets; no compatibility layer | Broad path-only diff | Medium |
| 2 | Move tracked files only | Smaller filesystem operation | Leaves ignored artifacts nested under an obsolete directory | Medium |
| 3 | Keep a compatibility symlink | Fewer immediate reference changes | Retains ambiguous package roots and stale paths | Small |

**Why option 1:** It produces the requested un-nested layout without leaving two apparent package roots.

---

## Execution Steps

| # | Step | Expected Output | Depends On |
|---|------|-----------------|------------|
| 1 | Move package contents from the former child directory to `web-tools/` | One physical package root | — |
| 2 | Update dependency paths, fixture imports, scripts, generators, CMake checks, CI, and docs | No functional reference to the old directory | Step 1 |
| 3 | Reinstall the local package link and scan for stale paths | `@audio-playground/wasm-tools` resolves from the new depth | Step 2 |
| 4 | Run unit-editor tests, build, lint, and focused artifact-generation verification | Flattened package is operational | Step 3 |
| 5 | Stage only migration files and commit | One verified refactor commit | Step 4 |

---

## Risks & Assumptions

| # | Type | Description | Mitigation |
|---|------|-------------|------------|
| 1 | Risk | Relative paths can silently point one directory above the repository | Update every depth-sensitive path and exercise all package scripts |
| 2 | Risk | CI or generated-atom checks can retain the old path | Run a repository-wide stale-path scan and focused generator verification |
| 3 | Assumption | `web-tools/` is intended to become the package root | Confirmed by the requested parent-folder target and absence of siblings |

---

## Success Criteria

- [x] `web-tools/package.json`, `web-tools/src/`, and related package files exist at the parent level.
- [x] The nested `unit-editor/` child no longer exists.
- [x] No active repository path points to the former child location.
- [x] Local WASM dependency and generated assets resolve correctly.
- [x] Tests, build, lint, and artifact-generation verification pass.

---

## First Action

Move the complete package contents into the conflict-free `web-tools/` parent.
