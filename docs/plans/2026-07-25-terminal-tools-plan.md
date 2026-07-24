# Terminal Tools Implementation Plan

## Goal

Add an optional native C++20 `terminal-tools/` package producing `apg-tui`: a mouse/keyboard terminal Pipeline editor for project-level unit chains, with validated editing, atomic save, and a native audio-session seam. The first implementation slice is deliberately deterministic and buildable without changing APGCore or the browser runtime.

## Scope

- Include project-level chain editing: select, insert, remove, move, parameter adjustment, undo/redo, validate, and save.
- Keep the project YAML and existing v2 route semantics authoritative; `chain.routes`, not `chain.nodes` order, controls audio order.
- Provide FTXUI interaction and a session interface with a null backend for tests.
- Leave atom-graph editing, stereo projects, cloud sync, and release packaging for follow-up slices.
- Keep the package optional and excluded from Emscripten builds.

## Architecture

```text
FTXUI -> ProjectDocument -> edit commands -> APGCore validation
                                |
                                +-> Session interface -> null/audio backend
```

The terminal UI never runs in the audio callback. Structural edits create a new document revision; validation and runtime preparation happen outside the callback; a later audio backend can implement staged swaps and deferred reclamation.

## Verification

```sh
cmake -S . -B build-terminal -DAPG_BUILD_TERMINAL_TOOLS=ON
cmake --build build-terminal --parallel
ctest --test-dir build-terminal -L terminal-tools --output-on-failure
```

Existing native, WASM, and web gates remain unchanged and must continue to pass.
