#!/bin/bash

# Build the LLVM pass as a shared object
# Use clang++ with hardcoded paths for LLVM 22

clang++ -shared -fPIC -I"C:\Program Files\LLVM\include" src/FunctionProfiler.cpp -o FunctionProfiler.so