#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${repo_root}/build/native}"

echo "=== [apg-core] Building & testing C11 core in ${BUILD_DIR} ==="
cmake -S "${repo_root}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" --parallel
ctest --test-dir "${BUILD_DIR}" --output-on-failure
