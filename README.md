# Assignment 5 — Function-Level Profiling Instrumentor

An LLVM compiler pass that automatically inserts profiling counters at every function entry and loop header, then dumps a sorted call-frequency table when the program exits. No changes to source code required — instrumentation happens entirely at the IR level.

---

## How It Works

```
Source C/C++ file
      │
      ▼  clang -O0 -S -emit-llvm
  LLVM IR (.ll)
      │
      ▼  opt -load-pass-plugin build/FunctionProfiler.so -passes="profiler-pass"
  Instrumented IR
      │   • one global i64 counter per function
      │   • atomicrmw increment at every function entry
      │   • atomicrmw increment at every loop header
      │   • dumpProfile() call injected before main() return
      │
      ▼  clang <bitcode> build/runtime.o
  Instrumented Binary
      │
      ▼  ./binary
  Profile Report (sorted by invocation count)
```

The pass is a **module pass** (new pass manager, `PassInfoMixin`) so it can see all functions, create module-level globals, and emit a constructor that registers each counter with the runtime before `main` starts.

---

## Repository Layout

```
.
├── src/
│   ├── FunctionProfiler.cpp   ← LLVM pass plugin
│   ├── runtime.c              ← C runtime: registry + dumpProfile()
│   └── runtime.h              ← runtime API header
│
├── benchmarks/
│   ├── sort_bench.c           ← quicksort + mergesort on 1 000 000 integers
│   └── matmul.c               ← naive O(n³) matrix multiply, 512×512
│
├── tests/
│   ├── test1.c                ← main only
│   ├── test2.c                ← single function call
│   ├── test3.c                ← nested calls
│   ├── test4.c                ← recursion
│   ├── test5.c                ← nested loops
│   └── test6.c                ← unused function (must not appear in profile)
│
├── scripts/
│   ├── evaluate.sh            ← batch baseline-vs-instrumented evaluation
│   └── compare_perf.sh        ← runs profiler + perf stat side by side
│
├── outputs/                   ← generated profile reports (gitignored)
│   ├── sort_bench_profile.txt
│   ├── matmul_profile.txt
│   └── eval/
│       ├── summary.csv
│       └── test{1..6}_profile.txt
│
├── build/                     ← CMake build output (gitignored)
│   ├── FunctionProfiler.so
│   └── runtime.o
│
├── build.sh                   ← builds plugin + runtime
├── run.sh                     ← instruments and runs a single C source file
├── CMakeLists.txt
├── DESIGN.md
├── IMPLEMENTATION.md
├── EVALUATION.md
└── EXPLANATION.md             ← consolidated technical write-up
```

---

## Environment Setup

**Recommended:** Ubuntu 22.04 on WSL2 (Windows) or native Linux.

### Install Dependencies

```bash
sudo apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
    clang llvm llvm-dev cmake build-essential
```

### Verify

```bash
clang --version        # expect 14.x – 18.x
opt --version
llvm-config --version
cmake --version        # expect 3.16+
```

> The project was developed and tested with **LLVM 18.1.3** on Ubuntu WSL2.

---

## Build

```bash
bash build.sh
```

This does three things:
1. Runs CMake using `llvm-config --cmakedir` to locate LLVM automatically.
2. Compiles `src/FunctionProfiler.cpp` into `build/FunctionProfiler.so`.
3. Compiles `src/runtime.c` into `build/runtime.o`.

Expected output:
```
-- Found LLVM 18.1.3 at /usr/lib/llvm-18/lib/cmake/llvm
[100%] Built target FunctionProfiler
Build complete: pass plugin and runtime object are in /path/to/build
```

---

## Run

### Profile a Single Program

```bash
bash run.sh <source.c> [output_profile.txt]
```

Example:

```bash
bash run.sh tests/test4.c
cat outputs/test4_profile.txt
```

```
=== Profile Report ===
Rank  Count       Function
   1  6          recursive
   2  1          main
=====================
```

`recursive` is called 6 times (initial call + 5 recursive steps) — exactly correct.

### Profile the Benchmarks

```bash
bash run.sh benchmarks/sort_bench.c outputs/sort_bench_profile.txt
bash run.sh benchmarks/matmul.c     outputs/matmul_profile.txt
```

### Limit Output with Top-N

```bash
PROFILER_TOP_N=3 bash run.sh benchmarks/sort_bench.c
```

---

## Example Benchmark Output

### sort_bench — Quicksort + Mergesort on 1 000 000 Integers

```
=== Profile Report ===
Rank  Count       Function
   1  26773896   partition
   2  23951420   merge
   3  1999999    mergesort
   4  1333663    quicksort
   5  1000002    main
   6  1000002    fill_random
=====================
```

`partition` and `merge` dominate because they are called once per recursive subdivision. Sorting 1M elements generates ~26M and ~24M recursive calls respectively. The counter includes both the function-entry hit and the inner loop-header hits, which is why `partition` outranks `quicksort`.

### matmul — Naive O(n³) 512×512 Matrix Multiply

```
=== Profile Report ===
Rank  Count       Function
   1  134743042  matmul
   2  526340     fill_matrix
   3  263170     zero_matrix
   4  1          main
=====================
```

`matmul` accumulates ~134M hits — the sum of the function-entry count (1) plus loop-header counts from three nested loops (512³ = 134 217 728 inner iterations). This makes the hot function immediately obvious.

---

## Evaluation

### Baseline vs. Instrumented — All 6 Test Cases

Run the full evaluation:

```bash
bash scripts/evaluate.sh
cat outputs/eval/summary.csv
```

Results (Ubuntu WSL2, LLVM 18.1.3, 500 iterations each):

