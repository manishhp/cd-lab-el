#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_DIR="/tmp/cd-lab-el"
OUTPUT_DIR="$ROOT_DIR/outputs/eval"
ITERATIONS="${ITERATIONS:-500}"

mkdir -p "$TMP_DIR" "$OUTPUT_DIR"

bash "$ROOT_DIR/build.sh" >/tmp/cd_eval_build.log 2>&1

SUMMARY_FILE="$OUTPUT_DIR/summary.csv"

measure_seconds() {
    local binary="$1"
    local start_ns end_ns
    start_ns="$(date +%s%N)"
    for ((i = 0; i < ITERATIONS; i++)); do
        "$binary" >/dev/null 2>&1
    done
    end_ns="$(date +%s%N)"
    awk -v s="$start_ns" -v e="$end_ns" 'BEGIN { printf "%.6f", (e - s) / 1000000000 }'
}

echo "test,iterations,baseline_s,instrumented_s,slowdown,top_function,top_count,profile_rows" > "$SUMMARY_FILE"

for t in "$ROOT_DIR"/tests/test*.c; do
    name="$(basename "$t" .c)"

    clang -O0 "$t" -o "$TMP_DIR/${name}_base"
    baseline_time="$(measure_seconds "$TMP_DIR/${name}_base")"

    bash "$ROOT_DIR/run.sh" "$t" "$OUTPUT_DIR/${name}_profile.txt" >/tmp/cd_eval_${name}.log 2>&1
    instrumented_time="$(measure_seconds "$TMP_DIR/${name}_profiled")"

    slowdown="$(awk -v b="$baseline_time" -v i="$instrumented_time" 'BEGIN { if (b == 0) { print "NA" } else { printf "%.2fx", i / b } }')"
    profile_rows="$(wc -l < "$OUTPUT_DIR/${name}_profile.txt")"
    top_line="$(awk '/^[[:space:]]*[0-9]+[[:space:]]+/ { print; exit }' "$OUTPUT_DIR/${name}_profile.txt")"
    top_count="$(awk '{print $2}' <<<"$top_line")"
    top_function="$(awk '{print $3}' <<<"$top_line")"

    echo "$name,$ITERATIONS,$baseline_time,$instrumented_time,$slowdown,$top_function,$top_count,$profile_rows" >> "$SUMMARY_FILE"
done

echo "Evaluation summary written to $SUMMARY_FILE"
cat "$SUMMARY_FILE"
