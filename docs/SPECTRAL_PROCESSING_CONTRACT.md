# Spectral Processing Contract

## Status

This document defines the production contract for Audio Playground spectral atoms. FFT, IFFT, multiply, window, and
overlap processing use this contract on desktop. FFT/IFFT remain experimental and are rejected for WASM and Cortex-M7
until target-specific transform implementations and memory accounting are complete.

## Processing context

Spectral atoms use an immutable context prepared by the compiler and stored in registry-owned memory:

```c
typedef struct {
    uint32_t fft_size;
    uint32_t bin_count;
    uint32_t hop_size;
} apg_spectral_info_t;
```

The runtime exposes a stable pointer to this context through `atom_call_t`. The pointer must not reference a control-thread
temporary. Atom processing never allocates, resizes, or constructs transform plans.

Supported production transform sizes are `256`, `512`, `1024`, and `2048`. For a real transform of size `N`:

- `fft_size` is `N`.
- `bin_count` is `N / 2 + 1`.
- `hop_size` is in `1..N` for streaming overlap atoms and `N` for standalone FFT/IFFT calls.

The compiler rejects other sizes and inconsistent derived values before runtime creation. Legacy `block_size` and
`hop_size` configuration fields remain accepted during v2 migration, but the compiled context is authoritative.

The contract terms have these exact meanings:

- `time_frames` is `fft_size` for complete transform/window frames and `hop_size` for each streaming overlap input/output.
- `window_size` is `fft_size`; shorter or longer analysis windows are not supported by this version.
- `output_frames` is `fft_size` for time-frame output, `hop_size` for overlap-add output, and is not used as a bin count.
- `latency_frames` is `fft_size - hop_size` for streaming overlap stages and zero for standalone complete-frame stages.

## Data layout and normalization

- Time-domain inputs and outputs are contiguous `float` arrays.
- Complex spectra use separate contiguous `real` and `imag` arrays.
- Valid bins are `0..bin_count - 1`, representing DC through Nyquist.
- FFT input is exactly `fft_size` real samples. FFT output is exactly `bin_count` complex bins.
- FFT is unscaled.
- IFFT consumes `bin_count` bins, reconstructs the conjugate half internally, and emits exactly `fft_size` real samples.
- IFFT applies `1 / fft_size` normalization, so an FFT/IFFT round trip reproduces the input within floating-point tolerance.
- `freq_multiply` consumes and emits exactly `bin_count` bins and performs element-wise complex multiplication.
- Non-finite input samples or bins are treated as zero. Outputs and persistent state must remain finite.

## Frame and latency semantics

Generic `apg_process_info_t.frames` remains the valid time-domain input count for the host block. It does not represent a
bin count. Spectral array bounds always come from `apg_spectral_info_t`.

| Atom | Input | Output | Algorithmic latency |
| --- | --- | --- | --- |
| `freq_window` | `fft_size` time samples | `fft_size` time samples | 0 |
| `freq_fft` | `fft_size` time samples | `bin_count` complex bins | 0 after a complete frame is available |
| `freq_multiply` | `bin_count` complex bins per operand | `bin_count` complex bins | 0 |
| `freq_ifft` | `bin_count` complex bins | `fft_size` time samples | 0 after a complete spectrum is available |
| `freq_overlap_save` | `hop_size` new time samples | one `fft_size` time frame | `fft_size - hop_size` warm-up samples |
| `freq_overlap_add` | one `fft_size` time frame | `hop_size` time samples | `fft_size - hop_size` samples |

`freq_overlap_save` retains the preceding `fft_size - hop_size` samples. `freq_overlap_add` accumulates a complete frame,
emits the oldest `hop_size` samples, shifts the remaining tail, and zeroes the vacated region.

`freq_shift` is currently a time-domain dual-delay pitch shifter despite its name. It continues to use
`apg_process_info_t`, remains experimental, and is not part of this FFT context migration.

## Failure behavior

Processing returns without access when required structures or buffers are null. Invalid compiled context is a runtime
contract violation; migrated atoms must safely zero only their declared output extent and clear affected persistent state.
They must never guess array capacity from `block_size`, write beyond the context extent, allocate memory, or perform
unbounded work.

## Target policy

- Desktop FFT/IFFT use registry-owned, preallocated scratch memory with the supported sizes above.
- WASM uses only spectral atoms carrying `APG_ATOM_WASM_SAFE`; FFT/IFFT remain rejected.
- Cortex-M7 FFT/IFFT will use CMSIS-DSP with statically declared plan and scratch requirements; they remain rejected now.
- An atom remains rejected for a target until its implementation, plan storage, numerical tests, and memory accounting for
  that target are complete.
