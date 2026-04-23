#!/usr/bin/env bash
set -euo pipefail
if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "Usage: $0 <source.c> [output_profile.txt]"
    exit 1
fi
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
OUTPUT_DIR="$ROOT_DIR/outputs"
TMP_DIR="/tmp/cd-lab-el"
SOURCE_FILE="$1"
SOURCE_BASE="$(basename "$SOURCE_FILE" .c)"
mkdir -p "$OUTPUT_DIR" "$TMP_DIR"
"$ROOT_DIR/build.sh"
PASS_LIB="$BUILD_DIR/FunctionProfiler.so"
if [ ! -f "$PASS_LIB" ]; then
    PASS_LIB="$BUILD_DIR/FunctionProfiler.dylib"
fi
if [ ! -f "$PASS_LIB" ]; then
    echo "Could not find FunctionProfiler pass plugin in $BUILD_DIR"
    exit 1
fi
LLVM_IR="$TMP_DIR/${SOURCE_BASE}.ll"
INSTRUMENTED_BC="$TMP_DIR/${SOURCE_BASE}_inst.bc"
BINARY="$TMP_DIR/${SOURCE_BASE}_profiled"
OUTPUT_FILE="${2:-$OUTPUT_DIR/${SOURCE_BASE}_profile.txt}"
clang -O0 -S -emit-llvm "$SOURCE_FILE" -o "$LLVM_IR"
opt -load-pass-plugin "$PASS_LIB" \
    -passes="profiler-pass" \
    "$LLVM_IR" -o "$INSTRUMENTED_BC"

clang "$INSTRUMENTED_BC" "$BUILD_DIR/runtime.o" -o "$BINARY"
"$BINARY" > "$OUTPUT_FILE" 2>&1
echo "Profile saved to $OUTPUT_FILE"