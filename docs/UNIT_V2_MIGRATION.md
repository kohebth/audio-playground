# Unit v2 Migration Path

This document defines the historical path for moving selected v1 `units/*.unit.yaml` graphs to `units-v2/*.unit.v2.yaml` fixtures. The production path is now APGCore v2; remaining files under `units/` are legacy local drafts and are not loaded by the default build.

## Migration Order

Start with deterministic, mono, runtime-covered units before broad host rollout:

1. **Scalar gain/control chains**: model v1 gain stages with `generation_dc` plus `amplitude_multiply`, following `units-v2/simple_gain.unit.v2.yaml` and `units-v2/control_gain.unit.v2.yaml`.
2. **Simple routing/mix chains**: migrate add, wet/dry, pan, and matrix routing using `simple_mix`, `stereo_pan`, and inline `mix_matrix` runtime coverage.
3. **Stateful delay/filter blocks**: migrate short, bounded state examples with `delay_line_state`, `filter_comb_ff_state`, and reset coverage.
4. **Modulation state blocks**: migrate only deterministic modulation paths first, following `modulation_frequency_state`.

Defer large reverb, cabinet, amp, and generated-audio chains until their v2 graphs can be validated against representative regression checks.

## Mechanical Mapping

For each legacy unit selected for preservation:

- Convert public `params` to v2 typed params with numeric bounds.
- Convert external audio/control names to `ports.inputs` and `ports.outputs`.
- Enumerate every graph buffer under `graph.signals`; do not rely on implicit signal names except mono public ports.
- Convert each v1 step to a v2 node with explicit `in`, `out`, and `config` bindings.
- Prefer `${params.name}` for user controls and literals for fixed constants.
- Keep fixtures small: a migrated fixture should compile and process in CTest without external audio.

## Acceptance Criteria

A migrated fixture is complete when it loads, compiles, runs through the v2 runtime or host bridge, has deterministic sample assertions, and is listed in `units-v2/` with no generated audio dependency.

## Current Status

Clean tracked v1 fixture files have been removed. Remaining modified/untracked `units/` files were removed as PA5b11b cleanup, and future migration work now lands directly in `units-v2/`.
