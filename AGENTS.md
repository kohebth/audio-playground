# Repository Guidelines

## Project Structure & Module Organization

This repository contains several independent project areas:

- `src/`, `inc/`, `test/`, `CMakeLists.txt`: C11 DSP engine, atom registry, YAML loaders, APGCore v2 compiler/runtime, and CTest targets.
- `units/`: v1 DSP unit YAML loaded by the legacy runtime.
- `units-v2/`: v2 compiler/runtime fixtures such as `simple_gain`, `simple_mix`, and `simple_clip`.
- `docs/schemas/unit-v2.md` and `docs/UNIT_V2_ARCHITECTURE.md`: current v2 schema and compiler/runtime design notes.
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

Reread `AGENTS.md` at the start of each new work slice before making repository changes. Use `fsmcp` for repository search and file edits. Do not use `apply_patch`; if an `fsmcp` edit fails, stop and report it. Before retrying a failed exact-match edit, reread the target file and base the next edit on the current text. Preserve unrelated user changes. Keep dependencies separate across the C engine, web editor, and MCP packages. Do not stage unrelated modified `units/`, generated audio, or local tool directories unless explicitly requested.
