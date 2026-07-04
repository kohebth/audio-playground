# Audio Playground v2 Upgrade Requirements

**Status:** Draft for review  
**Version:** 0.1  
**Date:** 2026-06-27  
**Scope:** Product + engineering requirements for the upgrade version

---

## 1. Purpose

Audio Playground v2 upgrades the current atom/unit prototype into a portable, realtime-safe audio graph platform.

The product should let users visually build, save, route, parameterize, validate, live-preview, and export DSP chains across desktop, browser, mobile, and embedded targets.

This document defines **what the upgrade must deliver**. The companion design document defines **how the system should be built**.

---

## 2. Product Goal

Audio Playground v2 should become:

> A portable audio graph engine where users visually build, save, route, parameterize, and live-play DSP chains across desktop, web, mobile, and embedded hardware.

The product should not be only a YAML editor. YAML remains the portable file format, while the primary user experience is a live pedalboard / graph composer.

---

## 3. Current Foundation

The current model is strong and should be preserved:

- An audio system is constructed from **units**.
- Each unit is created from **atoms**.
- Each atom has its own DSP functionality.
- The existing `overdrive` unit already demonstrates useful primitives: `params`, `signals`, and ordered DSP `pipeline` steps.
- The atom catalog already covers enough DSP categories for a real product: generation, amplitude, filter, delay, mix, detect, modulation, nonlinear, frequency-domain processing, sample-rate conversion, and interpolation.

The main upgrade requirement is to make this model portable, validated, realtime-safe, and usable from a visual product workflow.

---

## 4. Main Problems to Solve

| Problem | Impact | Required Fix |
|---|---|---|
| Fixed `CHUNK_LENGTH 512` in atom implementations | Limits portability across Web Audio, desktop, and embedded targets | Replace with runtime `frames` metadata |
| YAML-style graph is close to runtime execution | Unsafe for audio thread and hard to optimize | Compile YAML into numeric graph plan |
| Unit and full project/session are not separated | Reuse and multi-file projects are limited | Add `unit.v2.yaml` and `project.v2.yaml` formats |
| Runtime and DSP concerns are not clearly separated | Hard to target PipeWire, WASM, React Native, and Cortex M7 | Introduce `apgcore` and runtime adapters |
| No formal validation layer | Hard to trust graph correctness | Add schema, graph, and compatibility validation |
| No live-edit architecture | Knob/routing changes can glitch audio | Add control queue, smoothing, and hot graph swap |
| No clear compatibility profile | Some atoms are unsuitable for M7 or Web | Add backend profiles and compatibility matrix |

---

## 5. Target Users and Use Cases

| User | Need | Example |
|---|---|---|
| Musician / maker | Build and play effects chains | Create an overdrive → delay → tremolo pedalboard |
| DSP developer | Compose reusable DSP units from atoms | Create a new compressor unit from detect + amplitude atoms |
| Embedded developer | Export deterministic graph to Cortex M7 | Compile a restricted pedalboard into static C arrays |
| Web user | Edit and hear audio live in browser | Modify a unit graph and preview through WASM AudioWorklet |
| QA / developer | Validate and compare behavior | Render WAV output and run regression benchmarks |

---

## 6. Product Scope

### 6.1 In Scope for v2 MVP

- Rename/separate core DSP engine as `apgcore`.
- Replace fixed block size assumptions with runtime process metadata.
- Add atom metadata registry.
- Add `unit.v2.yaml` for reusable DSP units.
- Add `project.v2.yaml` or `chain.v2.yaml` for full sessions / pedalboards.
- Add graph compiler: YAML → validated numeric graph → process plan.
- Add scheduler, cycle detection, memory planner, and parameter binding.
- Add CLI validation and offline WAV render path.
- Add web app multi-file project support and autosave.
- Add visual pedalboard editor and unit graph editor.
- Add WASM live preview path.
- Add Linux runtime support for live parameter changes and safe graph hot-swap.
- Add Cortex M7 export with a restricted atom subset.

### 6.2 Out of Scope for v2 MVP

- Full DAW timeline editing.
- Multi-track arrangement view.
- Plugin store payments.
- Third-party untrusted native code running in the audio callback.
- Sending audio buffers through JavaScript or React Native JS.
- Runtime YAML parsing on Cortex M7.
- Full VST/AU/AAX plugin support as a hard MVP dependency.

