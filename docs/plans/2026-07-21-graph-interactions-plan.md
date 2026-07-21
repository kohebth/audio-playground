# Plan: Click-to-connect and graph action improvements

**Date:** 2026-07-21
**Goal:** Make project units and contract atoms faster to connect and edit while preserving valid audio topology, simple mono effect rules, and existing special-routing compatibility.
**Approach:** Extend the existing React Flow/YAML transformer stack with explicit click connections, typed context menus and clipboards, topology-aware removal, and a brighter canvas token.
**Complexity:** Complex

---

## Context Discovered

- React Flow already supports click-to-connect and the atom canvas already commits connections; the project canvas lacks the route callback.
- Project runtime bypass exists, but atom bypass does not, so only unit context menus expose On/Off.
- The project supports multi-port mixer/stereo contracts, and parallel routing depends on an internal two-input wet/dry mixer.
- Existing atom and project transformers already provide validation, replacement, clipboard, history, and autosave boundaries to extend.

---

## Approaches Considered

| # | Approach | Pros | Cons | Effort |
|---|----------|------|------|--------|
| 1 | Focused frontend transactions ✓ | Reuses React Flow, validation, Undo, and YAML contracts without backend changes | Requires careful project/atom topology transforms | High |
| 2 | Strict single-port schema | Simplifies every route | Breaks mixers, stereo units, fixtures, and parallel routing | Very high |
| 3 | Runtime atom bypass expansion | Gives every menu a live toggle | Expands into compiler/runtime/WASM work outside the web request | Very high |

**Why option 1:** It delivers the requested workflows while retaining special routing and the production backend contracts.

---

## Execution Steps

### Phase 1: Data policies and transactions

| # | Step | Expected Output | Depends On |
|---|------|-----------------|------------|
| 1 | Add mono user-effect port classification and placement guards | One mono audio input/output is mandatory for user-placeable effects; control ports and loaded special units remain supported | — |
| 2 | Add project unit replace, clipboard, and topology-aware remove transforms | Replacement resets parameters/scenes; simple removals bridge every downstream branch; special removals disconnect | Step 1 |
| 3 | Add atom disconnected-paste and topology-aware remove transforms | Pasted atoms have fresh outputs and empty inputs; simple removals bridge consumers/public outputs | Step 1 |

### Phase 2: Canvas interactions

| # | Step | Expected Output | Depends On |
|---|------|-----------------|------------|
| 4 | Expose named project handles and explicit click connections | One click arms an output and one click commits an input in both graph editors | Phase 1 |
| 5 | Add accessible unit/atom context menus and replacement previews | Pointer and keyboard menus expose the approved action sets with disabled reasons | Phase 1 |
| 6 | Apply the brighter canvas surface token | Project and contract backgrounds brighten without changing grid lines | — |

### Phase 3: Coverage and delivery

| # | Step | Expected Output | Depends On |
|---|------|-----------------|------------|
| 7 | Add transformer, browser, contract, and performance coverage | Regression coverage for policy, connections, menus, replacement, paste, removal, and Undo | Phases 1-2 |
| 8 | Update web readiness documentation and run release gates | Documentation matches behavior; all web verification passes | Step 7 |
| 9 | Review and commit only this slice | One focused Conventional Commit; unrelated local artifacts remain untouched | Step 8 |

---

## Risks & Assumptions

| # | Type | Description | Mitigation |
|---|------|-------------|------------|
| 1 | Risk | Auto-bridging can create an occupied target or cycle | Validate the complete replacement atomically and abort removal on failure |
| 2 | Risk | Special routing cannot be bridged generically | Remove incident links and leave explicit disconnected handles |
| 3 | Assumption | “Single input/output” means one mono audio input/output for user effects | Count only audio ports; allow control ports and internal routing infrastructure |
| 4 | Assumption | Paste creates a disconnected sibling | Use unique IDs, clear atom inputs, omit project routes and scenes |
| 5 | Assumption | Immediate destructive edits are acceptable with Undo | Push one history entry before each successful transaction and show the result |

---

## Success Criteria

- [x] Click output then input creates a validated project route or atom binding; Escape/pane click cancels.
- [x] User libraries reject non-mono effects while existing special units and parallel wet/dry routing still load and work.
- [x] Unit and atom context menus expose the approved actions in their applicable modes with keyboard access.
- [x] Replace, Cut, Copy, Paste, Remove, bridge, disconnect, and Undo behavior match the approved rules.
- [x] Both graph canvases use `--bg-canvas: #151813` while keeping the existing grid line color.
- [x] Static, transformer, browser, performance, build, and Pages release checks pass.

---

## Outcome

Implemented as a frontend-only slice on 2026-07-21. The final release gates passed: TypeScript, ESLint, contract and
transformer tests, the full graph performance threshold suite, 10 focused studio/transport browser tests, the
base-path production build and artifact policy, and 5 GitHub Pages smoke tests.

---

## First Action

Implement and test the shared port-classification and graph transaction helpers before wiring UI state.
