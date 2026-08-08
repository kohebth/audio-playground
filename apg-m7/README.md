# APG-M7 (`apg-m7`) - STM32F729 Embedded Audio Firmware Project

`apg-m7` is the embedded C11 firmware backbone for running `apg-core` on Cortex-M7 hardware (specifically configured for **STM32F729** @ 216 MHz).

## Domain Architecture

The firmware is structured into clean modular domain layers:

```
apg-m7/
├── include/apg_m7/
│   ├── system_config.h           # Central hardware parameters, memory limits, clocks, & sample rate
│   └── domain/
│       ├── bsp_hardware.h        # STM32F729 RCC Clocks, SAI/I2S, DMA & SCB Cache interfaces
│       ├── usb_host.h            # USB OTG High-Speed Host controller & storage event notifications
│       ├── preset_fs.h           # FatFS file system reader for loading USB YAML presets
│       └── audio_engine.h        # DMA audio callback bridge and APGCore engine manager
├── src/
│   ├── bsp/bsp_hardware.c        # Hardware abstraction implementation for STM32F729
│   ├── usb/usb_host.c            # USB Host MSC driver state machine
│   ├── fs/preset_fs.c            # FatFS volume mount & preset loader
│   ├── audio/audio_engine.c      # Real-time DMA audio buffer callback & engine execution
│   └── main.c                    # Firmware startup, initialization & main loop
├── test/
│   └── test_m7_firmware_backbone.c # Native host unit test suite for firmware lifecycle & preset loading
└── stm32f729_m7.ld               # Memory linker script mapping ITCM, DTCM, SRAM1, SRAM2 sections
```

## System Parameters & Hardware Configuration

Edit [`include/apg_m7/system_config.h`](file:///home/duync/repo/audio-playground/apg-m7/include/apg_m7/system_config.h) to adjust core hardware parameters:
- **System Frequency**: 216 MHz (`APG_M7_SYSCLK_FREQ_HZ`)
- **Audio Rate & Block Size**: 48,000 Hz, 64 frames (`APG_M7_AUDIO_SAMPLE_RATE_HZ`, `APG_M7_AUDIO_BLOCK_FRAMES`)
- **Cache Alignment**: 32 bytes (`APG_M7_CACHE_LINE_BYTES`)
- **USB Preset Path**: `0:/presets/active.project.v2.yaml` (`APG_M7_ACTIVE_PRESET_FILE`)

## Building & Verification

Run the dedicated test script:

```sh
./scripts/apg-m7.sh
```

Or build via CTest:

```sh
cmake --build build/native --target test_m7_firmware_backbone
ctest --test-dir build/native -R '^test_m7_firmware_backbone$' --output-on-failure
```
