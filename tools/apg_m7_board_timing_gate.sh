#!/bin/sh
set -eu

if [ "$#" -lt 2 ]; then
    echo "usage: $0 <m7-export-dir> <board-command> [args...]" >&2
    exit 2
fi

export_dir=$1
shift

if [ ! -d "$export_dir" ]; then
    echo "m7_static_board_error=missing_export_dir path=$export_dir" >&2
    exit 2
fi

output=$("$@" "$export_dir")
printf '%s\n' "$output"

printf '%s\n' "$output" | awk '
BEGIN {
    block_found = 0
    budget_found = 0
}
{
    for (i = 1; i <= NF; i++) {
        if ($i ~ /^m7_static_board_block_us=[0-9]+([.][0-9]+)?$/) {
            split($i, parts, "=")
            block_us = parts[2] + 0
            block_found = 1
        }
        if ($i ~ /^budget_us=[0-9]+([.][0-9]+)?$/) {
            split($i, parts, "=")
            budget_us = parts[2] + 0
            budget_found = 1
        }
    }
}
END {
    if (!block_found || !budget_found) {
        print "m7_static_board_error=missing_timing_fields" > "/dev/stderr"
        exit 1
    }
    if (block_us <= 0 || budget_us <= 0) {
        print "m7_static_board_error=nonpositive_timing" > "/dev/stderr"
        exit 1
    }
    if (block_us > budget_us) {
        printf "m7_static_board_error=budget_exceeded block_us=%.3f budget_us=%.3f\n", block_us, budget_us > "/dev/stderr"
        exit 1
    }
}
'
