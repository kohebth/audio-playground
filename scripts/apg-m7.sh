#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/build/native"

echo "=== [apg-m7] Building & testing STM32F729 M7 firmware backbone in ${build_dir} ==="

cmake -S "${repo_root}" -B "${build_dir}" -DCMAKE_BUILD_TYPE=Debug
cmake --build "${build_dir}" --target test_m7_firmware_backbone --parallel
ctest --test-dir "${build_dir}" -R '^test_m7_firmware_backbone$' --output-on-failure
