#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

for script in "${repo_root}"/scripts/apg-*.sh; do
    if [ -x "$script" ]; then
        "$script"
    fi
done
