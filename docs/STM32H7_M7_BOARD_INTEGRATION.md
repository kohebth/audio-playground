# STM32H7/M7 Board Integration Contract

This document defines the board-side contract for running an APGCore v2 `m7_static` export in a fixed-block STM32H7 audio callback. It is not a complete board support package; it is the boundary the future BSP must satisfy.

## Scope

The host board code owns clocks, codec setup, SAI/I2S, DMA, interrupt priority, cache maintenance, and physical buffers. APGCore owns only the exported DSP image, registered runtime memory, parameter/control tables, atom state, schedule execution, and measurement snapshots.

No YAML parsing, graph validation, graph expansion, allocation planning, or schedule construction may run from the audio callback. Those steps happen on the host/tooling side before the image is compiled into firmware.

## Static Inputs

The firmware build receives:

- `apg_project_m7.h` and `apg_project_m7.c` from `apg-v2 export --target m7_static`.
- A board-local configuration that maps codec channels to exported public ports.
- Optional board memory budgets for tightly coupled RAM, SRAM, and DMA-accessible audio buffers.

The generated project must declare deterministic constants for block frames, signal count, param count, schedule count, atom storage bytes, state bytes, total static RAM, and static atom-call workload. Firmware startup must reject or fail to build projects that exceed the selected board memory map.

## Audio Callback Contract

The board callback processes exactly `APG_M7_PROJECT_BLOCK_FRAMES` frames per invocation. If the codec/DMA period differs, the board layer must adapt outside APGCore by accumulating or splitting blocks before calling the runtime.

For each block:

1. DMA completes or reaches the half-transfer boundary for one input/output buffer region.
2. The board invalidates input cache lines when buffers are cacheable.
3. The board converts or aliases interleaved codec samples into the runtime input buffers.
4. APGCore executes the prebuilt schedule once.
5. The board converts runtime output buffers back into the DMA output region.
6. The board cleans output cache lines when buffers are cacheable.

The runtime must not block, allocate, load files, lock OS primitives, print diagnostics, or call platform APIs from this path.

## DMA Ownership

DMA owns the active transfer region. The CPU may only read/write the inactive half-buffer or a completed full-buffer region. The board layer must make ownership explicit before calling APGCore and must not pass pointers to memory that DMA is currently writing.

Runtime signal, param, state, and schedule memory should not be used directly as DMA buffers unless the board proves alignment, cache policy, and sample layout compatibility. Prefer a small board-owned conversion layer at the callback boundary.

## Cache And Alignment

DMA buffers must be aligned to the board cache-line size. On STM32H7 this is typically 32 bytes. Cache maintenance must cover whole cache lines, including any padding required by the HAL or linker script.

Runtime memory should be placed in deterministic sections chosen by the board:

- fast internal RAM for hot signal/state buffers;
- DMA-visible RAM for codec transfer buffers;
- flash or const memory for schedule and immutable lookup tables.

The generated bundle must remain freestanding C11-compatible and avoid assumptions about endian-specific byte access, unaligned loads, or host `sizeof` beyond exported macros and fixed-width integer types.

## Control And Measure Boundary

UI, MIDI, footswitch, or host-control updates must write through a non-audio control boundary. The board may apply pending control changes at the start of a block, then run the schedule without locks.

Meters and diagnostics are read through the measure boundary after a completed block. The audio callback may update lightweight meter state, but host/UI code must read snapshots outside the critical DMA ownership window.

## Required Verification

An STM32H7 integration is not production-ready until these gates pass:

- generated bundle syntax-checks with an ARM/M7 compiler;
- firmware links with generated static memory placed in intended sections;
- callback runs for sustained audio without allocation or blocking calls;
- cache maintenance is proven for DMA input and output regions;
- generated atom-call workload is reviewed against the selected block period;
- measured worst-case CPU time fits the selected sample rate and block period;
- measured static RAM and stack usage fit the board budget with margin.
