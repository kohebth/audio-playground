# Repository Guidelines

## Project Structure & Module Organization

This repository contains several independent project areas:

- `src/`, `inc/`, `test/`, `CMakeLists.txt`: C11 DSP engine, metadata registry, YAML parser helpers, APGCore v2 modules, and CTest targets. APGCore code is grouped by module under `src/apgcore/<module>/` and `inc/apgcore/<module>/`; atom implementations are grouped by family under `src/atom/<family>/`.
- `core-design.md`: current production architecture target. Keep new core work aligned with the `metadata -> parser -> validator -> compiler -> registry -> runtime -> measure -> host` boundary.
- `units/`: legacy local v1 YAML drafts only. They are not loaded by the default production build; do not stage modified files here unless the user explicitly decides to port or delete them.
- `test/fixtures/units-v2/` and `test/fixtures/projects-v2/`: v2 test metadata fixtures for parser/validator/compiler/runtime and UI contract checks. These are not APGCore source paths or production module roots.
- `test/golden/`: frozen JSON samples for frontend contract tests and mock data.
- `docs/schemas/unit-v2.md`, `docs/schemas/project-v2.md`, `docs/UNIT_V2_ARCHITECTURE.md`, and `docs/WEB_UI_READINESS.md`: current v2 schemas, compiler/runtime design notes, and web handoff context.
- `web-tools/unit-editor/`, `audio-mcp/`, `search-mcp/`: separate frontend and MCP packages with their own dependencies.
- `samples/` and `analysis/`: audio inputs and generated inspection outputs. Commit large generated audio only as intentional fixtures.

Keep C headers under `inc/` paired with implementation files under `src/` where practical.

## Build, Test, and Development Commands

For C/APGCore work, use the repo-root wrapper:

```sh
./build-and-test.sh
```

It configures CMake under `./build`, suppresses CMake/build stdout, and runs CTest. Use it once per completed implementation slice.

Useful direct commands:

```sh
ctest --test-dir ./build
cmake --build ./build --target check_v2
cmake -S . -B ./build-asan -DCMAKE_BUILD_TYPE=Debug -DAPG_ENABLE_SANITIZERS=ON
cmake --build ./build-asan && ctest --test-dir ./build-asan
./build/test_unit_v2_runtime
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

Name C tests `test_<feature>.c`. Name v2 fixture metadata `<name>.unit.v2.yaml` and `<name>.project.v2.yaml`. Treat v1 `*.unit.yaml` files as legacy drafts only. Keep v2 atom binding keys aligned with `src/apgcore/compiler/unit_compiler_v2.c` metadata.

## Testing Guidelines

C tests are CTest targets. Add focused tests under `test/` for atom behavior, parser boundaries, validator contracts, compiler plans, registry layout, runtime execution, and measure/host reads. V2 fixture coverage should load/compile all `test/fixtures/units-v2/*.unit.v2.yaml`, and runtime tests should exercise named signal buffers, params, schedule execution, state buffers, and failure messages.

## Commit & Pull Request Guidelines

Use short imperative subjects, preferably Conventional Commit style: `feat:`, `fix:`, `test:`, `docs:`, or `refactor:`. The user has approved `git commit`; commit each completed, verified slice with a message describing the task done.

Pull requests should name the changed project area, list commands run, call out audio-file requirements, and include screenshots for unit-editor UI changes.

## Agent-Specific Instructions

Reread `AGENTS.md` at the start of each new work slice before making repository changes. Before deciding to write new code, choose the shortest clear implementation that preserves the exact intended behavior and remains simple to read. Use fast local search such as `rg` for repository inspection, and use `apply_patch` for manual file edits. Preserve unrelated user changes. Keep dependencies separate across the C engine, web editor, and MCP packages. Do not stage unrelated modified `units/`, generated audio, or local tool directories unless explicitly requested.

## Continuous Work Protocol

Current milestone: Full Audio Playground v2 MVP phases AC-AJ are complete. The next target is production core hardening from `core-design.md`: make `metadata`, `parser`, `validator`, `compiler`, registry, `runtime`, `measure`, and `host` stay isolated so the real-time path only executes a compact prebuilt schedule over registered contiguous memory. After that, continue STM32H7/M7 export validation and real WASM AudioWorklet preview/export from `problem.md`.

When the user says `continue`, `next`, `go`, or gives broad approval, reread `AGENTS.md`, inspect the current trackers, pick the next unchecked actionable task, and carry it through implementation, tracker updates, build-only verification, and commit. Prefer one coherent module at a time, but continue into the next module in the same turn when the path is clear and no approval or product decision is needed. For broad edits or refactors, complete all closely related changes as one coherent slice instead of fragmenting them into small commits, unless a product decision, risky behavior change, or verification failure requires stopping.

For future phased implementation, keep the AC-AJ lifecycle pattern: implement module slices first, record pending tests while implementation is active, add focused tests after implementation modules are complete, then finish docs.

Keep `plan.md`, `task.md`, `docs/WEB_UI_READINESS.md`, and relevant plan documents aligned as work advances. If a task is blocked by missing design context, record it in `problem.md`, update the trackers, and move to the next actionable item.

For web UI work, start from `docs/WEB_UI_READINESS.md`, fixture metadata such as `test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml`, and the frozen files in `test/golden/`. Treat those samples as frontend data contracts only, not source paths. Build the project-level pedalboard workflow before unit-internals editing, and avoid changing backend JSON contracts unless a tracked UI requirement needs it.

After each slice, stage only the files that belong to that slice and commit with `git commit -m "<which tasks are done>"`. Docs-only tracker updates do not require `./build-and-test.sh`. Web implementation slices require `npm run build` inside `web-tools/unit-editor`; backend implementation slices require `./build-and-test.sh` when compile confidence is needed. Before ending a turn, report the phase, committed slice, verification run, and next planned task.
