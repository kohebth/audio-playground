# Live Microphone Latency Profiler

## Goal

Identify whether live microphone latency pressure comes from browser callback scheduling, Worklet data movement, WASM
graph execution, or opaque browser/device buffering without adding tracing dependencies to APGCore.

## Completed

- [x] Export versioned audio-trace snapshot, report, stage-statistics, and status types from `wasm-tools`.
- [x] Add Worklet start/poll commands and facade methods.
- [x] Record detailed stages every eighth callback into fixed-capacity preallocated buffers for five seconds.
- [x] Keep callback deadline timing on every quantum and construct statistics only from the message handler.
- [x] Show progress, estimates, stage timing, callback health, and the slowest internal stage in Developer Diagnostics.
- [x] Distinguish internal overruns, browser scheduling delay, and healthy internal processing.
- [x] Export timestamped `apg.audio-trace.v1` JSON.
- [x] Cover statistics, completion, reset, sample bounds, browser profiling, export, audio health, and stop cleanup.

## Boundary

Per-unit and per-atom instrumentation remains deferred unless a real-device report proves that WASM graph processing
dominates the callback. APGCore, its ABI, and embedded builds are unchanged.
