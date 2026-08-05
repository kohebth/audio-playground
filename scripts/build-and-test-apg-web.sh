#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "=== [apg-web] Building & testing Web React/Vite package ==="
cd "${repo_root}/apg-web"
npm ci
npm run typecheck
npm run lint
npm test
npm run build
