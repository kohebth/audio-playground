# Plan: Web UI Readiness

**Date:** 2026-06-29
**Goal:** Prepare stable backend contracts so the v2 web UI can be built against project, unit, validation, metadata, and runtime surfaces instead of changing C internals.
**Approach:** Option A: backend handoff first.
**Complexity:** Complex

---

## Context Discovered

- APGCore v2 is complete through Phase Y2 in `plan.md`: unit loader/compiler/runtime, fixtures, host bridge, control routing, atom catalog export, project schema validation, resolved project unit loading, mono project compilation, runtime product controls, product unit fixtures, the guitar pedalboard project fixture, deterministic project rendering, captured fixture outputs, frozen sample contracts, exact handoff commands, sample file references, and verification workflow are in place.
- `audio-playground-v2-requirements.md` and `audio-playground-v2-design.md` define the desired product as a visual pedalboard and graph composer, not just a YAML editor.
- The backend now exposes stable JSON data contracts for validation, atom inspection, unit inspection, project inspection, and deterministic project rendering. Runtime product controls, meters, product unit fixtures, representative pedalboard fixture, pedalboard sample outputs, unit inspect golden, and atom catalog manifest are implemented; benchmark tooling remains open.
- `web-tools/unit-editor/` exists as a separate package, but backend readiness should come before UI work to reduce frontend churn.

---

## Approaches Considered

| # | Approach | Pros | Cons | Effort |
|---|----------|------|------|--------|
| 1 | Backend handoff first | Stable UI contracts, fewer rewrites, clean test gates | Less immediate visual progress | High |
| 2 | Build UI and backend in parallel | Faster demos, can expose missing needs early | More churn, higher chance of ad hoc APIs | High |
| 3 | Minimal UI prototype now | Quick validation of layout ideas | Risks becoming a YAML editor and throwaway work | Medium |

**Why option 1:** The backend already has enough v2 runtime depth that the next risk is contract instability. Freezing inspect/validate/project surfaces before UI work gives the frontend a reliable target.

---

## Execution Steps

### Phase P: Backend Readiness Audit

| # | Step | Expected Output | Depends On |
|---|------|-----------------|------------|
| P1 | Audit current docs and implementation state | Web-readiness checklist | Completed Phase O |
| P2 | Identify UI-blocking backend contracts | `docs/WEB_UI_READINESS.md` | P1 |
| P3 | Update plan/task trackers | Phase Q-Y queue visible in repo | P2 |

### Phase Q: Unit Schema Stabilization

| # | Step | Expected Output | Depends On |
|---|------|-----------------|------------|
| Q1 | Validate `meta` and `ui` sections instead of only tolerating them | Loader validation and tests | Phase P |
| Q2 | Add param UI metadata contract | Schema docs and tests for label/control/unit/scale/precision | Q1 |
| Q3 | Emit stable validation paths for UI fields | Errors reference paths such as `params.gain.ui.control` | Q2 |

### Phase R: Atom Catalog Export

| # | Step | Expected Output | Depends On |
|---|------|-----------------|------------|
| R1 | Define atom catalog JSON shape | Documented contract | Phase Q |
| R2 | Add backend API or CLI/test binary for catalog export | JSON atom list with categories, fields, profiles | R1 |
| R3 | Add regression tests for exported metadata | Metadata stays aligned with compiler contracts | R2 |

### Phase S: Project v2 Schema

| # | Step | Expected Output | Depends On |
|---|------|-----------------|------------|
| S1 | Define `project.v2.yaml` / `chain.v2.yaml` schema | Schema docs | Phase R |
| S2 | Add project fixtures | `projects-v2/` examples | S1 |
| S3 | Validate unit refs, chain nodes, routes, scenes, and targets | Loader tests | S2 |

### Phase T: Project Loader and Resolver

