# Repository Guidelines

## Project Structure & Module Organization

This repository contains independent projects with separate dependencies:

- `src/`, `inc/`, `test/`, `units/`, `configs/`, `CMakeLists.txt`: C11 DSP engine, YAML unit loader, PipeWire live app, and CTest targets.
- `web-tools/unit-editor/`: React + TypeScript + Vite editor for `units/*.unit.yaml`.
- `audio-mcp/`: Python MCP server for audio analysis, generation, chunking, resampling, and rescaling helpers.
- `search-mcp/`: TypeScript MCP server exposing `search_web`.
- `samples/` and `analysis/`: audio inputs and rendered inspection outputs. Commit large generated audio only as intentional fixtures.

Headers in `inc/` mirror the C source layout. DSP unit YAML is hand-written and loaded at runtime.

## Build, Test, and Development Commands

Run C/CMake commands from the repo root:

```sh
cmake -S . -B build && cmake --build build
ctest --test-dir build
build/test_hall_reverb
```

`libpipewire-0.3` is required for `live` and PipeWire-linked targets. PipeWire tuning config is in `configs/10-quantum.conf`.

Run web editor commands from `web-tools/unit-editor/`:

```sh
npm run dev
npm run build
npm run lint
```

Run MCP package commands in their own directories:

```sh
# audio-mcp/
pip install -e .
python -m audio_mcp
python -m pytest tests/ -v

# search-mcp/
npm install
npm run build
npm run dev
```

## Coding Style & Naming Conventions

C uses LLVM `clang-format` with 4-space indentation and a 120-column limit:

```sh
clang-format -i src/**/*.c inc/**/*.h
```

Keep C headers and sources paired by directory and name where practical. Name C tests `test_<feature>.c` and DSP units `<effect>.unit.yaml`. TypeScript uses ES modules and local ESLint. Python targets 3.10+ under `audio-mcp/src/audio_mcp/`.

## Testing Guidelines

C tests are CTest targets. Run through CTest or from the repo root so `units/` and `configs/` resolve correctly. Add focused tests under `test/` for runtime, YAML, DSP atom, or offline chain behavior. `audio-mcp` uses `pytest` with `tmp_path` fixtures and no external audio files. The unit editor currently has lint/build checks but no test framework.

## Commit & Pull Request Guidelines

Recent history favors short imperative subjects, often Conventional Commit style: `feat: add simple test for each part`, `feat(editor): Implement automatic layout`. Prefer `feat:`, `fix:`, `test:`, `refactor:`, or a concise imperative sentence.

Pull requests should name the changed project area, list commands run, call out PipeWire or audio-file requirements, and include screenshots for unit-editor UI changes.

## Agent-Specific Instructions

Do not mix dependencies between the C engine, web editor, and MCP packages. Preserve user changes, and keep edits scoped to the requested project area.
