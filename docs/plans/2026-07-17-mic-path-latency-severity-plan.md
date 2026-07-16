# Plan: Mic Path Latency Severity

**Date:** 2026-07-17
**Goal:** Color the live microphone-path estimate as warning above 20 ms and danger above 30 ms without changing audio behavior.
**Approach:** Classify the displayed microphone-path total with a tested pure helper and apply existing warning/danger colors.
**Complexity:** Simple

---

## Context Discovered

- `LiveLatencyBadge` already separates microphone estimates, output-only estimates, and measured acoustic loopback results.
- The microphone estimate is capture latency plus the browser render/output estimate.
- Existing orange and red design tokens can express warning and danger without adding new theme values.

---

## Approaches Considered

| # | Approach | Pros | Cons | Effort |
|---|----------|------|------|--------|
| 1 | Tested pure severity helper ✓ | Exact boundaries, reusable behavior, focused unit coverage | Adds one internal export | Small |
| 2 | Inline component conditionals | Fewest lines | Boundary behavior is harder to test directly | Small |
| 3 | CSS attribute selectors | Styling stays declarative | Classification still needs component logic | Small |

**Why option 1:** It preserves the simple component while making the literal `>20` and `>30` rules independently verifiable.

---

## Execution Steps

| # | Step | Expected Output | Depends On |
|---|------|-----------------|------------|
| 1 | Add boundary tests and severity classification | Normal, warning, and danger behavior is locked at 20/30 ms | — |
| 2 | Apply severity only to microphone estimates and add matching styles | Badge color changes without affecting output-only or loopback modes | Step 1 |
| 3 | Update the frontend contract/readiness note, verify, and commit | Tests, build, and lint pass for the focused slice | Step 2 |

---

## Risks & Assumptions

| # | Type | Description | Mitigation |
|---|------|-------------|------------|
| 1 | Risk | Severity could accidentally affect measured loopback or output-only badges | Gate classification on available microphone capture latency and retain existing branches |
| 2 | Assumption | Exactly 20 ms is normal and exactly 30 ms is warning | Cover both boundaries explicitly |

---

## Success Criteria

- [x] Microphone estimates at or below 20 ms retain the normal teal style.
- [x] Microphone estimates above 20 ms through 30 ms use warning orange.
- [x] Microphone estimates above 30 ms use danger red.
- [x] Output-only and measured-loopback behavior remains unchanged.
- [x] Unit-editor tests, build, and lint pass.

---

## First Action

Add focused severity boundary assertions to the existing audio I/O test script.