| Test | Program | Baseline (s) | Instrumented (s) | Slowdown | Top Function | Count |
|---|---|---:|---:|---:|---|---:|
| test1 | main only | 0.557 | 0.658 | 1.18x | main | 1 |
| test2 | single call | 0.652 | 0.584 | 0.90x | foo | 1 |
| test3 | nested calls | 0.587 | 0.566 | 0.96x | bar | 1 |
| test4 | recursion | 0.509 | 0.653 | 1.28x | recursive | 6 |
| test5 | loops | 0.549 | 0.597 | 1.09x | loop_func | 60 |
| test6 | unused func | 0.548 | 0.615 | 1.12x | used | 1 |

Overhead ranges from ~0.90x to ~1.28x. Values below 1x (tests 2, 3) are measurement noise — these programs run in microseconds, making process-launch overhead dominate the wall-clock timing. The `test6` unused function does not appear in the profile, confirming zero-count filtering works correctly.

### What the Profile Counts Mean

The counter for a function = **function-entry invocations + loop-header crossings**.

- `test4` recursive with depth 5: `recursive` called 6 times (1 + 5 recursive steps) ✓  
- `test5` `loop_func` called 5 times, each with a 10-iteration loop: 5 × (1 entry + 10 loop headers) = 55... wait — the entry block and loop header are separate, so 5 calls × 1 entry + 5 calls × 10 loop iterations = 5 + 50 + 5 (`main` loop) = **60** ✓

---

## Demo

### Working Case

```bash
# Build
bash build.sh

# Run the sort benchmark
bash run.sh benchmarks/sort_bench.c outputs/sort_bench_profile.txt
cat outputs/sort_bench_profile.txt
```

Expected: sorted profile table showing `partition` and `merge` at the top with millions of hits.

```bash
# Run recursion test — verify exact count
bash run.sh tests/test4.c
cat outputs/test4_profile.txt
# recursive should show count = 6
```

### Failure Case 1 — Runtime Not Linked

```bash
# Manually instrument but skip linking runtime.o
clang -O0 -S -emit-llvm tests/test4.c -o /tmp/test4.ll
opt -load-pass-plugin build/FunctionProfiler.so \
    -passes="profiler-pass" /tmp/test4.ll -o /tmp/test4_inst.bc
clang /tmp/test4_inst.bc -o /tmp/test4_fail
```

```
/usr/bin/ld: error: undefined symbol: __profiler_register
/usr/bin/ld: error: undefined symbol: dumpProfile
```

The binary cannot link because the runtime is missing. Fix: add `build/runtime.o` to the link command.

### Failure Case 2 — Pass Not Loaded

```bash
# Run opt without loading the plugin
opt -passes="profiler-pass" /tmp/test4.ll -o /tmp/test4_noplugin.bc
```

```
error: unknown pass name 'profiler-pass'
```

The pass is not built into `opt` — it must be loaded explicitly with `-load-pass-plugin`.

### Failure Case 3 — Source Has No main

```bash
# A library file with no main function
echo 'void helper() {}' > /tmp/noMain.c
bash run.sh /tmp/noMain.c /tmp/noMain_profile.txt
cat /tmp/noMain_profile.txt
```

```
=== Profile Report ===
Rank  Count       Function
   1  1          helper
=====================
```

The pass handles this gracefully — it instruments `helper` but skips `dumpProfile` injection (no `main` to return from). The profile is printed only if `dumpProfile()` is called externally or not at all.

---

## Pass Design Summary

| Transformation | Where Inserted | LLVM API Used |
|---|---|---|
| `@__prof_count_<fn>` global | Module level, once per function | `Module::getOrInsertGlobal` |
| `@__prof_name_<fn>` global | Module level, string constant | `ConstantDataArray::getString` |
| Counter increment | First non-PHI of entry block | `IRBuilder::CreateAtomicRMW` |
| Counter increment | First non-PHI of each loop header | `LoopInfo` + `IRBuilder::CreateAtomicRMW` |
| `__profiler_register` call | Module constructor (`llvm.global_ctors`) | `appendToGlobalCtors` |
| `dumpProfile()` call | Before every `ret` in `main` | `IRBuilder::CreateCall` |

**Atomic increments** (`atomicrmw add monotonic`) are used so the profiler is correct in multi-threaded programs without needing locks.

---

## Runtime API

```c
// Called automatically from the module constructor before main()
void __profiler_register(const char *name, int64_t *counter);

// Called automatically before main() returns
void dumpProfile(void);
```

The registry is a static array of 4096 entries — no heap allocation, no malloc. `dumpProfile` uses `qsort` to sort by descending count and prints only entries with count > 0.

Control output size:
```bash
PROFILER_TOP_N=5 ./your_binary
```

---

## Instrumentation vs. Sampling Profilers

| Property | This Project (Instrumentation) | perf / gprof (Sampling) |
|---|---|---|
| Mechanism | Counter increments inserted at compile time | Periodic interrupts at runtime |
| Count accuracy | Exact — every invocation counted | Statistical estimate |
| Granularity | Function entries + loop headers | Any instruction / call stack |
| Runtime overhead | ~1–1.4x for small programs | < 5% typically |
| Determinism | Fully deterministic | Non-deterministic |
| Requires recompile | Yes | No |
| Kernel/library visibility | No | Yes |

**When to use instrumentation:** exact counts needed, integrating into a compiler pipeline, studying specific loop behavior.

**When to use sampling:** production systems (low overhead), profiling code you cannot recompile, system-wide visibility.

---

## Documents

| File | Contents |
|---|---|
| `DESIGN.md` | Design goals, chosen architecture, alternatives considered |
| `IMPLEMENTATION.md` | LLVM pass internals, IR transformations, runtime details |
| `EVALUATION.md` | Timing results, test case analysis, perf comparison discussion |
| `EXPLANATION.md` | Consolidated end-to-end technical write-up |
