# Chorus - BOSS CE-2 Style

This unit implements a classic chorus effect modeled after the BOSS CE-2, utilizing a modulated delay line (simulated Bucket Brigade Delay) and a sine LFO.

## 📦 DSP Unit Definition
```yaml
# chorus_boss_ce2.unit.yaml

id:      "a1b2c3d4-0000-0000-0000-000000000002"
name:    chorus_ce2
version: 1.0.0

params:
  rate:   { min: 0.1,  max: 10.0, default: 0.5, unit: Hz }
  depth:  { min: 0.0,  max: 1.0,  default: 0.5, unit: scalar }

pipeline:
  - id:       osc
    fn:       generation_lfo
    args:
      frequency:   ${params.rate}
      waveform:    sine
      sample_rate: ${context.sample_rate}
    out:      lfo_out

  - id:       modulate
    fn:       modulation_phase
    args:
      signal:      ${input}
      modulator:   ${lfo_out}
      depth:       ${params.depth} * 20.0  # Sweep range in samples
    out:      wet_signal

  - id:       blend
    fn:       mix_wet_dry
    args:
      dry:  ${input}
      wet:  ${wet_signal}
      mix:  0.5
    out:      ${output}
```

## 📜 Boss CE-2 Characteristics
- **BBD Delay**: Typical delay time ranges from ~5ms to ~30ms.
- **LFO**: Sine wave modulation.
- **Mix**: Fixed 50/50 dry/wet for that classic thick chorus sound.
