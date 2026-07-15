# Unit Editor Performance Test Plan

## Objective

Implement the performance program for drag-and-drop editing, immutable atom mutation, rendering scalability, autosave,
memory stability, and live WASM audio. The source requirements are the July 15 performance plan supplied by the user.

## Budgets

| Area | Budget | Current gate |
| --- | ---: | --- |
| Drag feedback | `< 16 ms` | Browser marks for drag start and drag over |
| Node drop | `< 100 ms` | Browser drop and project mutation marks |
| Inspector open | `< 100 ms` | Selection and inspector-switch marks |
| Parameter edit | `< 50 ms` | Knob mutation mark |
| Undo/redo | `< 100 ms` | Browser marks |
| Autosave blocking | `< 16 ms` | Browser serialization/storage mark |
| 100-node/atom load | `< 500 ms` | 100-atom contract canvas and 20-unit project import |
| 500-node/atom load | `< 2 s` | 500-atom contract canvas and 50-unit project import |
| Residual edit memory | `< 10%` | Chromium collected-heap gate after bounded-history warm-up |
| Interaction frame rate | `>= 50 FPS` | Not implemented |
| Runtime parameter control | `< 50 ms` | UI mutation only; Worklet round-trip not implemented |
| Audio underruns from UI | `0` | Not implemented |

The project import checks measure from browser file import through the expected rendered React Flow node count. Contract
checks use the median of three opens, including the cold first open, through mounting every actual atom node. The
1,000-atom boundary is scheduled-only.

## Fixture Matrix

- [x] Deterministic 5/20/50/100-unit project fixtures.
- [x] Linear, branching, highly connected, reused-unit, payload, and invalid variants.
- [x] Target route counts of 30/140/700/1,500 where the topology supports them.
- [x] Large parameter payload and repeated unit instances.
- [x] Executable unit fixtures containing 25/100/500/1,000 actual atom nodes.
- [ ] Contract-canvas fixtures for linear, branched, dense, payload, and invalid atom graphs.

## Implementation Phases

### Phase 1: Instrumentation

- [x] User Timing spans around project and unit graph operations.
- [x] React Profiler samples for project canvas, contract canvas, and inspector.
- [x] Deterministic fixture generator and size metadata.
- [x] Pure-operation and Chromium collected-heap reporting.
- [x] Async operations remain open until their promises settle.
- [x] Per-node render counters and selection/parameter render-scope assertions.
- [ ] Per-edge render counters and store notification counters.
- [ ] Long-task, dropped-frame, style, layout, and paint collection.
- [ ] Worker compile/swap timing and Worklet control round-trip marks.

### Phase 2: Baseline

- [x] Stored pure-operation baseline for generated fixture families.
- [x] Small through extreme browser import budget checks.
- [ ] Persisted browser baseline with machine/profile metadata.
- [ ] Ranked report of the three most expensive UI interactions.
- [ ] Typical-laptop 4x CPU baseline and low-end 6x CPU baseline.

### Phase 3: Automated Editing Tests

- [x] Pure project/unit parse and mutation benchmarks.
- [x] Native HTML drag/drop of a project unit.
- [x] Project add/remove, route create/delete, parameter, inspector, undo, and redo checks.
- [x] Autosave debounce and synchronous blocking budget.
- [x] Bounded residual heap growth after repeated add/remove.
- [ ] Pointer-event drag tests, invalid drop, Escape cancel, rapid drop, zoom/pan drop, edge drop, and filtered catalog.
- [ ] Unit drag/drop with large atom payloads.
- [ ] Inspector open/close loops, reconnect, rename, explicit replacement, and replacement undo/redo.
- [ ] Malformed import proving atom `fn` cannot mutate except through explicit replacement.
- [x] Selection and parameter edits do not rerender unrelated project or contract nodes.
- [ ] Assertions that drag, topology, and YAML edits do not rerender unrelated nodes or the full canvas.
- [ ] Slow/failing storage and one-hour autosave scenarios.

### Phase 4: Live Runtime

- [ ] Add atom/unit, reconnect, replacement, undo/redo, save, and import while audio runs.
- [ ] Separate UI commit latency from compile/prepare/commit latency.
- [ ] Assert zero meter underruns and zero callback deadline misses.
- [ ] Rapid Worklet parameter-control latency under `50 ms`.
- [ ] Start/stop leak checks for AudioContexts, Worklets, Workers, ports, streams, and timers.

### Phase 5: Regression Gates

- [x] Pull-request pure-operation threshold and baseline regression gate.
- [x] Fixed threshold plus percentage/absolute baseline regression policy.
- [ ] Stable medium-fixture browser checks on pull requests.
- [ ] Scheduled 500/1,000-atom, long-session, continuous-drag, live-audio, browser-matrix, and throttled runs.
- [ ] Historical JSON browser results and trend artifact generation.

## Primary Success Criteria Audit

1. **500-node graph remains usable:** Partially proven. The browser mounts the real 500-atom contract graph under 2 seconds,
   but pan/zoom/config editing and interaction FPS are not yet combined in that case.
2. **Local changes avoid full graph rerender:** Partially proven. Exact node counters show one-node selection and parameter
   edits stay local; drag, topology, YAML, edge, and store-notification scope remain.
3. **Autosave has no visible stall:** Partially proven. Default-workspace rapid edits keep synchronous persistence under
   16 ms; large payload, slow storage, drag, and long-session cases remain.
4. **Removed objects are not retained:** Partially proven. A collected-heap browser gate stays below 10% across a second
   saturated undo-history window; edges, inspectors, navigation, Worklets, and 20x100 atom cycles remain.
5. **Editing during audio causes no underrun:** Not proven.
6. **Explicit replacement stays controlled:** Partially proven by pure transformer timing and correctness tests; the
   medium contract-canvas transaction and rerender impact remain.

## Verification Commands

```sh
cd web-tools/unit-editor
npm run perf:benchmark:regression
npm run perf:ui -- --reporter=line --workers=1
npm run test
npm run lint
npm run build
```
