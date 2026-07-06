# M7 Static Export Finish Plan

## Goal

Generated M7 bundles are static, bounded, linkable, and board-verifiable for STM32H7/M7 production.

## Current Status

Partly production-ready; board-specific gates remain.

Done:

- M7 export emits deterministic memory manifest, generated source/header, static atom storage, atom calls, schedule runner, section attributes, and configurable block/sample/cache settings.
- Host-side generated runner, stack, object, section, ARM syntax, and static RAM gates exist.
- Unsupported target profiles and incompatible atoms are rejected.
- `APG_M7_BOARD_TIMING_COMMAND` output is now routed through a strict timing gate that rejects missing, nonpositive,
  or over-budget board measurements.
- `APG_M7_REQUIRE_BOARD_TIMING=ON` fails CMake configure unless a board timing command is supplied.

## Remaining Implementation

- [x] Configure and require ARM syntax/stack gates with installed `arm-none-eabi-gcc` in production runs.
- [x] Add or receive a minimal STM32H7 linker script and enable `APG_M7_LINKER_SCRIPT`.
- [ ] Define and wire `APG_M7_BOARD_TIMING_COMMAND` for real board timing output.
- [x] Improve export output-directory behavior with either auto-create or a clearer diagnostic; prefer clearer diagnostic unless auto-create already exists nearby.
- [x] Confirm M7 export consumes registry layout facts after registry/compiler finalization.

Blocked external input:

- `APG_M7_BOARD_TIMING_COMMAND` must be a board/BSP command that runs the generated bundle on target hardware or an equivalent board harness and prints `m7_static_board_block_us=<value> budget_us=<value>`.
- The repository cannot invent this measurement locally; normal developer runs report that the board timing command is
  unconfigured, while production runs can set `APG_M7_REQUIRE_BOARD_TIMING=ON` to make that a hard configure failure.

## Tests

- `test_apg_v2_cli_m7_export_arm_syntax`
- `test_apg_v2_cli_m7_export_arm_stack_usage`
- ARM link gate when a linker script is configured.
- Board timing gate when command is configured.
- Mock board timing gate test for command-output validation only; not production timing evidence.
- Configure-failure test for `APG_M7_REQUIRE_BOARD_TIMING=ON` without a command.

## Exit Criteria

- Generated bundle links with the target memory map.
- Board timing and stack gates are measured, not assumed.
- Static export has no runtime YAML, loader, dynamic allocation, or runtime-init dependency.