---

## 7. Functional Requirements

### FR-01 — Preserve the atom/unit model

The system must keep the existing concept that units are reusable DSP blocks built from atoms.

**Acceptance criteria**

- Existing unit-style graphs can be migrated into v2 units.
- Atom names, ports, configs, and state requirements are discoverable through metadata.
- Users can compose units without writing C/C++.

---

### FR-02 — Support `unit.v2.yaml`

The system must support a reusable unit file format.

Required fields:

- `kind`
- `schema`
- `name`
- `version`
- `params`
- `ports`
- `graph`
- `ui`
- `compatibility`

**Acceptance criteria**

- A v2 unit can define public controls, audio/control ports, internal atoms, routes, defaults, ranges, labels, and UI hints.
- Unit validation reports missing atoms, invalid params, disconnected ports, cycles, and unsupported backend features.

---

### FR-03 — Support `project.v2.yaml` / `chain.v2.yaml`

The system must support a project/session file that references reusable units and defines full routing.

Required capabilities:

- Multi-file unit references.
- Chain nodes.
- Routes between units.
- Scenes / presets.
- Automation events.
- Target backend configuration.

**Acceptance criteria**

- One overdrive unit file can be reused in multiple projects.
- A project can load multiple unit files.
- Validation resolves all referenced unit files before compilation.

---

### FR-04 — Compile YAML into a numeric graph

The runtime must not execute YAML directly.

Required compiler pipeline:

```text
YAML files
  ↓
schema validation
  ↓
unit expansion
  ↓
graph compilation
  ↓
numeric node graph
  ↓
topological schedule
  ↓
real-time process plan
```

**Acceptance criteria**

- Audio thread uses numeric IDs, pointers, buffers, and precomputed schedules.
- No string lookup is required in the audio callback.
- Compile errors are deterministic and user-readable.

---

### FR-05 — Validate schemas and graph correctness

The system must validate files before runtime execution.

Validation categories:

- Schema errors.
- Missing files.
- Unknown atom names.
- Invalid atom config fields.
- Port mismatches.
- Parameter range errors.
- Disconnected required ports.
- Cycles where unsupported.
- Backend compatibility warnings.

**Acceptance criteria**

- CLI validation returns non-zero exit code on invalid files.
- Web inspector shows exact node/field causing an error.
- Errors include stable codes, messages, and paths.

---

### FR-06 — Provide an atom metadata registry

Every atom must expose metadata.

Required metadata:

- Atom name.
- Category.
- Input ports.
- Output ports.
- Config fields.
- Parameter/state size.
- Flags: stateless/stateful, realtime-safe, M7-compatible, WASM-compatible.
- Process function pointer.

**Acceptance criteria**

- The graph compiler can validate atom usage without executing atoms.
- UI can show atom names, ports, and config fields from metadata.
- Backend compatibility can be computed from atom flags.

---

### FR-07 — Replace fixed chunk size with runtime process info

All atoms must process `frames` from runtime metadata instead of hardcoded `CHUNK_LENGTH`.

Required process metadata:

```c
typedef struct {
  float sample_rate;
  uint32_t frames;
  uint32_t channels;
} apg_process_info_t;
```

**Acceptance criteria**

- Atoms work with 64, 128, 256, 512, and 1024 frame blocks where valid.
- No atom assumes a compile-time block size unless explicitly marked as a fixed-size spectral atom.
- Fixed-size atoms declare their block-size requirement in metadata.

---

### FR-08 — Enforce realtime-safe processing

The audio callback must be deterministic and realtime-safe.

Requirements:

- No heap allocation in `process()`.
- No YAML parsing in audio thread.
- No locks in audio thread.
- No file I/O in audio thread.
- No logging from audio thread except optional lock-free diagnostic counters.
- No string lookup in audio thread.
- State memory must be preallocated.

**Acceptance criteria**

- Static analysis or code review checklist covers every atom.
- Runtime process callback can run without dynamic allocation.
- Hot graph changes are prepared outside the callback.

---

### FR-09 — Support parameter binding and smoothing

The runtime must support safe live parameter changes.

Required behavior:

- UI/control thread sends parameter changes to a realtime-safe queue or double-buffered control state.
- Audio thread consumes pending changes at block boundary or sample-accurate timestamp where supported.
- Parameters can define smoothing time.

