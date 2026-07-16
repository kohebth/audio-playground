# Atom Cost Model

## Status

The core exposes deterministic normalized estimates through `apg.cost_model.v1`. Cost values are relative Audio Playground Cost Units (ACU); they are not elapsed time or real-time guarantees.

## Dimensions

Each atom estimate reports:

- `cpu_acu`: normalized computational work for one invocation.
- `persistent_bytes`: state structure and registry-declared persistent buffers.
- `scratch_bytes`: temporary workspace required by the algorithm.
- `latency_frames`: algorithmic latency, separate from CPU work.
- `cost_class`: `trivial`, `low`, `medium`, `high`, or `extreme`.

## APIs

```c
bool apg_atom_estimate_cost(
    const atom_registry_entry_t *entry,
    const void *config,
    const apg_process_context_t *process_context,
    const apg_spectral_info_t *spectral_info,
    apg_atom_cost_result_t *out
);

bool apg_graph_estimate_cost(
    const atom_registry_entry_t *const *entries,
    const void *const *configs,
    const apg_spectral_info_t *const *spectral_infos,
    size_t count,
    const apg_process_context_t *process_context,
    apg_graph_cost_result_t *out
);

bool apg_compiled_unit_estimate_cost(
    const apg_v2_compiled_unit_t *compiled,
    const apg_process_context_t *process_context,
    apg_graph_cost_result_t *out
);
```

The compiled-unit estimator follows execution schedule order and honors compiled spectral contexts. It currently uses conservative defaults for configuration-dependent atoms because compiled configuration storage is materialized by registry/runtime construction.

## Aggregation

- CPU work is summed across scheduled atoms.
- Persistent memory is summed.
- Scratch memory is the maximum per-atom scratch requirement.
- Schedule-only latency is conservatively summed.

Topology-aware graph latency must eventually use the longest input-to-output path rather than the conservative schedule sum.

## Calibration

ACU must be calibrated separately for each target class:

- native desktop
- desktop WASM
- mobile WASM
- Cortex-M7

Calibration maps ACU to measured execution time or cycles. Admission control must include a target-specific safety factor and measured p99/max callback latency.

## Restrictions

The model must not be presented as guaranteed latency. Real-time approval still requires measured callback duration, deadline misses, jitter, allocation checks, and target-specific benchmarks.
