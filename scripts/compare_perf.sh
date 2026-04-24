#!/usr/bin/env bash
# File: compare_perf.sh
# Description: Run perf stat alongside the instrumentation profiler for comparison
# Part of: LLVM Profiling Pass Project

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_DIR="$ROOT_DIR/outputs"
TMP_DIR="/tmp/cd-lab-el"

mkdir -p "$OUTPUT_DIR" "$TMP_DIR"

if [ $# -lt 1 ]; then
    echo "Usage: $0 <source.c>"
    exit 1
fi

SOURCE="$1"
BASE="$(basename "$SOURCE" .c)"

echo "=== Building and running instrumentation profiler ==="
bash "$ROOT_DIR/run.sh" "$SOURCE" "$OUTPUT_DIR/${BASE}_profile.txt"
echo ""
cat "$OUTPUT_DIR/${BASE}_profile.txt"

echo ""
echo "=== Building baseline binary for perf ==="
clang -O0 "$SOURCE" -o "$TMP_DIR/${BASE}_base"

if ! command -v perf &>/dev/null; then
    echo ""
    echo "perf not available on this system. Skipping perf stat."
    echo "Install with: sudo apt install linux-tools-common linux-tools-generic"
    exit 0
fi

PERF_OUT="$OUTPUT_DIR/${BASE}_perf.txt"
echo ""
echo "=== perf stat output ==="
perf stat -e cycles,instructions,cache-misses,branch-misses \
    "$TMP_DIR/${BASE}_base" 2>&1 | tee "$PERF_OUT"

echo ""
echo "perf stat saved to $PERF_OUT"
echo ""
echo "=== Summary ==="
echo "Instrumentation profiler output : $OUTPUT_DIR/${BASE}_profile.txt"
echo "perf stat output                : $PERF_OUT"
