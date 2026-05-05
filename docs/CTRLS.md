# Control Layer

`ctrl` sits between an interface and the fixed DSP core:

```text
interface -> ctrl -> runtime/unit core -> atoms
```

The atom layer stays fixed. The control layer owns parameter transitions.
It smooths target values over time and writes the smoothed values into the
already-instantiated unit config fields that were declared as `${params.name}`
in the unit YAML.

Typical usage:

```c
runtime_context_t ctx = {.sample_rate = 48000, .chunk_length = 512};
runtime_unit_t *unit = runtime_unit_load("units/marshall_plexi_head_amp.unit.yaml", ctx);

ctrl_unit_t ctrl;
ctrl_unit_init(&ctrl, unit, "units/marshall_plexi_head_amp.unit.yaml");
ctrl_unit_set_smoothing_ms(&ctrl, "presence", 45.0f);
ctrl_unit_set_target(&ctrl, "presence", 0.8f);

ctrl_unit_process(&ctrl, input, output);
```

For a parameter to be controllable, bind it in unit YAML through an atom config:

```yaml
params:
  treble:
    default: 0.5

pipeline:
  - id: treble_amount
    fn: generation_dc
    out:
      signal: treble_gain
    config:
      value: ${params.treble}
```

The control layer will discover that binding and update the `value` field
smoothly while the unit runs.
