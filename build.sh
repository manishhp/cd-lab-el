#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"

mkdir -p "$BUILD_DIR"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
	-DLLVM_DIR="$(llvm-config --cmakedir)" \
	-DCMAKE_BUILD_TYPE=Release

cmake --build "$BUILD_DIR" -j"$(nproc)"

clang -O1 -c "$ROOT_DIR/src/runtime.c" -o "$BUILD_DIR/runtime.o"

echo "Build complete: pass plugin and runtime object are in $BUILD_DIR"