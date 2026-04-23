# Assignment 5: Function-Level Profiling Instrumentor

This repository implements an LLVM new-pass-manager plugin that injects profiling counters at function entries and loop headers, then prints a sorted report at program exit.

## Repository Contents

- `src/FunctionProfiler.cpp`: LLVM pass plugin registered as `profiler-pass`
- `src/runtime.c`, `src/runtime.h`: runtime registry and `dumpProfile()` implementation
- `build.sh`: builds the pass plugin and runtime object
- `run.sh`: instruments and runs a given C source file
- `scripts/evaluate.sh`: baseline-vs-instrumented evaluation runner for all tests
- `tests/test1.c` ... `tests/test6.c`: test programs
- `DESIGN.md`: design decisions and alternatives
- `IMPLEMENTATION.md`: LLVM-specific implementation details
- `EVALUATION.md`: metrics, comparisons, and test-case results
- `DEMO.md`: working and failure-case demo evidence

## Environment

Recommended environment: Ubuntu on WSL2.

Install dependencies:

```bash
sudo apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y clang llvm llvm-dev cmake build-essential
```

Verify tools:

```bash
clang --version
opt --version
llvm-config --version
cmake --version
```

## Build

```bash
bash build.sh
```

Expected artifact: `build/FunctionProfiler.so` and `build/runtime.o`.

## Run

Run one program through the pass:

```bash
bash run.sh tests/test4.c
cat outputs/test4_profile.txt
```

Example output:

```text
=== Profile Report ===
Rank  Count       Function
	1  6          recursive
	2  1          main
=====================
```

## Evaluation

Run measurable baseline comparison across all test cases:

```bash
bash scripts/evaluate.sh
```

This generates `outputs/eval/summary.csv` with baseline and instrumented timing, slowdown, and top hot function per test.

## Submission Checklist Mapping

- Project overview and execution steps: this file
- Design document: `DESIGN.md`
- Implementation document: `IMPLEMENTATION.md`
- Evaluation with at least 5 tests and baseline comparison: `EVALUATION.md` and `scripts/evaluate.sh`
- Scripts and source code: included (`build.sh`, `run.sh`, `src/`, `tests/`)
- Demo evidence (working + failure): `DEMO.md`

## Demo Video Evidence

Use the demo checklist and link template in `DEMO.md`.

- Working demo video: REPLACE_WITH_LINK_OR_MEDIA_PATH
- Failure demo video: REPLACE_WITH_LINK_OR_MEDIA_PATH