**Acceptance criteria**

- Changing `drive`, `tone`, or `level` during playback does not produce zipper noise.
- Parameter updates do not allocate or lock in audio thread.
- Parameter metadata supports min, max, default, unit, scale, and smoothing.

---

### FR-10 — Support safe graph hot-swap

Routing and structural graph changes must compile outside the audio callback.

Required behavior:

```text
UI graph edit
  → compile new graph
  → validate
  → allocate/prepare memory
  → atomic swap at buffer boundary
```

**Acceptance criteria**

- Invalid graph edits do not replace the active graph.
- Valid graph swaps occur at buffer boundary.
- Old graph memory is released only after it is no longer used by the audio thread.

---

### FR-11 — Provide metering

The system must expose safe audio level and diagnostic data to UI.

Required meters:

- Peak.
- RMS.
- CPU estimate.
- Buffer underrun/xrun count where backend supports it.
- Graph latency estimate.

**Acceptance criteria**

- Meter updates use UI-safe queue or polling snapshot.
- Metering does not block audio processing.

---

### FR-12 — Support bypass, mute, and solo

Pedalboard workflow requires basic live controls.

**Acceptance criteria**

- Each unit node can be bypassed.
- Project-level mute and solo are supported.
- Bypass behavior is click-safe with optional crossfade.

---

### FR-13 — Provide CLI tools

Required CLI commands:

```bash
apg validate units/overdrive.yaml
apg validate projects/guitar-board.yaml
apg render projects/guitar-board.yaml input.wav output.wav
apg benchmark projects/guitar-board.yaml
apg export --target m7 projects/guitar-board.yaml build/m7_bundle/
```

**Acceptance criteria**

- CLI validation is usable in CI.
- Offline render is deterministic for identical inputs.
- Benchmark reports CPU estimate, memory estimate, and compatibility profile.

---

### FR-14 — Provide Web app v2 screens

Required screens:

| Screen | Requirement |
|---|---|
| Project browser | Create, open, import, export, autosave projects |
| Pedalboard canvas | Drag units, connect routes, reorder chains |
| Unit editor | Edit atom-level graph inside a unit |
| Parameter panel | Edit knobs/sliders and param mappings |
| Live preview | Run compiled graph through same engine path |
| Inspector | Show schema, graph, CPU, memory, and compatibility issues |
| Export panel | Export desktop, web, and embedded bundles |

**Acceptance criteria**

- User can create a project with overdrive + delay + tremolo without manually editing all YAML.
- Web app autosaves edits locally.
- Validation errors are visible before runtime execution.

---

### FR-15 — Support runtime adapters

`apgcore` must stay independent of platform APIs. Platform-specific runtime code must live in adapters.

Required adapters:

- Desktop runtime.
- Browser/WASM AudioWorklet runtime.
- React Native C++ control runtime.
- Cortex M7 static runtime.
- CLI/offline runtime.

**Acceptance criteria**

- `apgcore` builds without PipeWire, browser APIs, React Native, or UI dependencies.
- Each adapter calls the same graph compile/process API.

---

### FR-16 — Support backend compatibility profiles

Each graph must be checked against target backend capabilities.

Required profiles:

| Profile | Purpose |
|---|---|
| `desktop_full` | Most complete profile, supports dynamic projects and broad atom set |
| `wasm_realtime` | Browser-safe profile for AudioWorklet/WASM |
| `m7_static` | Embedded static-memory profile with restricted atom subset |
| `offline_render` | Deterministic non-realtime rendering profile |

**Acceptance criteria**

- Compatibility matrix shows whether each unit runs on Desktop, Web, React Native, and M7.
- Unsupported atoms or graph features produce warnings/errors before export.

---

## 8. Nonfunctional Requirements

### NFR-01 — Reliability

- Audio processing must avoid glitches under expected CPU load.
- Invalid graph edits must never crash active audio playback.
- Runtime must fail safe on null buffers or invalid state.

### NFR-02 — Performance

- Audio callback must avoid allocations and locks.
- Graph execution must use precomputed schedules and pointer bindings.
- Memory reuse should minimize temporary buffers.

### NFR-03 — Portability

- `apgcore` must compile as portable C/C++ with minimal dependencies.
- Platform features must live in adapters.
- Backend-specific optimized implementations must preserve the public atom ABI.

