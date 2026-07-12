#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="build-wasm-container"
image="emscripten/emsdk:5.0.1"

docker run --rm \
    --volume "${repo_root}:/src" \
    --workdir /src \
    "${image}" \
    bash -lc "cmake -S . -B ${build_dir} \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake \
        && cmake --build ${build_dir} --target apg_control apg_processor -j2"

install -d "${repo_root}/web-tools/unit-editor/public/wasm"
install -m 0644 \
    "${repo_root}/${build_dir}/wasm-tools/apg_control.mjs" \
    "${repo_root}/${build_dir}/wasm-tools/apg_control.wasm" \
    "${repo_root}/${build_dir}/wasm-tools/apg_processor.mjs" \
    "${repo_root}/${build_dir}/wasm-tools/apg_processor.wasm" \
    "${repo_root}/web-tools/unit-editor/public/wasm/"
