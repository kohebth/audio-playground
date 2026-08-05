#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${repo_root}/build/tui}"

echo "=== [apg-tui] Building & testing Terminal UI editor in ${BUILD_DIR} ==="
cmake -S "${repo_root}" -B "${BUILD_DIR}" -DAPG_BUILD_TERMINAL_TOOLS=ON
cmake --build "${BUILD_DIR}" --parallel
ctest --test-dir "${BUILD_DIR}" -L apg-tui --output-on-failure
