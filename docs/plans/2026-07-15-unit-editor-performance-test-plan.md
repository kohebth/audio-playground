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
| Interaction frame rate | `>= 50 FPS` | Scheduled 500-atom pointer drag/pan/zoom average-frame gate |
| Runtime parameter control | `< 50 ms` | Real Worklet round trip during continuous knob drag |
| Audio underruns from UI | `0` | Zero DSP underrun and callback-deadline-miss increase during live editing |

The project import checks measure from browser file import through the expected rendered React Flow node count. Contract
checks use the median of three opens, including the cold first open, through registering every actual atom and mounting
the visible viewport subset. The 1,000-atom boundary is scheduled-only.

## Fixture Matrix

- [x] Deterministic 5/20/50/100-unit project fixtures.
- [x] Linear, branching, highly connected, reused-unit, payload, and invalid variants.
- [x] Target route counts of 30/140/700/1,500 where the topology supports them.
- [x] Large parameter payload and repeated unit instances.
- [x] Executable unit fixtures containing 25/100/500/1,000 actual atom nodes.
- [x] Contract-canvas fixtures for linear, branched, dense, payload-heavy, and invalid atom graphs.

## Implementation Phases

### Phase 1: Instrumentation

- [x] User Timing spans around project and unit graph operations.
- [x] React Profiler samples for project canvas, contract canvas, and inspector.
- [x] Deterministic fixture generator and size metadata.
- [x] Pure-operation and Chromium collected-heap reporting.
- [x] Async operations remain open until their promises settle.
- [x] Per-node render counters and selection/parameter render-scope assertions.
- [x] Atom metadata lookup is memoized by catalog revision in the inspector.
- [x] Per-edge render counters.
- [x] Centralized workspace state-dispatch counters; the editor has no external subscription store.
- [x] Scheduled 500-atom frame-interval and long-task collection.
- [x] Scheduled DevTools style, layout, paint, screenshot, and user-timing trace collection for 500-atom interaction.
- [x] Worker validation/prepare, Worklet commit/control/meter round-trip marks, and runtime resource snapshots.

### Phase 2: Baseline

- [x] Stored pure-operation baseline for generated fixture families.
- [x] Small through extreme browser import budget checks.
- [x] Persisted browser baseline with machine/profile metadata.
- [x] Normalized browser report ranking the three most expensive measured UI interactions per profile.
- [x] Typical-laptop 4x CPU baseline and low-end 6x CPU baseline.

The checked-in `scripts/perf-ui-browser-baseline.json` seeds the development, 4x, and 6x profiles with CPU, memory,
Node, browser, throttle, and viewport metadata. It is informational rather than a pull-request gate. Scheduled CI keeps
the latest 12 samples per profile in a restored cache, computes rolling medians, and emits a self-contained HTML chart;
that rolling history is the representative baseline as runner data accumulates.

### Phase 3: Automated Editing Tests

- [x] Pure project/unit parse and mutation benchmarks.
- [x] Native HTML drag/drop of a project unit.
- [x] Project add/remove, route create/delete, parameter, inspector, undo, and redo checks.
- [x] Autosave debounce and synchronous blocking budget.
- [x] Bounded residual heap growth after repeated add/remove.
- [x] Scheduled 20x100 atom mount/remove retention gate after saturating bounded undo history.
- [x] Pointer-event node drag while 500 atoms are mounted.
- [x] Invalid drop, Escape cancel, rapid drop, zoom/pan drop, and filtered catalog.
- [x] Atom and project-unit edge drops with explicit split-and-reconnect transactions and single-step undo.
- [x] Unit drag/drop with a referenced 500-atom payload, without expanding internals on the project canvas.
- [x] Explicit medium-graph replacement, replacement undo/redo, and post-replacement config isolation.
- [x] Malformed import proving legacy `fn` cannot mutate the immutable `atom` type.
- [x] One hundred inspector open/close cycles and isolated atom-ID rename with undo/redo.
- [x] Reconnect transaction and render-scope coverage.
- [x] Selection and parameter edits do not rerender unrelated project or contract nodes.
- [x] Assertions that drag, topology, and YAML metadata edits do not rerender unrelated nodes or edges.
- [x] Slow and failing storage, including last-good-snapshot retention and visible failure state.
- [x] Scheduled one-hour autosave emulation with exact write count and saturated-history heap gate.
- [x] Scheduled 30-second parameter burst, continuous node drag, 100-atom add burst, repeated routing, and 500-atom
  payload autosave gates with payload size, duplicate-write, and main-thread blocking assertions.
