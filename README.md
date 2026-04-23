# LLVM Function Profiler Lab

This project implements an LLVM pass plugin that instruments C programs with profiling counters and prints a runtime report.

## What is included

- `src/FunctionProfiler.cpp`: LLVM pass plugin (`profiler-pass`)
- `src/runtime.c`: runtime registry and profile report printing
- `src/runtime.h`: runtime API declarations
- `build.sh`: builds the LLVM pass plugin and runtime object
- `run.sh`: instruments and runs a test/source file
- `tests/`: small C programs for validation

## Requirements

Use WSL/Linux with:

- LLVM/Clang/opt (14+; verified with 18.1.3)
- CMake 3.16+
- Bash

## Build

From WSL/Linux (recommended):

```bash
bash build.sh
```

## Run a test

```powershell
bash run.sh tests/test2.c
```

The profile output is written to:

- `outputs/test2_profile.txt`

To inspect:

```powershell
cat outputs/test2_profile.txt
```

## Pass behavior (current baseline)

- Creates one counter per instrumented function
- Increments counter at function entry
- Increments counter at loop headers
- Registers counters through module constructor
- Calls `dumpProfile()` before each `main` return

## Notes

- `run.sh` compiles test input with `-O0` so simple functions in tests are not optimized away.
- Generated artifacts are kept in `build/`, `outputs/`, and `/tmp/cd-lab-el`.
