# Repository Guide

## Product Boundaries

- Root CMake builds the C11 `apgcore` library, `apg-v2` CLI, native tests, and native `apg-wasm` tests. The C11 core, CLI, and benchmarks live under `apg-core/`; APGCore modules live in matching `apg-core/src/apgcore/<layer>/` and `apg-core/inc/apgcore/<layer>/` directories; atom implementations live under `apg-core/src/atom/<family>/`.
- Preserve the `metadata -> parser -> validator -> compiler -> registry -> runtime -> measure -> host` boundary in `core-design.md`. The runtime walks a prebuilt schedule; parsing, compilation, allocation, formatting, and name lookup do not belong in an audio callback.
- `apg-wasm/` owns the C control/processor ABI and the TypeScript Worker/AudioWorklet facade. Keep browser dependencies out of `apg-core/src/apgcore/` and `apg-core/inc/apgcore/`; CTest has boundary checks for both core imports and processor real-time work.
- `apg-web/` is the React 19/Vite application. Its real entry path is `src/main.tsx -> StudioApp.tsx -> App.tsx`; it consumes `@audio-playground/apg-wasm` through `file:../apg-wasm`.
- There is no root JavaScript workspace. `apg-wasm/` and `apg-web/` have separate lockfiles and installs. Ignored/local `audio-mcp/`, `search-mcp/`, `.opencode/`, `.codex/`, `analysis/`, `samples/`, and build directories are not product packages.
- V1 `units/*.unit.yaml` content was deleted. Do not restore or stage local v1 drafts; executable metadata is under `test/fixtures/units-v2/` and `test/fixtures/projects-v2/`.
- `apg-tui/` is an optional native C++20 terminal editor. It is intentionally outside APGCore; enable it with `-DAPG_BUILD_TERMINAL_TOOLS=ON`. Native monitoring uses miniaudio; deterministic tests use its fake audio backend.

## Contracts And Generated Files

- `test/fixtures/` and `test/golden/` are executable backend/frontend contracts, not production module roots. `apg-web/src/lib/backendSamples.ts` imports selected files directly with `?raw`, and the web contract scripts inspect both fixtures and source files.
- `schema/atoms/atoms.json` is the source of truth for atom ABI order, registry order, capabilities, field metadata, TypeScript metadata, and the binding schema. Never hand-edit files carrying the `Generated from schema/atoms/atoms.json` banner.
- Regenerate atom outputs only after configuring `build/`, then verify stale output:

```sh
cmake --build build --target generate_atom_artifacts
cmake --build build --target check_atom_artifacts
```

- A public atom-catalog change also requires an intentional update to `test/golden/v2-inspect-atoms.json` and `test/golden/v2-inspect-atoms.manifest.txt`, followed by native and web verification.
- Apply `.clang-format` only to handwritten C/H files you changed. Generated C/H files are byte-for-byte checked and must not be reformatted separately.
- New ordinary C test files are not auto-registered: add their target to `TEST_TARGETS` in `CMakeLists.txt`. `test_atom_basic` is the exception and aggregates multiple source files explicitly.

## Native Verification

- Native configuration requires a C/C++ toolchain and Perl. The full gate configures, builds, and runs CTest; configure/build output is suppressed, `BUILD_DIR` overrides `./build/native`, and no npm or Emscripten checks run:

```sh
./apg.sh
./scripts/apg-core.sh
BUILD_DIR=./build/alt ./scripts/apg-core.sh
```

- After a build is configured, run one C test with:

```sh
cmake --build build/native --target test_unit_v2_runtime
ctest --test-dir build/native -R '^test_unit_v2_runtime$' --output-on-failure
```

- `check_v2` runs every test carrying the `v2` label, but its target dependencies do not build every labeled executable. Build all targets first:

```sh
cmake --build build/native --parallel
cmake --build build/native --target check_v2
```

- Native WASM middleware tests, including boundary checks, use the `wasm-tools` label:

```sh
cmake --build build/native --target test_wasm_tools_abi test_wasm_tools_workspace test_wasm_tools_processor
ctest --test-dir build/native -L wasm-tools --output-on-failure
```

- For sanitizer coverage, use the ASan build tree (or `cmake --preset asan`):

```sh
cmake -S . -B build/asan -DCMAKE_BUILD_TYPE=Debug -DAPG_ENABLE_SANITIZERS=ON
cmake --build build/asan --parallel
ctest --test-dir build/asan --output-on-failure
```

- A green default CTest run is not hardware proof. ARM syntax/link gates skip without `APG_M7_C_COMPILER` and `APG_M7_LINKER_SCRIPT`; board timing skips without `APG_M7_BOARD_TIMING_COMMAND`. See `docs/STM32H7_M7_BOARD_INTEGRATION.md` before claiming M7 readiness.
- Terminal UI verification uses a separate build tree because it fetches C++ dependencies through CMake FetchContent (or `cmake --preset tui`):

```sh
cmake -S . -B build/tui -DAPG_BUILD_TERMINAL_TOOLS=ON
cmake --build build/tui --parallel
ctest --test-dir build/tui -L terminal-tools --output-on-failure
```

## Browser Verification

- CI uses Node 22 and Emscripten 5.0.1. Reproduce the complete browser dependency/build order from the repository root:

```sh
cd apg-wasm
npm ci
npm run build
cd ..
./apg-wasm/build-emscripten-docker.sh
cd apg-web
npm ci
npm run typecheck
npm run lint
npm test
npm run build
```

- `build-emscripten-docker.sh` uses the pinned Docker image, runs the Node runtime smoke test, and stages generated modules under ignored `apg-web/public/wasm/`. Never commit that directory, `apg-wasm/dist/`, or `apg-web/dist/`.
- `apg-web`'s `npm test` runs Node contract/transform tests only; it does not run Playwright. Run one Node suite directly with `node --disable-warning=ExperimentalWarning --experimental-strip-types scripts/project-v2-graph-tests.mts`.
- Stage the WASM artifacts and run `npx playwright install chromium` before browser tests. A focused UI test is `npx playwright test tests/studio-shell.spec.ts --grep 'creates and restores a visual-first local project' --workers=1`; the normal PR performance gate is `npm run perf:ui:pr`.
- `npm run dev` is suitable for microphone work only on localhost. LAN microphone testing requires `npm run dev:https`, `APG_HTTPS_CERT` and `APG_HTTPS_KEY` in `apg-web/.env.lan-https.local`, and a client that trusts the mkcert CA; see `apg-web/README.md`.

## Release And Workflow

- GitHub Pages publishes only numbered `v2.0-beta<number>` tags and uploads only `apg-web/dist/`. Pull requests validate without deploying; normal `main` pushes do not deploy. Use `docs/GITHUB_PAGES_DEPLOY.md` for the base-path build and smoke sequence.
- `docs/plans/` contains dated implementation records, not an active global tracker. Root `plan.md`, `task.md`, and `problem.md` do not exist; do not select work from historical unchecked boxes unless the user names that plan.
- Preserve unrelated worktree changes. After a completed verified slice, stage only its files and commit with the repository's usual `feat:`, `fix:`, `test:`, `docs:`, or `refactor:` prefix; never include local build/tool output.
