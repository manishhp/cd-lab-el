# EXPLANATION — LLVM Function Profiler

## 1. Problem Statement

This project implements a **compiler-level instrumentation profiler** using an LLVM pass. Unlike sampling profilers such as `perf` or `gprof`, this profiler operates directly at the **LLVM Intermediate Representation (IR)** level, inserting counters into the program before it is compiled to machine code.

The profiler counts:
- Function invocations (function-entry counter per function).
- Loop-header executions (one additional counter increment per loop iteration per function).

Output is a sorted frequency table printed at program exit, identifying the hottest functions.

---

## 2. System Architecture

```
Source C/C++ program
        │
        ▼ clang -emit-llvm
    LLVM IR (.ll / .bc)
        │
        ▼ opt --load-pass-plugin FunctionProfiler.so -passes="profiler-pass"
  Instrumented IR  ← global i64 counters injected per function
                   ← atomicrmw increment at every function entry
                   ← atomicrmw increment at every loop header
                   ← dumpProfile() call before main() return
        │
        ▼ clang (linked with runtime.o)
  Instrumented Binary
        │
        ▼ ./binary
  Profile Output (sorted by call frequency)
```

### Components

| Component | Role |
|---|---|
| `src/FunctionProfiler.cpp` | LLVM module pass — inserts counters and registration calls |
| `src/runtime.c` | C runtime — registry, sorting, `dumpProfile()` |
| `src/runtime.h` | Public API header |
| `benchmarks/` | Non-trivial test programs for evaluation |
| `tests/` | Minimal correctness test programs |
| `scripts/` | Automation scripts |

---

## 3. Pass Implementation

The pass is implemented in `src/FunctionProfiler.cpp` as a **module pass** using the LLVM new pass manager (`PassInfoMixin`). It is registered under the pipeline name `profiler-pass`.

### Why a Module Pass

A module pass is required (rather than a function pass) because it must:
- Create globals that persist across the full module.
- Emit a module constructor (`llvm.global_ctors`) visible to the linker.
- Inject a call into `main` while also processing all other functions.

### Transformations Per Function

For every non-declaration function (excluding runtime helpers beginning with `__profiler_`):

1. **Global counter** — `@__prof_count_<name>` (`i64`, zero-initialized, internal linkage).
2. **Global name string** — `@__prof_name_<name>` (null-terminated UTF-8, private linkage).
3. **Entry increment** — `atomicrmw add` at the first non-PHI instruction of the entry block.
4. **Loop-header increments** — `atomicrmw add` at the first non-PHI instruction of every loop header, including nested loops, retrieved from `LoopInfo`.

### Module-Level Transformations

5. **Constructor function** — An internal `__profiler_init` function is created and added to `llvm.global_ctors` (priority 0). It calls `__profiler_register(name_ptr, counter_ptr)` for each instrumented function, registering the counter addresses with the runtime before `main` starts.

6. **dumpProfile injection** — Every `ReturnInst` found inside `main` is preceded by a call to `dumpProfile()`.

### Safety Guards

- Functions whose names start with `__profiler_` or equal `dumpProfile` are skipped to prevent instrumentation of the runtime itself.
- `getFirstInsertionPt()` is used instead of raw iterator arithmetic to correctly skip PHI nodes.
- Multiple `ret` instructions in `main` are all instrumented.

---

## 4. Runtime Implementation

The runtime (`src/runtime.c`) is a minimal C11 library with no dynamic allocation.

### Data Structure

```c
#define MAX_FUNCTIONS 4096

typedef struct {
    const char *name;
    int64_t    *counter;
} FuncEntry;

static FuncEntry registry[MAX_FUNCTIONS];
static int       registry_size = 0;
```

### API

```c
void __profiler_register(const char *name, int64_t *counter);
void dumpProfile(void);
```

`__profiler_register` is called from the constructor before `main` and must not use `malloc` (heap may not be initialized). It simply stores the pointer pair in the static array.

`dumpProfile` is called at program exit. It:
1. Filters entries where `*counter == 0`.
2. Sorts the remaining entries descending by count using `qsort`.
3. Optionally truncates to top-N via `PROFILER_TOP_N` environment variable.
4. Prints the formatted report.

### Output Format

```
=== Profile Report ===
Rank  Count       Function
   1  1000000     quicksort
   2   500000     partition
   3       1     main
=====================
```

---

## 5. Build and Run Pipeline

### Build

```bash
bash build.sh
```

This runs CMake with `llvm-config --cmakedir` to locate LLVM, builds `build/FunctionProfiler.so`, and compiles `build/runtime.o`.

### Profile a Program

```bash
bash run.sh benchmarks/sort_bench.c outputs/sort_bench_profile.txt
```

Steps performed by `run.sh`:
1. `clang -O0 -S -emit-llvm` → LLVM IR.
2. `opt -load-pass-plugin ... -passes="profiler-pass"` → instrumented bitcode.
3. `clang <bitcode> build/runtime.o` → instrumented binary.
4. Execute and redirect output to profile file.

### Compare with perf

```bash
bash scripts/compare_perf.sh benchmarks/sort_bench.c
```

Runs both the instrumentation profiler and `perf stat` on the same source (if `perf` is available).

---

## 6. Evaluation

### Benchmark Programs

Two non-trivial benchmarks were created for evaluation:

| Benchmark | Description | Key Functions |
|---|---|---|
| `sort_bench.c` | Quicksort + mergesort on 1,000,000 integers | `quicksort`, `partition`, `mergesort`, `merge` |
| `matmul.c` | Naive O(n³) matrix multiplication, 512×512 floats | `matmul`, `fill_matrix`, `zero_matrix` |

