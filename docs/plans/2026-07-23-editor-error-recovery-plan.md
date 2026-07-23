# Editor Error Recovery Plan

Status: Implemented

## Summary

Make project editing recoverable and understandable for non-technical users. Unused unit references remain available as
the project-local effect catalog but no longer block runtime validation. Project, editing, microphone, and save failures
show their exact diagnostics in a persistent responsive banner, and Save is an explicit action on mobile and desktop.

## Implementation

- Resolve and validate unit definitions only when a `chain.nodes` instance uses their reference. Keep all declared
  references in the parsed project, require every reference path to remain confined, and report active-unit failures.
- Apply the same active dependency semantics to native loading, the WASM workspace, browser readiness, compatibility,
  project inspection, and workspace summaries.
- Surface project and audio failures as separate persistent issues with the diagnostic code, path, actionable message,
  and a Details action. Clear recovered diagnostics instead of leaving the project visibly blocked.
- Keep autosave, add an explicit Save/Saved/Retry control, and allow local saves even when validation is blocked.

## Verification

- Cover empty and non-empty projects with unused references, unsafe paths, activation failures, pass-through compilation,
  and active dependency counts in native and WASM tests.
- Cover last-unit removal, validation recovery, microphone permission/device errors, and responsive Save states in the
  browser suite.
- Run the repository backend wrapper plus web unit, lint, production build, Studio Playwright, and Pages checks.
