# IMPLEMENTATION

## LLVM Plugin Registration

The pass is implemented in `src/FunctionProfiler.cpp` and exported via `llvmGetPassPluginInfo()`. It registers pipeline name `profiler-pass`, allowing:

```bash
opt -load-pass-plugin build/FunctionProfiler.so -passes="profiler-pass" input.ll -o output.bc
```

## Pass Type and Analyses

- Pass type: module pass using `PassInfoMixin<FunctionProfilerPass>`.
- Accesses `FunctionAnalysisManager` through `FunctionAnalysisManagerModuleProxy`.
- Uses `LoopAnalysis` to find loop headers per function.

## IR Transformations

For each non-declaration function (excluding runtime helpers):

1. Create/lookup global counter:
   - Name: `__prof_count_<sanitized_function_name>`
   - Type: `i64`
   - Initial value: 0

2. Create function-name global string:
   - Name: `__prof_name_<sanitized_function_name>`
   - Value: original function name bytes with null terminator.

3. Insert function-entry increment:
   - Insert at `getFirstInsertionPt()` of entry block.
   - Instruction style: `atomicrmw add` monotonic by 1.

4. Insert loop-header increments:
   - For each loop from `LoopInfo` (including nested loops), insert increment at header `getFirstInsertionPt()`.

5. Register counters through module constructor:
   - Build internal `__profiler_init` function.
   - In constructor body call:
     `__profiler_register(name_ptr, count_ptr)`
   - Add to global constructors via `appendToGlobalCtors`.

6. Inject profile dump at process end:
   - Find `main` definition.
   - For each `ReturnInst` in `main`, insert call `dumpProfile()` before return.

## Runtime Details

Implemented in `src/runtime.c`:

- Static registry capacity: 4096 entries.
- API:
  - `void __profiler_register(const char *name, int64_t *counter);`
  - `void dumpProfile(void);`
- `dumpProfile` flow:
  1. Collect entries with `*counter > 0`.
  2. Sort descending by counter using `qsort`.
  3. Optional truncation with environment variable `PROFILER_TOP_N`.
  4. Print table with rank, count, function name.

## Build and Run Pipeline

1. `build.sh`
   - Configure and build plugin with CMake and `llvm-config --cmakedir`.
   - Compile runtime object `build/runtime.o`.

2. `run.sh <source.c> [profile_out]`
   - Emit source LLVM IR (`clang -O0 -S -emit-llvm`).
   - Run pass with `opt` to produce instrumented bitcode.
   - Link instrumented bitcode with runtime object.
   - Execute profiled binary and redirect report to output file.

## Script Robustness Notes

- Scripts were validated in Ubuntu WSL.
- Command continuation formatting was corrected to avoid bash parsing errors in multiline invocations.
