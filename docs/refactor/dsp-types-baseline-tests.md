# DSP Type Refactor Baseline Tests

## Environment

```text
commit: 938157e42bcc665a5a376bcd3e6db3df5b30e0be
required baseline ancestor: a813b26f59bfeca9b1dae854979aa424c4d93e2b
compiler: GCC 13.3.0
cmake: 3.28.3
build type: Debug
C standard: C11
APG_ENABLE_SANITIZERS: OFF
```

## Commands

```sh
cmake -S . -B build-dsp-types-baseline -DCMAKE_BUILD_TYPE=Debug
cmake --build build-dsp-types-baseline -j2
ctest --test-dir build-dsp-types-baseline --output-on-failure
```

## Result

```text
Total Tests: 67
Passed: 67
Failed: 0
Total test time: 6.26 seconds
Labels:
  v2: 63 tests
  wasm-tools: 5 tests
```

The suite includes registry, catalog, parser, validator, compiler, runtime, measure, host live-swap, CLI export, WASM,
and M7 static export smoke coverage. The complete CTest output is retained in `dsp-types-baseline-ctest.log`.
