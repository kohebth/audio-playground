# Web UI Readiness Checklist

This checklist defines what must be true before the v2 web UI becomes the main workstream. The goal is to give the frontend stable backend contracts for visual editing, validation, preview, and export.

## Current Backend Status

- APGCore v2 loader, compiler, scheduler, runtime MVP, fixtures, host bridge, control-to-param routing, atom catalog export, project schema validation, resolved project unit loading, mono project compilation, validate/inspect JSON contracts, and runtime product controls for params, bypass, mute, and meters are implemented. Solo remains a host/UI routing concern until a real routing contract exists.
- `unit.v2.yaml` is executable and tested, and optional unit/param UI metadata is parsed and validated.
- Reusable test metadata fixtures exist in `test/fixtures/units-v2/`, including representative overdrive, phaser, tremolo, chorus, feedback delay, tone stack, noise gate, and wet/dry mix graphs.
- Project/session schema, deterministic test metadata fixtures, referenced-unit resolution, mono project compilation, and `test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml` exist. Its serial chain now places a six-stage all-pass phaser before overdrive and a cubic-delay chorus after tremolo, then uses the delay unit's own feedback and wet/dry controls instead of a redundant project-level blend. The final Schroeder reverb is exercised for tail continuity by native and Emscripten/WASM tests; the complete project supports desktop, WASM real-time, and offline render, but not M7 static export.
- The default `tone_stack` fixture is an audible Plexi-inspired amp stage rather than a scalar placeholder: preamp gain drives soft saturation; normalized bass and treble crossfades surround a 700 Hz `filter_biquad` band-pass mid branch; master volume drives a second saturation stage; and presence, cabinet roll-off, and a safety limiter shape the output. The stage is covered by native response/control tests and exposes six project knobs.
- The default `noise_gate` fixture uses full-wave envelope detection before thresholding, then smooths the gate gain with user-facing Attack and Release controls. This avoids closing on negative waveform halves and replaces abrupt switching with portable desktop/WASM/M7/offline ballistics; its three controls retain YAML order on one project-card row.
- The `apg-v2` CLI emits structured validation JSON, inspect JSON for atoms/units/projects, deterministic project render/benchmark JSON, and export surfaces for `wasm_realtime` and `m7_static`. Validation, unit inspect, project inspect, render, and atom catalog sample contracts are frozen under `test/golden/`.
- `schema/atoms/atoms.json` now generates the C atom ABI/registry contracts, TypeScript atom catalog, and atom-binding JSON Schema together; a CTest stale-output gate prevents frontend and backend field drift.

## Visual-First Web Studio Status

The primary web workflow is now visual and local-first. YAML remains the canonical engine and persistence contract, but
it is not exposed as the normal editing interface.

- A project home screen creates, opens, duplicates, imports, exports, and deletes browser-local projects backed by
  IndexedDB. The portable `.apg` package contains the versioned workspace, manifest, optional mono audio, and readiness
  snapshot; topbar import replaces the open project's contents in place, while home import creates a separate project.
- A global Simple/Pro switch serves both musicians and DSP authors. Simple mode provides an effect library, pedal-style
  controls, serial insertion, guided non-nested parallel routing through a real wet/dry mixer, microphone-only preview,
  presets, scenes, and a guided tour. Pro mode adds project structure, batch operations, file preview, readiness details,
  and unit-internals editing.
- Scenes capture parameter values and per-instance bypass state. Built-in and personal presets can be applied from the
  selected pedal, and structured units can be saved to a personal browser library.
- Pro unit editing uses structured identity, compatibility, parameter, port, and atom-graph controls. User-placeable
  effects are constrained to one mono audio input and one mono audio output; optional control ports remain available,
  while loaded multi-port mixers and routing helpers stay supported as internal project infrastructure. Raw YAML text is
  no longer mounted in the active editor; transformer and backend validation errors remain visible without replacing the
  last valid runtime.
- Simple mode accepts live mono input only. Pro mode additionally accepts packaged or selected audio files and rejects
  stereo/multichannel content with a clear error. Cloud sync, URL imports, browser recording, stereo projects, and browser
  deployment bundles remain outside this scope.
- The studio adapts to phone-sized viewports, preserves a direct pass-through empty project, and keeps common project
  terminology user-facing while retaining the existing v2 file contracts internally.

## Readiness Declaration

