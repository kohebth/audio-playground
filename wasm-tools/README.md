# WASM Tools

`wasm-tools` is the browser middleware around APGCore. It owns the Web Worker and AudioWorklet-facing ABI while APGCore
remains independent of browser APIs and reusable by native and embedded hosts.

The middleware is split into two modules:

- `apg_control`: in-memory workspace validation, compilation, diagnostics, and prepared runtime images.
- `apg_processor`: prepared-image hydration, real-time processing, controls, runtime swapping, and measurements.

The control module currently accepts a revisioned in-memory workspace, confines and resolves relative project unit
paths within that workspace, validates all YAML through APGCore, and compiles the entry project. It exposes structured
diagnostics and a compiled-workspace summary. Processor operations are added as independent vertical slices without
adding browser dependencies to `inc/apgcore/` or `src/apgcore/`.

## Native verification

Configure and test from the repository root:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build -L wasm-tools --output-on-failure
```

## Emscripten build

Configure the repository with the Emscripten toolchain. The `apg_control` and `apg_processor` targets produce modular
ES module loaders and their corresponding `.wasm` binaries:

```sh
emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm --target apg_control apg_processor
```

Neither module uses Emscripten's virtual filesystem. YAML workspace files are supplied from browser memory through the
control ABI.
