# Plan: Pipeline and Contract workspaces

**Date:** 2026-07-22
**Goal:** Restore the pedalboard as the primary product view and make effect-definition editing explicit, safe, and
approachable without exposing YAML.

## Product decisions

- **Pipeline** is the existing knob-rich board. One continuous main rail runs from Input through serial units to Output
  and tucks beneath their cards; panner/mixer cards, flexible height, separate straight branch rails, and rounded
  orthogonal turns are preserved. Connection handles reveal only during interaction.
- **Contract** edits one Personal effect definition as a Graphviz atom graph. Its right inspector is atom-only;
  identity, compatibility, parameters, and ports live in a separate Contract Settings drawer.
- Editing a built-in first creates a Personal copy. Entry is available from a library context menu and a Pipeline
  instance context menu.
- Editing an instance rebinds only that instance to the Personal copy. Later definition edits update matching instances
  in the active project, including routes, parameter defaults, and scene paths; other projects remain unchanged.
- User effects retain exactly one mono audio input and one mono audio output. Internal routing helpers remain exempt.
- Microphone is the first and default preview source in both workspaces; mono-file preview remains available as the
  secondary source through view-independent transport and Audio I/O controls.
- Opening or reloading a project always starts in Pipeline. Internal `simple`/`pro` values are retained only for
  `.apg` compatibility.

## Delivered implementation

- [x] Rename and separate the two visible workspaces.
- [x] Preserve the Pipeline cards, controls, routing geometry, context actions, scenes, and locked layout.
- [x] Add Personal-copy creation, instance rebinding, active-project propagation, serialized persistence, and Undo/Redo.
- [x] Add actionable empty Contract guidance, Graphviz lifecycle stabilization, atom-only inspection, and Contract
  Settings.
- [x] Move Audio I/O into a view-independent non-modal topbar panel.
- [x] Normalize direct/reloaded Contract routes back to Pipeline without breaking internal Contract navigation.
- [x] Offer blank-rail and bundled eight-effect templates when creating a project.
- [x] Cover graph transforms, ownership/propagation, route stability, invalid edits, responsive Pipeline behavior,
  live audio, and production smoke paths.

## Verification

- `npm run lint`
- `npm test`
- `npm run build`
- Playwright: 12 Pipeline/Contract/live-preview tests
- Playwright: 5 production Pages smoke tests