The APGCore v2 backend and visual web MVP surfaces are ready for production hardening. The release gate covers the C
suite, web contracts and transformers, TypeScript, lint, production build and artifact policy, focused studio/browser
workflows, repeated AudioWorklet lifecycle behavior, and the medium UI performance profile.

This is not a hardware readiness declaration. STM32H7/M7 production deployment is not ready yet: the `m7_static` path is a bounded C11 export surface for compatible/simple projects, not proof that the full guitar-pedalboard project runs on target hardware. The generic browser runtime now lives in `wasm-tools/`; the older project-specific `wasm_realtime` export remains a compatibility scaffold.

The real browser runtime is now tracked as the separate `wasm-tools/` project. Its versioned control ABI accepts
revisioned in-memory project/unit YAML, resolves unit references without a filesystem, validates and compiles the entry
project, and reports structured diagnostics and schedule summaries. Emscripten build targets and a build gate prevent
browser dependencies from entering APGCore. The control module now emits a checksummed pointer-free registry image;
the processor hydrates it into an inactive runtime, commits matching revisions at block boundaries, crossfades runtime
replacements, processes mono audio, applies indexed controls, and reads real output meters. Worker/AudioWorklet and
typed frontend integration are implemented through a versioned TypeScript facade. The editor debounces internal workspace
revisions, validates and prepares them in a Worker, stages valid images in an AudioWorklet, keeps failed edits away from
the active runtime, and routes stable parameter/bypass names plus meter snapshots through the facade. Runtime image
hydration and commit are separate Worklet messages; a newly announced editor revision invalidates stale preparation
before commit, while failure state retains the affected revision and diagnostic. In Pro mode, mono files decode through
WebAudio and feed the real processor Worklet; microphone capture remains a separate selectable input using the same
runtime. Simple mode keeps that choice focused on live microphone/audio-interface input.

The compact transport keeps one WASM backend alive across parent UI renders, starts and stops both decoded files and
microphone streams, and suppresses stale meter failures while a Worklet is being disposed. Browser regression coverage
exercises both transport modes. Workspace shortcuts map `Ctrl/Cmd+S` to local save, `B` to build and save, `M` to mute,
and `Space` to start or stop playback while leaving text-editing controls unaffected. The v2 initialization splash uses
the bundled APG grid mark and dismisses automatically when the audio runtime is ready, without a manual launch gate.

The right inspector exposes persisted browser audio input/output selection. AudioContext output routing is
feature-detected, microphone constraints request the selected device, mono capture, matching sample rate, and disabled
voice processing, and the UI reports actual capture/context sample rates plus capture/base/output latency. Device
changes rebuild the browser backend, rehydrate the same workspace revision, and restore running, mute, bypass, and
parameter-control state; a failed rebuild restores the previous known-good configuration.

The live preview exposes workspace, prepared, active, and failed revisions independently. Worker and processor failures
render the structured diagnostic code, phase, revision, file, and schema path instead of collapsing the backend result
to message text. Beginning a newer revision clears the stale diagnostic display while retaining the failed revision
counter, and a corrected snapshot proceeds through validation and preparation normally.

Parameter edits clamp to unit metadata, update the project YAML, use the indexed Worklet control for immediate audible
feedback, and then flow through the debounced replacement runtime. Resetting an override sends the original value over
the same fast path before the synchronized YAML revision is prepared.

Project-unit parameters are edited directly on their canvas cards. Each card renders compact knobs from the referenced
unit's parameter metadata, including label, range, and unit; the inspector retains structural actions and reset only.
Dragging a card knob uses the same clamped YAML update and live parameter synchronization path as other parameter edits.
Cards grow by parameter-row count and wrap at three knobs per row for every unit. Knob order follows the referenced unit
YAML parameter mapping exactly, and the Contract inspector can move parameters up or down through a structured YAML edit.
Project cards are fixed in both Simple and Pro modes. Topology changes rebuild a deterministic left-to-right Dagre layout
without consuming or writing project `ui.position` values, while scalar updates preserve the existing React Flow nodes,
edges, and viewport. Linear routes stay straight; split and merge routes use Dagre's obstacle lanes rendered as rounded
orthogonal elbows. Automatic layout never changes the current pan or zoom after the initial mount.

