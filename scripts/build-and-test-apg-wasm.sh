#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "=== [apg-wasm] Building & testing WASM facade package ==="
cd "${repo_root}/apg-wasm"
npm ci
npm run typecheck
npm run build
