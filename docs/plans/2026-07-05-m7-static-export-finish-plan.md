# M7 Static Export Finish Plan

## Goal

Generated M7 bundles are static, bounded, linkable, and board-verifiable for STM32H7/M7 production.

## Current Status

Partly production-ready; board-specific gates remain.

Done:

- M7 export emits deterministic memory manifest, generated source/header, static atom storage, atom calls, schedule runner, section attributes, and configurable block/sample/cache settings.
- Host-side generated runner, stack, object, section, ARM syntax, and static RAM gates exist.
- Unsupported target profiles and incompatible atoms are rejected.

## Remaining Implementation

- [ ] Configure and require ARM syntax/stack gates with installed `arm-none-eabi-gcc` in production runs.
- [ ] Add or receive a minimal STM32H7 linker script and enable `APG_M7_LINKER_SCRIPT`.
- [ ] Define and wire `APG_M7_BOARD_TIMING_COMMAND` for real board timing output.
- [ ] Improve export output-directory behavior with either auto-create or a clearer diagnostic; prefer clearer diagnostic unless auto-create already exists nearby.
- [ ] Confirm M7 export consumes runtime-image layout facts after runtime-image/compiler finalization.

## Tests

- `test_apg_v2_cli_m7_export_arm_syntax`
- `test_apg_v2_cli_m7_export_arm_stack_usage`
- ARM link gate when a linker script is configured.
- Board timing gate when command is configured.

## Exit Criteria

- Generated bundle links with the target memory map.
- Board timing and stack gates are measured, not assumed.
- Static export has no runtime YAML, loader, dynamic allocation, or runtime-init dependency.