Monitoring is explicitly polled at 10 Hz outside `process()`. Snapshots include peak, RMS, frame count, active revision,
and underruns; the render callback performs no meter message allocation or temporary typed-array view allocation.

Developer Diagnostics includes an opt-in five-second microphone latency profile. The AudioWorklet measures callback
cadence gaps and samples input copy, WASM graph execution, output copy, latency-probe work, channel copy, and total
callback time every eighth quantum into fixed-capacity buffers. Polling constructs mean, p95, maximum, deadline
utilization, underrun, and deadline-miss results outside `process()`. Reports classify real-time health from callback
execution, underruns, and deadline misses; callback cadence gaps remain diagnostic because browser/device render batching
does not alone prove a missed deadline. The UI combines those results with browser latency estimates and an available
acoustic-loopback result, and exports the
versioned `apg.audio-trace.v2` JSON contract. Device-aware calibration measures the current graph for five seconds at
2.7 ms, 5.3 ms, 10.7 ms, and interactive latency hints, rejects unstable candidates, installs the lowest reported stable
path, and retains the chirp as the authoritative under-15-ms round-trip check. APGCore and embedded runtime boundaries
remain unchanged.

The live microphone-path estimate remains neutral through 20 ms, warns above 20 ms, and shows danger above 30 ms.
Output-only estimates and measured acoustic-loopback status retain their existing presentation.

The GitHub Pages production boundary uses a configurable Vite base path and hash routes for the project and unit views.
Developer Diagnostics exposes the injected build commit, deployment base, and mode, and the explicitly public overdrive
v2 fixture is shipped under `units/` with a contract test that prevents fixture drift. Clean `npm ci`, lint, tests, a
`/audio-playground/` production build, and the project/unit browser route flow pass locally. The Pages workflow now
rebuilds and smoke-tests the Emscripten control/processor modules, uses clean installs and fixed Node 22, runs TypeScript,
lint, contract, artifact-policy, and production-build Playwright gates, uploads only `web-tools/dist`, and deploys only
after a successful numbered `v2.0-beta<number>` tag build; normal `main` pushes and manual dispatches do not publish.
Pull requests retain the same validation without artifact upload. The browser palette hides the three advanced
frequency atoms whose backend profiles mark `wasm_realtime` unsupported. GitHub Pages publishes the workflow artifact
at `https://kohebth.github.io/audio-playground/`; Pages reports `built/workflow`, HTTPS is enforced, and the deployment
environment is restricted to the beta tag family. Earlier push run `29566796038` and manual run `29566959580` passed the
complete build and deployment gates for production revision `4a85a30b8432e476b90bce7e3fccb2b8ad288d6b`, and live
Playwright acceptance passed 5/5 while asserting that exact diagnostic SHA. Evidence is recorded in
`docs/plans/2026-07-17-github-pages-deployment-plan.md`. Production operation, local reproduction, public-data rules,
diagnostics, troubleshooting, and rollback are documented in `docs/GITHUB_PAGES_DEPLOY.md`.

The processor keeps an active, staged, and retired runtime slot. It only promotes a staged slot at a block boundary and
crossfades that block; retired-slot destruction occurs during a later control-thread staging operation, never in the
audio callback. A CMake boundary test rejects allocation, slot destruction, formatting, parsing, compilation, image
hydration, measurement lookup, and string lookup calls from the processor callback and its real-time diagnostic helper.
The pinned Emscripten Docker build script also executes the generated control/processor modules under Node with supplied
WASM bytes, covering workspace diagnostics, prepared-image transfer, hydration, commit, processing, controls, meters,
and corrupt-image rollback before public editor artifacts are replaced.

Bypass and mute use indexed Worklet controls. The facade retains per-instance bypass and project mute shadows, applies
them to a newly hydrated runtime before commit, and therefore preserves live control state across structural swaps.
The preview exposes both controls while running. Each pedal card uses its full-width footer as the on/off target and
fades to 50% opacity while bypassed, making inactive stages visible at a glance. Project routes omit repetitive
`output -> input` edge labels while retaining their selectable paths. Live bypass and mute remain runtime controls rather
than instance properties; scene snapshots persist supported per-instance bypass values alongside their parameter values.

