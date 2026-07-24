# APG Terminal Tools

`apg-tui` is the optional native C++ terminal Pipeline editor. It is intentionally separate from APGCore and `web-tools`.

## Build

The first build downloads pinned FTXUI 7.0.1 and yaml-cpp 0.9.0 through CMake FetchContent:

```sh
cmake -S . -B build-terminal -DAPG_BUILD_TERMINAL_TOOLS=ON
cmake --build build-terminal --parallel
ctest --test-dir build-terminal -L terminal-tools --output-on-failure
```

Run it with a v2 project path:

```sh
./build-terminal/terminal-tools/apg-tui test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml
```

Click an effect to select it. The right panel shows compact parameter knobs as `[########]` bars; click the left side of a row to decrease or the right side to increase within the unit's declared range. Drag an ordinary effect onto the left or right half of another effect to place it before or after that target. Projects containing explicit parallel-routing sections reject drag moves until the terminal layout can represent branch rails safely.

Keyboard navigation falls back to Tab (next), Tab-reverse (previous), and the arrow keys for selection. `Enter` toggles transport, and `q` quits.

`--version` and `--self-test` are non-interactive CI entrypoints. The current session is a null-audio seam; native device I/O and staged APGCore runtime swaps are the next implementation slice.

Saving rewrites YAML canonically through a temporary file and atomic rename. Invalid structural documents are not saved.