- [x] Scheduled matched-window lifecycle heap gate covering 1,000 inspector cycles, repeated route edges, atom
  replacements, project load/unload, and project/contract navigation.

### Phase 4: Live Runtime

- [x] Add atom/unit, reconnect, replacement, undo/redo, save, and import while audio runs.
- [x] Separate UI mutation latency from worker compile/prepare and Worklet commit latency.
- [x] Assert zero meter underruns and zero callback deadline misses.
- [x] Rapid Worklet parameter-control latency under `50 ms`, with per-path diffing and in-flight coalescing.
- [x] Repeated file/microphone start-stop ownership checks for the retained AudioContext/Worker and transient Worklet,
  request ports, streams, source nodes, and polling timers.

### Phase 5: Regression Gates

- [x] Pull-request pure-operation threshold and baseline regression gate.
- [x] Fixed threshold plus percentage/absolute baseline regression policy.
- [x] Stable Chromium drag/drop, medium-fixture graph-load, and replacement checks on pull requests.
- [x] Weekly 500/1,000-atom, long-session, continuous-drag, live-audio, Chromium 4x/6x, mobile 6x,
  Chromium 256 MB, Firefox, and WebKit runs.
- [x] Historical Playwright JSON, normalized profile/runner/commit records, rolling 12-sample baselines, and HTML trend
  charts with 90-day artifact retention.

## Primary Success Criteria Audit

1. **500-node graph remains usable:** Proven for the required interaction set. The browser registers the real 500-atom
   graph under 2 seconds, virtualizes offscreen nodes, and passes pointer drag, pan, zoom, configuration editing, and
   `>= 50 FPS` average-frame gates.
2. **Local changes avoid full graph rerender:** Proven for the required interaction set. Exact node/edge and centralized
   state-dispatch counters keep selection, parameter editing, 500-atom drag/config, project drag/drop, route topology,
   atom insertion/reconnect, and raw YAML metadata edits limited to the affected graph objects.
3. **Autosave has no visible stall:** Proven across rapid default edits, 120 parameter edits over a simulated 30 seconds,
   an 80-step continuous node drag, 20 route add/delete cycles, 100 rapid atom additions, a real 500-atom payload,
   an emulated one-hour session, and injected slow/failing storage. Persistence remains under 16 ms, duplicate snapshots
   are rejected, and the last good snapshot survives failures.
4. **Removed objects are not retained:** Proven. Collected-heap browser gates stay below 10% across saturated-history
   project add/remove windows, 20 cycles that mount/remove 100 atoms, and matched lifecycle windows containing 1,000
   inspector cycles plus repeated edge, replacement, reload, and navigation operations. Twenty alternating file/microphone
   playback cycles also balance Worklet starts/stops and release request, stream, source-node, and timer resources.
5. **Editing during audio causes no underrun:** Proven for the scheduled structural workflow. Project-unit and atom
   insertion, atom replacement, reconnect, undo/redo, save, and a 100-atom import each hot-swap while fake microphone
   audio remains running, revisions converge, and neither the meter underrun nor measured Worklet callback-deadline-miss
   count increases.
6. **Explicit replacement stays controlled:** Proven. Pure transformer tests cover compatibility planning, the browser
   gate keeps medium-graph replacement under 300 ms, only the replaced node rerenders, undo/redo restore both types, and
   subsequent configuration editing remains isolated.

## Verification Commands

```sh
cd web-tools/unit-editor
npm run perf:benchmark:regression
npm run perf:ui:pr
npm run perf:ui -- --reporter=line --workers=1
APG_BROWSER_MATRIX=1 APG_CPU_THROTTLE=4 npm run perf:ui -- --project=chromium --grep @browser-matrix --workers=1
APG_BROWSER_MATRIX=1 APG_CPU_THROTTLE=6 npm run perf:ui -- --project=chromium-mobile --grep @browser-matrix --workers=1
APG_BROWSER_MATRIX=1 APG_MEMORY_LIMIT_MB=256 npm run perf:ui -- --project=chromium --grep @browser-matrix --workers=1
npm run perf:ui:baseline -- --input <normalized-report.json> --output <baseline.json> --chart <trend.html>
npm run test
npm run lint
npm run build
```