| # | Step | Expected Output | Depends On |
|---|------|-----------------|------------|
| T1 | Resolve project-relative unit files safely | Project model owns resolved unit refs | Phase S |
| T2 | Load referenced v2 units | Multi-file project load tests | T1 |
| T3 | Reject unsafe paths and ambiguous refs | Security-focused loader coverage | T2 |

### Phase U: Project Compiler

| # | Step | Expected Output | Depends On |
|---|------|-----------------|------------|
| U1 | Expand unit instances into namespaced graph nodes/signals/params | Single compiled project graph | Phase T |
| U2 | Compile inter-unit routes | Route validation and schedule | U1 |
| U3 | Preserve instance param names | Runtime can address `delay1.feedback` | U2 |

### Phase V: CLI Contract

| # | Step | Expected Output | Depends On |
|---|------|-----------------|------------|
| V1 | Add JSON validation output | UI-readable errors/warnings | Phase U |
| V2 | Add inspect commands for atoms, units, and projects | Stable frontend data sources | V1 |
| V3 | Add render/benchmark command surface after product fixture scope is defined | Render JSON is implemented; benchmark output remains | Phase X |

### Phase W: Runtime Product Controls

| # | Step | Expected Output | Depends On |
|---|------|-----------------|------------|
| W1 | Implement smoothing from `smoothing_ms` | Click-safe param changes | Phase V |
| W2 | Add bypass/mute/solo product controls | Pedalboard-level controls | W1 |
| W3 | Add meter snapshots | Peak/RMS data for UI | W2 |

### Phase X: Product Fixture Slice

| # | Step | Expected Output | Depends On |
|---|------|-----------------|------------|
| X1 | Add or migrate guitar units | Done: overdrive, delay, tremolo, EQ/tone stack, noise gate, wet/dry mix | Phase W |
| X2 | Add guitar pedalboard project fixture | Done: `projects-v2/guitar-pedalboard.project.v2.yaml` | X1 |
| X3 | Validate, compile, run, and render fixture | Done: deterministic JSON via `apg-v2 render project <path>` | X2 |
| X4 | Capture compatibility and validation outputs | Done: pedalboard validate, inspect, and render goldens | X3 |

### Phase Y: Web Handoff Package

| # | Step | Expected Output | Depends On |
|---|------|-----------------|------------|
| Y1 | Freeze JSON contracts and sample outputs | Done: validation, unit inspect, project inspect, render, and atom catalog contracts | Phase X |
| Y2 | Document exact backend commands and sample files | Done: UI can develop against frozen fixtures listed in `docs/WEB_UI_READINESS.md` | Y1 |
| Y3 | Update repo guidance and trackers | Ready-to-start-web milestone | Y2 |

---

## Risks & Assumptions

| # | Type | Description | Mitigation |
|---|------|-------------|------------|
| 1 | Risk | CLI or JSON contracts may be designed too early and miss UI needs | Keep contracts small and fixture-driven |
| 2 | Risk | Project compiler can grow large quickly | Build only linear pedalboard routing first, then generalize |
| 3 | Risk | Runtime controls may require deeper realtime architecture | Keep initial controls block-boundary and deterministic |
| 4 | Assumption | Web UI should start at pedalboard level, then drill into unit internals | Confirm through product fixture and requirements docs |
| 5 | Assumption | Docs-only slices do not require `./build-and-test.sh` | Run the wrapper for any code/test implementation slice |

---

## Success Criteria

- [x] `unit.v2.yaml` includes validated UI metadata that can render a parameter panel.
- [x] Atom catalog metadata is exportable as stable JSON.
- [x] `project.v2.yaml` can load, validate, and compile multiple unit instances.
- [x] CLI or equivalent tools emit structured JSON validation and inspection output.
- [x] Runtime supports basic live product controls: params, bypass, and meters.
- [x] A representative guitar pedalboard fixture validates, compiles, runs, and renders deterministically.
- [x] Web handoff sample outputs are committed.
- [x] Web handoff docs are finalized.

---

## First Action

Complete Phase P by creating the web-readiness checklist and updating repo task tracking with Phases Q-Y.
