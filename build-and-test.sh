#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-./build/native}"

cmake -S . -B "$BUILD_DIR" >/dev/null
cmake --build "$BUILD_DIR" >/dev/null
ctest --test-dir "$BUILD_DIR"
