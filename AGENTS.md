# Repository Guidelines

## Project Structure & Module Organization

This repository contains several independent project areas:

- `src/`, `inc/`, `test/`, `CMakeLists.txt`: C11 DSP engine, atom registry, YAML loaders, APGCore v2 compiler/runtime, and CTest targets.
- `units/`: v1 DSP unit YAML loaded by the legacy runtime.
- `units-v2/` and `projects-v2/`: v2 compiler/runtime fixtures, including product units and guitar pedalboard project examples.
- `test/golden/`: frozen JSON samples for frontend contract tests and mock data.
- `docs/schemas/unit-v2.md`, `docs/schemas/project-v2.md`, `docs/UNIT_V2_ARCHITECTURE.md`, and `docs/WEB_UI_READINESS.md`: current v2 schemas, compiler/runtime design notes, and web handoff context.
- `configs/`: PipeWire/runtime tuning config.
- `web-tools/unit-editor/`, `audio-mcp/`, `search-mcp/`: separate frontend and MCP packages with their own dependencies.
- `samples/` and `analysis/`: audio inputs and generated inspection outputs. Commit large generated audio only as intentional fixtures.

Keep C headers under `inc/` paired with implementation files under `src/` where practical.

## Build, Test, and Development Commands

For C/APGCore work, use the repo-root wrapper:

```sh
./build-and-test.sh
```

It configures CMake under `/tmp/audio-playground-apgcore-build`, suppresses CMake/build stdout, and runs CTest. Use it once per completed implementation slice.

Useful direct commands:

```sh
ctest --test-dir /tmp/audio-playground-apgcore-build
cmake --build /tmp/audio-playground-apgcore-build --target check_v2
cmake -S . -B /tmp/audio-playground-apgcore-asan -DCMAKE_BUILD_TYPE=Debug -DAPG_ENABLE_SANITIZERS=ON
cmake --build /tmp/audio-playground-apgcore-asan && ctest --test-dir /tmp/audio-playground-apgcore-asan
/tmp/audio-playground-apgcore-build/test_unit_v2_runtime
```

For web and MCP packages, run commands inside their package directories:

```sh
cd web-tools/unit-editor && npm run build && npm run lint
cd audio-mcp && python -m pytest tests/ -v
cd search-mcp && npm run build
```

## Coding Style & Naming Conventions

C uses LLVM `clang-format` with 4-space indentation and a 120-column limit. The user has approved running `clang-format`; format touched C/H files before committing:

```sh
clang-format -i src/**/*.c inc/**/*.h test/**/*.c
```

Name C tests `test_<feature>.c`. Name v1 units `<effect>.unit.yaml` and v2 fixtures `<name>.unit.v2.yaml`. Keep v2 atom binding keys aligned with `src/apgcore/compiler_v2.c` metadata.

## Testing Guidelines

C tests are CTest targets. Add focused tests under `test/` for atom behavior, loaders, compiler contracts, runtime execution, and adapter frame limits. V2 fixture coverage should load/compile all `units-v2/*.unit.v2.yaml`, and runtime tests should exercise named signal buffers, params, schedule execution, state buffers, and failure messages.

## Commit & Pull Request Guidelines

Use short imperative subjects, preferably Conventional Commit style: `feat:`, `fix:`, `test:`, `docs:`, or `refactor:`. The user has approved `git commit`; commit each completed, verified slice with a message describing the task done.

Pull requests should name the changed project area, list commands run, call out PipeWire or audio-file requirements, and include screenshots for unit-editor UI changes.

## Agent-Specific Instructions

Reread `AGENTS.md` at the start of each new work slice before making repository changes. Before deciding to write new code, choose the shortest clear implementation that preserves the exact intended behavior and remains simple to read. Use fast local search such as `rg` for repository inspection, and use `apply_patch` for manual file edits. Preserve unrelated user changes. Keep dependencies separate across the C engine, web editor, and MCP packages. Do not stage unrelated modified `units/`, generated audio, or local tool directories unless explicitly requested.

## Continuous Work Protocol

Current milestone: Full Audio Playground v2 MVP phases AC-AJ are complete. The next target is post-MVP hardening in `problem.md`, primarily real WASM AudioWorklet preview/export beyond the deterministic preview adapter and blocked `wasm_realtime` export skeleton.

When the user says `continue`, `next`, `go`, or gives broad approval, reread `AGENTS.md`, inspect the current trackers, pick the next unchecked actionable task, and carry it through implementation, tracker updates, build-only verification, and commit. Prefer one coherent module at a time, but continue into the next module in the same turn when the path is clear and no approval or product decision is needed.

For future phased implementation, keep the AC-AJ lifecycle pattern: implement module slices first, record pending tests while implementation is active, add focused tests after implementation modules are complete, then finish docs.

Keep `plan.md`, `task.md`, `docs/WEB_UI_READINESS.md`, and relevant plan documents aligned as work advances. If a task is blocked by missing design context, record it in `problem.md`, update the trackers, and move to the next actionable item.

For web UI work, start from `docs/WEB_UI_READINESS.md`, `projects-v2/guitar-pedalboard.project.v2.yaml`, and the frozen files in `test/golden/`. Treat those samples as the first frontend data contract. Build the project-level pedalboard workflow before unit-internals editing, and avoid changing backend JSON contracts unless a tracked UI requirement needs it.

After each slice, stage only the files that belong to that slice and commit with `git commit -m "<which tasks are done>"`. Docs-only tracker updates do not require `./build-and-test.sh`. Web implementation slices require `npm run build` inside `web-tools/unit-editor`; backend implementation slices require `./build-and-test.sh` when compile confidence is needed. Before ending a turn, report the phase, committed slice, verification run, and next planned task.