Unit Atom CRUD is backed by structured YAML transforms and executable transformer tests. The editor can create a valid
unit scaffold, add catalog-derived atoms, rename nodes, edit bindings/configuration, and remove atoms. Removing or cutting
a one-signal-input/one-signal-output atom bridges its upstream signal to every compatible consumer and public output;
removing a mixer or other special atom clears incident bindings and leaves explicit disconnected endpoints. The control
Worker validates
and compiles every unit draft in a snapshot, including units not yet referenced by the entry project, so incomplete
forms return file-specific diagnostics without replacing the active runtime.

Unit connections use structured output/input endpoints. Clicking an output handle arms a connection and clicking an input
commits it; Escape or a canvas click cancels the armed state. React Flow connect, reconnect, and edge-delete actions
transform YAML bindings and pass through the common validation/swap pipeline. The transformer rejects unknown nodes or
fields,
incompatible catalog field types/sizes, occupied targets, and cycles. Canvas node movement updates UI-only position state
and does not change DSP YAML or announce a workspace revision.

The unit contract canvas derives two fixed, handle-sized boundary rings from public audio ports. Each ring is labeled with
its graph signal name: the left ring shows where the preceding project stage enters atom inputs, and the right ring shows
which atom signals leave for the following stage. Boundary rings and their highlighted edges are view-only, remain outside
atom counts and unit YAML, and cannot be selected, dragged, deleted, reconnected, or used as insertion targets.

Project chain editing is driven from the current project YAML rather than the frozen inspect sample. Users can add,
duplicate, remove, rename, and reorder instances; add, replace, disconnect, and reorder routes; and select endpoints from
resolved unit port metadata. Project and atom cards expose keyboard-accessible right-click menus for replace, cut, copy,
paste, and remove; unit menus additionally expose live on/off. Paste creates a disconnected sibling. Replacement previews
its impact, keeps the project instance ID and routes, and resets replacement parameters and scene values to defaults.
Rename updates route endpoints, parameter-control identities, and scene paths atomically. Removing or cutting a normal
effect bridges its upstream route to every downstream branch; special routing units drop incident routes and remain an
explicit repair task. Direction, port, occupied-target, and cycle checks run before snapshot
synchronization. A broken chain can validate structurally but fails preparation, leaving the previous active revision in
the Worklet until a complete route is restored.

The internal workspace still uses the versioned `apg.ui.workspace.v2` envelope. Browser persistence wraps that workspace
in `apg.project.package.v1`, stores projects in IndexedDB, and exports/imports the same JSON-based `.apg` package. The
package retains every project/unit file as path, role, and YAML content plus manifest, optional mono audio, and readiness
data; baseline-only editor fields are not serialized. Restore validates format versions, confined relative paths, unique
files, roles, entry project, audio shape, and readiness data before mounting the Worker/AudioWorklet integration. Legacy
local storage migrates once. Invalid imports report the error without replacing the active workspace, while valid
imports proceed through normal WASM validation and revision preparation.

## Ready To Start Web UI When

- [x] Unit metadata can render a parameter panel without frontend hardcoding.
- [x] Atom catalog metadata can render an atom palette without frontend hardcoding.
- [x] Project files can reference units, define routes between unit instances, and compile mono routes into one runtime plan.
- [x] Validation returns structured errors and warnings with stable file/path fields.
- [x] A guitar pedalboard fixture validates, compiles, runs, and renders deterministically.
- [x] Runtime supports the first live UI controls: parameter changes, bypass, and meters.
- [x] Sample JSON outputs are committed for frontend tests and UI mock data, including unit inspect, project inspect, atom catalog, and guitar pedalboard fixture contracts.

## Frozen Backend Samples

Build the CLI once through the normal C workflow, then use these exact commands as frontend fixture metadata sources:

```sh
./build/apg-v2 validate unit test/fixtures/units-v2/simple_gain.unit.v2.yaml
./build/apg-v2 validate project test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml
./build/apg-v2 inspect atoms
./build/apg-v2 inspect unit test/fixtures/units-v2/simple_gain.unit.v2.yaml
./build/apg-v2 inspect project test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml
./build/apg-v2 render project test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml
./build/apg-v2 benchmark project test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml
./build/apg-v2 export --target wasm_realtime test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml dist/web/
./build/apg-v2 export --target m7_static test/fixtures/projects-v2/two-gain-chain.project.v2.yaml build/m7/
```

Committed sample files:

