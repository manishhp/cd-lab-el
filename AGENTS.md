# AGENTS.md — LLVM Profiling Pass Project

> **Purpose:** This file is the authoritative guide for any AI agent (or developer) working on this codebase. It defines the project structure, build conventions, coding rules, task breakdown, testing protocol, and contribution workflow.

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Repository Layout](#2-repository-layout)
3. [Agent Roles & Responsibilities](#3-agent-roles--responsibilities)
4. [Environment Setup](#4-environment-setup)
5. [Build Instructions](#5-build-instructions)
6. [Source File Contracts](#6-source-file-contracts)
7. [Coding Standards](#7-coding-standards)
8. [Task Checklist](#8-task-checklist)
9. [Testing Protocol](#9-testing-protocol)
10. [Running the Profiler End-to-End](#10-running-the-profiler-end-to-end)
11. [Common Errors & Fixes](#11-common-errors--fixes)
12. [Do's and Don'ts](#12-dos-and-donts)
13. [Glossary](#13-glossary)

---

## 1. Project Overview

This project implements a **compiler-level instrumentation profiler** using an LLVM pass. Unlike OS-level profilers (e.g., `perf`, `gprof`), this profiler operates at the **LLVM Intermediate Representation (IR)** level, inserting counters directly into the program before it is compiled to machine code.

### What the system does

| Component | Role |
|---|---|
| `ProfilerPass.cpp` | LLVM pass — inserts global counters, increments at function entries and loop headers |
| `profiler_rt.c` | C runtime — registers counter names and implements `dumpProfile()` |
| `benchmarks/` | Non-trivial test programs to profile |
| `CMakeLists.txt` | Build system for the pass plugin |

### High-level data flow

```
Source C/C++ program
        │
        ▼ clang -emit-llvm
    LLVM IR (.ll / .bc)
        │
        ▼ opt --load-pass-plugin ProfilerPass.so
  Instrumented IR  ← global i64 counters injected
                   ← increment at every function entry
                   ← increment at every loop header
                   ← dumpProfile() call before main() return
        │
        ▼ clang (linked with profiler_rt.o)
  Instrumented Binary
        │
        ▼ ./binary
  Profile Output (sorted by call frequency)
```

---

## 2. Repository Layout

```
llvm-profiler/
├── AGENTS.md                  ← (this file)
├── EXPLANATION.md             ← detailed technical explanation
├── CMakeLists.txt             ← builds ProfilerPass.so
├── ProfilerPass.cpp           ← the LLVM pass (main deliverable)
├── profiler_rt.c              ← C runtime with dumpProfile()
├── profiler_rt.h              ← header for runtime API
│
├── benchmarks/
│   ├── sort_bench.c           ← sorting benchmark (quicksort + mergesort)
│   ├── matmul.c               ← matrix multiplication benchmark
│   └── fib.c                  ← recursive Fibonacci (optional)
│
├── build/                     ← cmake build output (gitignored)
│   └── ProfilerPass.so        ← compiled pass plugin
│
├── outputs/                   ← profiler output logs (gitignored)
│   ├── sort_bench_profile.txt
│   └── matmul_profile.txt
│
└── scripts/
    ├── run_profiler.sh        ← end-to-end instrumentation script
    └── compare_perf.sh        ← runs perf alongside for comparison
```

> **Agent rule:** Never write generated files (`.so`, `.bc`, `.o`, profile logs) into the root directory. Always use `build/` or `outputs/`.

---

## 3. Agent Roles & Responsibilities

If multiple agents (or agents + humans) collaborate, divide work as follows:

### Agent A — Pass Author
- Owns `ProfilerPass.cpp`
- Responsible for: global counter creation, entry-block instrumentation, loop header detection, `dumpProfile()` injection at `main()` exit
- Must not touch `profiler_rt.c` or benchmark files

### Agent B — Runtime Author
- Owns `profiler_rt.c` and `profiler_rt.h`
- Responsible for: counter registration API, sorting logic, `dumpProfile()` implementation, thread-safety considerations
- Must not modify the pass

### Agent C — Benchmark & Evaluation
- Owns `benchmarks/`, `outputs/`, `scripts/`
- Responsible for: writing non-trivial benchmark programs, running the end-to-end pipeline, capturing profiler output, comparing with `perf`

### Agent D — Documentation
- Owns `EXPLANATION.md`, `AGENTS.md`, README sections
- Responsible for: technical writeup, perf comparison discussion, keeping docs in sync with code

> **Coordination rule:** Any interface change (e.g., the counter registration function signature in `profiler_rt.h`) must be agreed upon by Agents A and B before either modifies their file.

---

## 4. Environment Setup

### Required tools

| Tool | Minimum Version | Purpose |
|---|---|---|
| LLVM | 14.x or later | Pass compilation and `opt` tool |
| Clang | matching LLVM version | Compiling targets to IR |
| CMake | 3.16+ | Building the pass plugin |
| GCC or Clang | any recent | Compiling `profiler_rt.c` |
| Python 3 | 3.8+ | Helper scripts |
| `perf` (optional) | Linux kernel tool | Comparison profiler |

### Installation — Ubuntu/Debian

```bash
sudo apt update
sudo apt install llvm-17 llvm-17-dev clang-17 cmake build-essential python3
# Symlink if needed
sudo update-alternatives --install /usr/bin/llvm-config llvm-config /usr/bin/llvm-config-17 100
sudo update-alternatives --install /usr/bin/opt opt /usr/bin/opt-17 100
sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-17 100
```

### Installation — macOS

```bash
brew install llvm cmake
export PATH="$(brew --prefix llvm)/bin:$PATH"
export LDFLAGS="-L$(brew --prefix llvm)/lib"
export CPPFLAGS="-I$(brew --prefix llvm)/include"
```

### Verify setup

```bash
llvm-config --version    # should print 14.x, 15.x, 16.x, or 17.x
clang --version
opt --version
cmake --version
```

> **Agent rule:** Always run the verification step before beginning any task. If versions do not match, do not proceed — log the discrepancy.

---

## 5. Build Instructions

### Build the pass plugin

```bash
mkdir -p build && cd build
cmake .. -DLLVM_DIR=$(llvm-config --cmakedir) -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd ..
```

Expected output: `build/ProfilerPass.so` (Linux) or `build/ProfilerPass.dylib` (macOS).

### Build the runtime

```bash
clang -O1 -c profiler_rt.c -o build/profiler_rt.o
```

### CMakeLists.txt (reference)

```cmake
cmake_minimum_required(VERSION 3.16)
project(ProfilerPass)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM: ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

add_llvm_pass_plugin(ProfilerPass
    ProfilerPass.cpp
)
```

> **Agent rule:** Never hard-code LLVM paths in CMakeLists.txt. Always use `llvm-config` or `find_package(LLVM)`.

---

## 6. Source File Contracts

### `ProfilerPass.cpp`

The pass must be registered under the name `"profiler-pass"` so it can be invoked as:
```bash
opt -passes="profiler-pass" ...
```

**Required transformations:**

| Transformation | Location | Implementation |
|---|---|---|
| Create `@__prof_count_<funcname>` global | Module level, once per function | `Module::getOrInsertGlobal` |
| Create `@__prof_name_<funcname>` global | Module level, string constant | `IRBuilder::CreateGlobalString` |
| Increment counter at entry | First non-PHI instruction of entry block | `IRBuilder::CreateAtomicRMW` or load/add/store |
| Increment counter at loop headers | First non-PHI of each loop header BB | `LoopInfo` analysis |
| Register counter with runtime | Module constructor (`llvm.global_ctors`) | Call `__profiler_register(name_ptr, counter_ptr)` |
| Inject `dumpProfile()` call | Before every `ret` in `main` | `IRBuilder::CreateCall` |

**Pass structure (new pass manager):**

```cpp
struct ProfilerPass : public PassInfoMixin<ProfilerPass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
};
```

**Do not use the legacy pass manager.** LLVM 14+ deprecates it.

---

### `profiler_rt.c`

**Required functions:**

```c
// Called by the pass-injected constructor for each instrumented function
void __profiler_register(const char *name, int64_t *counter);

// Called at main() exit — sorts and prints the profile
void dumpProfile(void);
```

**Internal data structure:**

```c
#define MAX_FUNCTIONS 4096

typedef struct {
    const char  *name;
    int64_t     *counter;
} FuncEntry;

static FuncEntry registry[MAX_FUNCTIONS];
static int       registry_size = 0;
```

**Sorting:** use `qsort` with a comparator on `*counter` descending. Print top-N (default: all entries with count > 0).

**Output format (strict — tests depend on this):**

```
=== Profile Report ===
Rank  Count       Function
   1  1000000     quicksort
   2   500000     partition
   3       1     main
=====================
```

---

### `profiler_rt.h`

```c
#ifndef PROFILER_RT_H
#define PROFILER_RT_H

#include <stdint.h>

void __profiler_register(const char *name, int64_t *counter);
void dumpProfile(void);

#endif
```

---

## 7. Coding Standards

### C++ (ProfilerPass.cpp)

- Standard: **C++17**
- Naming: follow LLVM conventions — `CamelCase` for types, `camelCase` for variables and functions
- No raw `new`/`delete` — use LLVM's allocators or stack allocation
- No `printf` in the pass — use `llvm::errs()` for debug output, guarded by a debug flag
- All LLVM API calls must check return values where applicable (e.g., `getOrInsertFunction` results)
- Use `IRBuilder<>` for all instruction insertion — never manually construct `Instruction` objects

### C (profiler_rt.c)

- Standard: **C11**
- No dynamic memory allocation at runtime (use static arrays)
- `__profiler_register` must be **async-signal-safe** (no malloc, no locks if possible)
- `dumpProfile` will be called once at program exit — it is allowed to use `malloc`/`qsort`/`printf`
- All functions beginning with `__profiler_` are reserved for internal use — do not expose them unnecessarily

### General

- Every source file must have a header comment block:
  ```
  // File: <filename>
  // Description: <one line>
  // Part of: LLVM Profiling Pass Project
  ```
- Commit messages: `[component] short description` (e.g., `[pass] add loop header instrumentation`)
- No commented-out code in final submissions — use git history instead

---

## 8. Task Checklist

Use this checklist to track progress. Agents should mark items when complete.

### Pass (`ProfilerPass.cpp`)
- [ ] Project scaffolding — CMakeLists, skeleton pass, confirm plugin loads
- [ ] Global counter creation per function
- [ ] Global name string creation per function
- [ ] Entry-block increment instruction insertion
- [ ] Loop header detection and increment insertion (requires `LoopAnalysis`)
- [ ] Module-level constructor injection (`llvm.global_ctors`)
- [ ] `__profiler_register` call injected per function in constructor
- [ ] `dumpProfile()` call injected before `ret` in `main`
- [ ] Pass handles functions with no loops gracefully
- [ ] Pass handles `main` returning via multiple `ret` instructions
- [ ] Pass skips injected functions (avoid instrumenting `dumpProfile` itself)

### Runtime (`profiler_rt.c`)
- [ ] `FuncEntry` registry with static array
- [ ] `__profiler_register` implementation
- [ ] `dumpProfile` with `qsort`-based sorting
- [ ] Formatted output matching the required format above
- [ ] Top-N configurable via `PROFILER_TOP_N` environment variable

### Benchmarks
- [ ] `sort_bench.c` — implements quicksort and mergesort on 1M integers
- [ ] `matmul.c` — implements naive O(n³) matrix multiply on 512×512 matrices
- [ ] Both benchmarks instrumented and profiler output captured to `outputs/`

### Evaluation
- [ ] Profile output for `sort_bench` saved and discussed
- [ ] Profile output for `matmul` saved and discussed
- [ ] `perf stat` run on both benchmarks for comparison
- [ ] Discussion written in `EXPLANATION.md` section 6

---

## 9. Testing Protocol

### Unit test: pass loads

```bash
echo 'define i32 @main() { ret i32 0 }' > /tmp/trivial.ll
opt -load-pass-plugin ./build/ProfilerPass.so \
    -passes="profiler-pass" \
    /tmp/trivial.ll -o /tmp/trivial_inst.bc
llvm-dis /tmp/trivial_inst.bc | grep __prof_count
# Expected: at least one @__prof_count_main global visible
```

### Unit test: counter increments at entry

```bash
llvm-dis /tmp/trivial_inst.bc | grep -A5 "define i32 @main"
# Expected: atomicrmw or load/add/store sequence before the ret
```

### Unit test: dumpProfile injected

```bash
llvm-dis /tmp/trivial_inst.bc | grep dumpProfile
# Expected: call void @dumpProfile() before ret i32
```

### Integration test: sort benchmark

```bash
bash scripts/run_profiler.sh benchmarks/sort_bench.c outputs/sort_bench_profile.txt
cat outputs/sort_bench_profile.txt
# Expected: quicksort/partition appear as top entries with high counts
```

### Integration test: matmul benchmark

```bash
bash scripts/run_profiler.sh benchmarks/matmul.c outputs/matmul_profile.txt
cat outputs/matmul_profile.txt
# Expected: inner multiply loop body / helper function at top
```

---

## 10. Running the Profiler End-to-End

The script `scripts/run_profiler.sh` automates all steps:

```bash
#!/usr/bin/env bash
# Usage: run_profiler.sh <source.c> <output_profile.txt>

SOURCE=$1
OUTPUT=$2
BASE=$(basename "$SOURCE" .c)

# Step 1: Lower to LLVM IR
clang -O1 -S -emit-llvm "$SOURCE" -o /tmp/"$BASE".ll

# Step 2: Run the pass
opt -load-pass-plugin ./build/ProfilerPass.so \
    -passes="profiler-pass" \
    /tmp/"$BASE".ll -o /tmp/"$BASE"_inst.bc

# Step 3: Link with runtime
clang /tmp/"$BASE"_inst.bc build/profiler_rt.o -o /tmp/"$BASE"_profiled

# Step 4: Run and capture output
/tmp/"$BASE"_profiled > "$OUTPUT" 2>&1

echo "Profile saved to $OUTPUT"
```

### Manual step-by-step (for debugging)

```bash
# 1. Emit IR
clang -O1 -S -emit-llvm benchmarks/sort_bench.c -o /tmp/sort.ll

# 2. Inspect IR before instrumentation
cat /tmp/sort.ll

# 3. Run pass (verbose)
opt -load-pass-plugin ./build/ProfilerPass.so \
    -passes="profiler-pass" \
    -debug-only=profiler-pass \
    /tmp/sort.ll -o /tmp/sort_inst.bc

# 4. Disassemble to check instrumentation
llvm-dis /tmp/sort_inst.bc -o /tmp/sort_inst.ll
grep -n "__prof_count\|dumpProfile\|__profiler_register" /tmp/sort_inst.ll

# 5. Compile + link
clang /tmp/sort_inst.bc build/profiler_rt.o -o /tmp/sort_profiled

# 6. Run
/tmp/sort_profiled
```

---

## 11. Common Errors & Fixes

| Error | Likely Cause | Fix |
|---|---|---|
| `Pass not registered` | Plugin not loaded or pass name mismatch | Check `-passes="profiler-pass"` matches `getPluginInfo()` registration |
| `undefined symbol: __profiler_register` | Runtime not linked | Add `build/profiler_rt.o` to link command |
| `dumpProfile not called` | Pass did not find `main` or `main` has no `ret` | Check IR — ensure `main` compiles with `ret i32` |
| `Segfault in dumpProfile` | Registry overflow (`registry_size >= MAX_FUNCTIONS`) | Increase `MAX_FUNCTIONS` or add bounds check |
| `Counter always 0` | Atomicity issue or wrong pointer passed to register | Verify `__profiler_register` receives the address of the global, not its value |
| `opt: error: unknown pass name` | Wrong pass manager API | Ensure pass uses new PM (`PassInfoMixin`) and is registered via `PassBuilder` callback |
| CMake can't find LLVM | `LLVM_DIR` not set | Run `cmake .. -DLLVM_DIR=$(llvm-config --cmakedir)` |

---

## 12. Do's and Don'ts

### Do's ✅
- **Do** use `AtomicRMWInst::BinOp::Add` for counter increments — programs may be multi-threaded
- **Do** skip instrumenting functions whose names start with `__profiler_` to avoid infinite recursion
- **Do** handle the case where `main` does not exist (library compilation) — the pass should proceed without injecting `dumpProfile`
- **Do** use `DebugLoc` preservation — call `I.setDebugLoc(Builder.getCurrentDebugLocation())` after inserting instructions
- **Do** test on both `-O0` and `-O1` IR — optimization can change basic block structure

### Don'ts ❌
- **Don't** use `new PM` and `legacy PM` APIs in the same pass — pick one (use new PM)
- **Don't** insert instructions into PHI-node positions — always call `getFirstNonPHI()` before building
- **Don't** assume `entry` is the only predecessor of a loop header
- **Don't** use `malloc` inside `__profiler_register` — it may be called from a constructor before the heap is initialized
- **Don't** modify the `LoopInfo` structure during the pass — only read it

---

## 13. Glossary

| Term | Definition |
|---|---|
| **LLVM IR** | LLVM's platform-independent intermediate representation, similar to a typed assembly language |
| **Pass** | A transformation or analysis that runs over LLVM IR |
| **Module Pass** | A pass that operates on the entire program (all functions) at once |
| **Function Pass** | A pass that operates on one function at a time |
| **Basic Block (BB)** | A straight-line sequence of instructions with one entry and one exit |
| **Entry Block** | The first basic block of a function, always executed when the function is called |
| **Loop Header** | The basic block that controls a loop — the one with the back-edge from the loop body |
| **IRBuilder** | LLVM utility class for inserting new instructions at a given point |
| **LoopInfo** | LLVM analysis that identifies loops and their structure in a function |
| **PHI node** | A special IR instruction that selects a value based on which predecessor block was executed |
| **`opt`** | LLVM's optimization/transformation tool — used to run passes on IR files |
| **`llvm.global_ctors`** | Special LLVM global that lists functions to call before `main` (C++ static initializers) |
| **Instrumentation** | The act of inserting extra code into a program for measurement purposes |
| **PGO** | Profile-Guided Optimization — using profile data to drive compiler optimizations |
| **Sampling profiler** | A profiler (like `perf`) that periodically interrupts the program to record the current instruction pointer |
| **Instrumentation profiler** | A profiler (like this project) that inserts counting code directly into the program |
