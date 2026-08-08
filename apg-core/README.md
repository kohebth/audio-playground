# APGCore (`apg-core`)

`apg-core` is the native C11 core audio synthesis and processing engine for Audio Playground. It contains all core DSP atoms, graph validation, routing, runtime execution, and project serialization/compilation logic.

## Directory Structure

```
apg-core/
├── benchmarks/         # Hardware timing benchmarks & board timing gates
├── cmd/                # Native CLI executables (apg-v2 CLI)
├── include/            # Public and internal C11 headers
│   ├── apgcore/        # APGCore module headers (runtime, compiler, parser, validator, etc.)
│   ├── atom/           # DSP atom definitions, field descriptors, and generated ABI headers
│   ├── rte/            # Real-Time Engine atom registry and thunks
│   ├── util/           # Memory management & utility headers
│   └── yaml/           # Minimal zero-allocation C YAML parser headers
├── src/                # Core C11 source implementations
│   ├── apgcore/        # APGCore module logic
│   ├── atom/           # DSP atom implementations grouped by family (amplitude, filter, delay, etc.)
│   ├── rte/            # Atom registration and thunk implementations
│   ├── util/           # Utility implementations
│   └── yaml/           # C YAML parser implementation
└── test/               # Native C11 test suite & ABI verification snapshot tests
    └── abi/            # ABI compatibility verification and header smoke tests
```

## Build & Verification

The core library is built as part of the root CMake build system or independently using the standard component runner:

```sh
./scripts/apg-core.sh
```

### Running CTest directly

To run the native C11 test suite:

```sh
cmake --build build/native --parallel
ctest --test-dir build/native -L v2 --output-on-failure
```

### Atom Generation & Verification

Atom definitions and ABI contracts are generated from `schema/atoms/atoms.json`. After altering atom definitions:

```sh
cmake --build build/native --target generate_atom_artifacts
cmake --build build/native --target check_atom_artifacts
```
