#!/bin/sh
set -eu

export_dir=${1:-}
if [ ! -f "$export_dir/apg_project_m7.h" ] || [ ! -f "$export_dir/apg_project_m7.c" ]; then
    echo "m7_static_board_error=missing_bundle" >&2
    exit 2
fi

echo "m7_static_board_block_us=10.000 budget_us=1000.000 source=mock"
