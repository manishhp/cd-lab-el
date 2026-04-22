#!/bin/bash

# Usage: ./run.sh <test.c>

if [ $# -ne 1 ]; then
    echo "Usage: $0 <test.c>"
    exit 1
fi

TEST_FILE=$1
BASE_NAME=$(basename "$TEST_FILE" .c)

# Compile input C file to LLVM IR using clang
clang -S -emit-llvm "$TEST_FILE" -o "${BASE_NAME}.ll"

# Run the LLVM pass using opt
opt -load-pass-plugin=./FunctionProfiler.so -passes=function-profiler "${BASE_NAME}.ll" -o "${BASE_NAME}_instrumented.ll"

# Compile instrumented IR with runtime.c
clang "${BASE_NAME}_instrumented.ll" src/runtime.c -o "${BASE_NAME}.exe"

# Execute final binary
./"${BASE_NAME}.exe"