- `test/golden/v2-validate-unit-simple_gain.json`
- `test/golden/v2-validate-project-two-gain-chain.json`
- `test/golden/v2-validate-project-guitar-pedalboard.json`
- `test/golden/v2-inspect-unit-simple_gain.json`
- `test/golden/v2-inspect-project-two-gain-chain.json`
- `test/golden/v2-inspect-project-guitar-pedalboard.json`
- `test/golden/v2-render-project-guitar-pedalboard.json`
- `test/golden/v2-inspect-atoms.json`
- `test/golden/v2-inspect-atoms.manifest.txt`

## Backend Contracts Needed

### Unit Inspect Contract

The UI needs a structured view of one unit:

- `name`, `version`, `meta`, `compatibility`
- public params with defaults, ranges, smoothing, and UI hints
- input/output ports and channel layout
- graph nodes and binding errors for unit-internals editing

### Atom Catalog Contract

The generated TypeScript catalog and backend inspect contract provide a structured atom palette:

- atom name and category
- `public`, `advanced`, or `internal` visibility; only public atoms appear by default
- input, output, config, and state fields
- config defaults, bounds, units, scale, enum options, smoothing hints, and real-time/structural policy
- supported target profiles
- constraints such as fixed-size or stateful behavior

The current generated catalog contains 72 atoms: 27 public, 26 advanced, and 19 internal. The default palette shows
the public subset, advanced mode adds the advanced subset, and internal compatibility/infrastructure atoms remain
loadable without being addable from the palette.

### Project Contract

The UI needs a full pedalboard/session model:

- unit references
- unit instances
- routes between instances
- scenes and preset parameter values
- target backend settings

### Validation Contract

The UI needs stable diagnostics:

```json
{
  "ok": false,
  "errors": [
    {
      "code": "APG_ATOM_UNKNOWN",
      "file": "test/fixtures/units-v2/example.unit.v2.yaml",
      "path": "graph.nodes[2].atom",
      "message": "Unknown atom 'filter_biquadd'"
    }
  ],
  "warnings": []
}
```

### Runtime Preview Contract

The UI needs a way to drive live or offline preview:

- set parameter by stable instance path
- bypass unit instance
- read meters
- compile/swap a valid graph without replacing active audio on failure
- move or replace units by preparing a resolved project in host, committing only a valid replacement runtime, and
  crossfading mono preview output after commit

## Phase Gates

- **Phase Q:** Unit schema validates UI metadata.
- **Phase R:** Atom catalog is exportable.
- **Phase S:** Project schema and fixtures exist.
- **Phase T:** Project loader resolves multi-file units safely. Complete.
- **Phase U:** Project compiler creates a single runtime plan. Complete for mono project routes.
- **Phase V:** CLI tooling emits JSON inspect/validate output and deterministic project render output. Complete.
- **Phase AH:** CLI tooling emits deterministic benchmark JSON, deterministic `wasm_realtime` export scaffolds, and bounded C11 M7 static bundles for compatible/simple projects. Complete as an export surface, not as STM32H7 production readiness.
- **Phase W:** Runtime supports product controls and meters. Complete for params, bypass, mute, and peak/RMS meter snapshots.
- **Phase X:** Representative unit fixture metadata, the guitar pedalboard project fixture metadata, deterministic render proof, and compatibility/output capture are complete.
- **Phase Y:** Web handoff package freezes sample contracts, documents exact fixture commands, refreshes repo guidance, and declares backend readiness. Complete.
- **Visual-first studio follow-on:** Project home, IndexedDB/`.apg` persistence, Simple/Pro workflows, scenes, presets,
  parallel routing, locked deterministic project layout, structured Pro editing, and browser release/performance gates
  are complete.

## Delivered Web UI Scope

The first product workflow is delivered:

- [x] Local project browser and portable `.apg` packages
- [x] Pedalboard canvas for unit instances, serial routes, and guided parallel wet/dry routes
- [x] Simple musician workflow with pedals, live input, scenes, presets, and tour
- [x] Pro workflow with diagnostics, compatibility, batch actions, file preview, and readiness
- [x] Parameter controls generated from unit metadata
- [x] Structured atom-level unit editor after the project-level workflow
- [x] One-click project/atom connections with named handles and cancel state
- [x] Mono user-effect placement policy with internal multi-port routing exemptions
- [x] Unit/atom context menus, replacement previews, disconnected paste, topology-aware removal, and Undo
