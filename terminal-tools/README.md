# APG Terminal Tools

`apg-tui` is the optional C++20 terminal counterpart to `web-tools`. It opens and
saves the same `.apg` package envelope while keeping terminal, FTXUI, and native
audio dependencies outside APGCore.

## Build and run

Terminal tools are off in the default root build. The first terminal build fetches
the pinned FTXUI, yaml-cpp, nlohmann/json, and miniaudio sources:

```sh
cmake -S . -B build-terminal -DAPG_BUILD_TERMINAL_TOOLS=ON
cmake --build build-terminal --parallel
ctest --test-dir build-terminal -L terminal-tools --output-on-failure
```

Open the cross-editor fixture:

```sh
./build-terminal/terminal-tools/apg-tui test/fixtures/packages-v1/simple-gain.apg
```

Use `--no-audio` when no native device should be opened. Raw project or unit YAML
is intentionally rejected; package files preserve their embedded unit sources,
mono audio assets, and extension data.

## Editing

The UI uses a three-pane studio at 120×32 and larger, compact tabs down to 80×24,
and a resize guard below that. The route graph is derived from `chain.routes` and
renders nested panner/mixer sections recursively.

- `Tab` / `Shift+Tab`: switch panes
- Arrow keys or `h`/`j`/`k`/`l`: navigate or adjust the active control
- `r`: cycle the selected insertion route
- `Enter`: activate a node, insert a unit, or recall a scene
- `p`: insert a two-path parallel section from the Units pane
- `x`: move the selected ordinary effect to the selected route
- `c`: collapse a selected parallel section after its paths are empty
- `b` / `d`: bypass or remove the selected ordinary effect
- `n` / `u` / `e` / `d`: create, update, rename, or delete a scene
- `Space` / `m`: start or stop monitoring, or toggle mute
- `Ctrl+S`, `Ctrl+Z`, `Ctrl+Y`: save, undo, and redo
- `?` or `F1`: in-app help; `q` uses a guarded dirty-project exit

Mouse selection is supported. Drag an ordinary effect onto a visible signal
line to move it. In the wide layout, drag a unit card onto the line to insert
it. In the compact layout, click a unit to carry it to Graph, then click a
signal line; `Escape` cancels placement. Signal lines meet the exact vertical
center of five-row unit cards, and only the visible line accepts a drop.
Dragging a parameter gauge commits one history entry on release.

## Data and audio safety

Every structural edit is applied to a candidate revision and accepted only after
APGCore resolves, validates, compiles, and prepares it. Save writes a unique
sibling temporary file, flushes it, and atomically replaces the package. The
readiness snapshot is refreshed without discarding packaged audio or unknown
project YAML fields.

Native monitoring uses miniaudio full-duplex mono float I/O and always starts
muted. Runtime graphs and parameter/bypass indices are prepared on the control
thread. The audio callback performs no parsing, allocation, locking, formatting,
or name lookup. Structural changes crossfade over 64 frames and retired graphs
are reclaimed later by the UI thread. Device stops, reroutes, and interruptions
are reported in the Audio pane.

The deterministic fake backend covers callback behavior in tests. A green test
run is not proof that a particular host audio device or driver works; verify live
monitoring on the target machine before relying on it.