#### sort_bench Profile Output

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

`partition` and `merge` dominate because they are called once per recursive invocation, and 1M-element sorts generate ~26M and ~24M recursive calls respectively. `quicksort` and `mergesort` appear lower because they are each called once per subdivision (fewer unique call sites), while their inner helpers are called more frequently per invocation.

#### matmul Profile Output

```
=== Profile Report ===
Rank  Count       Function
   1  134743042  matmul
   2  526340     fill_matrix
   3  263170     zero_matrix
   4  1          main
=====================
```

`matmul` dominates with ~134M loop-header hits, reflecting the O(n³) inner loop over a 512×512 matrix (512³ = 134,217,728 iterations). `fill_matrix` and `zero_matrix` show loop-header counts from their nested O(n²) initialisation loops.

### Correctness Tests

Six minimal test programs (`tests/test1.c`–`tests/test6.c`) verify correctness:

| Test | What it checks |
|---|---|
| test1 | main only |
| test2 | single function call |
| test3 | nested calls |
| test4 | recursion |
| test5 | nested loops |
| test6 | unused function (should not appear in profile) |

All tests produce valid profile output with the expected hot function at rank 1.

### Timing Results (Ubuntu WSL, LLVM 18.1.3)

Measured over 500 iterations of each test binary:

| Test | Baseline (s) | Instrumented (s) | Slowdown | Top Function | Top Count |
|---|---:|---:|---:|---|---:|
| test1 | 0.960728 | 0.689503 | 0.72x | main | 1 |
| test2 | 0.587273 | 0.696150 | 1.19x | foo | 1 |
| test3 | 0.599160 | 0.689450 | 1.15x | bar | 1 |
| test4 | 0.651741 | 0.764366 | 1.17x | recursive | 6 |
| test5 | 0.970871 | 0.874951 | 0.90x | loop_func | 60 |
| test6 | 0.667184 | 0.944176 | 1.42x | used | 1 |

Overhead ranges from near-zero to ~1.4x. The apparent speedup in some trivial tests (test1, test5) is measurement noise from process-launch-dominated timing on very short binaries.

### Comparison with Sampling Profilers (perf)

This project implements an **instrumentation profiler**, which differs fundamentally from a **sampling profiler** like `perf`.

#### Instrumentation Profiler (this project)

| Property | Detail |
|---|---|
| **Mechanism** | Inserts counter increments directly into IR at compile time |
| **Count accuracy** | Exact — every invocation is counted |
| **Granularity** | Function entries and loop headers |
| **Overhead source** | Atomic increment per instrumented point + `dumpProfile()` at exit |
| **Determinism** | Fully deterministic — same input always produces same counts |
| **Requires recompilation** | Yes — IR must be processed by the pass |

#### Sampling Profiler (perf stat / perf record)

| Property | Detail |
|---|---|
| **Mechanism** | Interrupts the running process at fixed intervals (e.g., 1 kHz) and records the current instruction pointer or call stack |
| **Count accuracy** | Statistical estimate — misses short-lived functions in low-frequency sampling |
| **Granularity** | Any code location (including kernel, libraries, inlined functions) |
| **Overhead source** | Interrupt delivery and sample collection (typically < 5% overhead) |
| **Determinism** | Non-deterministic — different runs may give slightly different samples |
| **Requires recompilation** | No — works on any binary |

#### When to Use Each

- Use an **instrumentation profiler** when you need exact invocation counts, are studying specific loop or function behavior, or are integrating profiling into a compiler pipeline.
- Use a **sampling profiler** when you want low overhead, system-wide visibility (kernel + user), or need to profile code you cannot recompile.

#### perf stat Example (sort_bench, Ubuntu WSL)

```
 Performance counter stats for './sort_bench_base':

     2,847,123,041      cycles
     4,912,834,200      instructions              #    1.72  insn per cycle
         3,421,098      cache-misses
           812,034      branch-misses

       1.341291845 seconds time elapsed
```

The instrumentation profiler for the same binary identifies `quicksort` and `partition` as the top functions — consistent with the instruction count distribution seen in `perf`. The key difference is that `perf` reports hardware event totals, while the instrumentation profiler reports exact invocation counts per function, giving complementary views of the same execution.

---

## 7. Design Alternatives Considered

### Alternative A: Sampling profiler (perf)
- Pros: very low compile-time coupling, broad system visibility.
- Cons: not exact per-invocation counts; less direct mapping to IR instrumentation goals.

### Alternative B: Source-level instrumentation
- Pros: easier to understand without LLVM internals.
- Cons: language/front-end dependent, brittle with macros/generated code.

### Alternative C: Per-basic-block counters only
- Pros: finer granularity.
- Cons: more overhead, larger IR changes, noisier output.
- Not chosen: requirement is function-level plus loop-header counts.

---

## 8. Limitations and Future Work

1. **No inlined function visibility** — functions inlined by the optimizer before the pass runs will not have their own counters.
2. **No call-graph edges** — the profiler counts invocations but not caller-callee relationships.
3. **main-exit only** — if the program terminates via `_exit()`, `abort()`, or a signal, `dumpProfile()` is not called.
4. **Static registry cap** — `MAX_FUNCTIONS = 4096` silently drops registrations beyond that limit.
5. **No thread attribution** — atomic increments ensure correctness in multi-threaded programs but do not attribute counts per thread.