### NFR-04 — Scalability

- Graph compiler must support many units and nested reusable units.
- Project format must support multiple files and future library sharing.
- Atom registry must be extensible without rewriting compiler logic.

### NFR-05 — Security

- YAML/project loading must validate all file paths and schemas.
- Web app must not execute arbitrary native code from project files.
- Embedded export must avoid runtime file parsing.
- Project imports should be treated as untrusted until validated.

### NFR-06 — Accessibility and Usability

- UI controls must be keyboard accessible.
- Parameter controls must expose labels, values, units, and ranges.
- Visual graph errors must also be available as text in the inspector.
- Users should be able to build a basic pedalboard without understanding internal atom graphs.

### NFR-07 — Testability

- Atoms must have unit tests.
- Graph compiler must have schema, routing, cycle, and memory-plan tests.
- Offline render must support golden WAV/regression tests.
- Platform adapters must have smoke tests.

---

## 9. Success Metrics

| Metric | Target for MVP |
|---|---|
| Existing overdrive migration | v1 overdrive can be represented as v2 unit |
| Validation coverage | Invalid schemas, missing atoms, cycles, and port mismatches detected |
| Runtime safety | Audio callback uses no heap allocation or locks |
| Live parameter update | Knob changes are click-safe and smoothed |
| Graph hot-swap | Valid graph swaps at buffer boundary without stopping audio |
| CLI render determinism | Same input/project produces same output across repeated runs on same backend |
| Web preview | Browser can load and play a simple pedalboard through WASM path |
| M7 export | Restricted pedalboard exports to static graph bundle |
| Compatibility matrix | Per-unit target support is visible before export |

---

## 10. MVP Acceptance Criteria

The v2 MVP is accepted when all of the following are true:

1. `apgcore` is separated from platform runtimes.
2. Atom process functions use runtime `frames` metadata instead of fixed `CHUNK_LENGTH` for MVP-supported atoms.
3. `unit.v2.yaml` and `project.v2.yaml` schemas exist and are validated by CLI.
4. Graph compiler emits numeric node graph, topological schedule, memory plan, and process plan.
5. CLI can validate and offline-render a simple guitar pedalboard project.
6. Web app can load multiple YAML files, autosave them, and show validation errors.
7. Web app can run a compiled graph through the live engine path.
8. Linux runtime supports live parameter changes and graph hot-swap.
9. Cortex M7 export supports a restricted atom subset with static memory plan.
10. At least these first validation units are supported: overdrive, delay, tremolo, EQ/tone stack, noise gate, and wet/dry mix.

---

## 11. First Validation Product Slice

Use **guitar pedalboard effects** as the first product slice.

| Unit | Why it matters |
|---|---|
| Overdrive | Already exists and validates v1 → v2 migration |
| Delay | Tests state, buffers, and feedback |
| Tremolo | Tests LFO modulation and parameter smoothing |
| EQ / tone stack | Tests filter stability and UI controls |
| Noise gate | Tests detect atoms and control behavior |
| Wet/dry mix | Tests routing, mix atoms, and UX |

This slice gives high technical coverage without requiring a full DAW.

---

## 12. Review Gates

Because the preferred workflow requires review before moving to the next step, the upgrade should proceed through these gates:

1. Approve requirements and design direction.
2. Review reference products and target user workflow.
3. Define v2 unit/project parameters and schema boundaries.
4. Review Mermaid architecture and graph compiler diagrams.
5. Create sample `unit.v2.yaml` files.
6. Fine-tune atom mappings, compatibility profiles, and UI metadata.
7. Collect feedback from validation projects.

---

## 13. Open Decisions

| Decision | Options | Recommendation |
|---|---|---|
| Project file name | `project.yaml`, `chain.yaml`, both | Support `project.v2.yaml`; allow `chain.v2.yaml` alias for pedalboards |
| First desktop backend | PipeWire, JACK, JUCE adapter | PipeWire first for Linux; keep runtime interface generic |
| First browser runtime | Main thread WASM, AudioWorklet WASM | AudioWorklet WASM |
| First embedded profile | Full atom catalog, restricted atom subset | Restricted atom subset |
| Migration strategy | Hard break, compatibility parser | Compatibility parser for v1 unit files |
| Unit library scope | Built-in only, sharing, marketplace | Built-in library first; sharing later |
