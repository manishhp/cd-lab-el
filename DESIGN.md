# DESIGN

## Problem Statement

Build a compiler-level profiling instrumentor that counts:

1. Function entries.
2. Loop-header executions.

The profiler must inject instrumentation into LLVM IR and print a sorted frequency report at program exit.

## Design Goals

1. Keep instrumentation fully automatic at compile time.
2. Avoid source-level changes to profiled programs.
3. Keep runtime simple and portable C.
4. Make output deterministic and easy to validate.

## Chosen Approach

### Pass architecture

- Use LLVM new pass manager plugin (`add_llvm_pass_plugin` + `PassInfoMixin` module pass).
- Iterate module functions and skip declarations/runtime helpers.
- Create one global `i64` counter per function.
- Inject increments:
  - At function entry block first insertion point.
  - At each loop header (from `LoopInfo`).
- Build a constructor (`llvm.global_ctors`) that registers `(name, counter*)` with runtime.
- Inject `dumpProfile()` before each return in `main`.

### Runtime architecture

- Static registry array storing `(const char *name, int64_t *counter)`.
- Registration API called from constructor-created code.
- `dumpProfile()` filters zero counters, sorts by descending count (`qsort`), prints formatted report.
- Optional top-N limit via environment variable (`PROFILER_TOP_N`).

## Why This Design

1. Module pass is ideal because it can:
   - See all functions.
   - Create globals once per function.
   - Emit constructor and cross-function runtime calls.
2. Counter globals are stable addresses, so runtime can read final counts without synchronization APIs.
3. Constructor registration avoids manual registration code in user programs.
4. Main-return injection guarantees report generation in normal exit paths.

## Alternatives Considered

## Alternative A: Sampling profiler (perf)

- Pros: very low compile-time coupling, broad system visibility.
- Cons: not exact per-invocation counts; less direct mapping to IR instrumentation goals.
- Reason not chosen: assignment requires compiler-inserted exact counters.

## Alternative B: Source-level instrumentation

- Pros: easier to understand without LLVM internals.
- Cons: language/front-end dependent, brittle with macros/generated code.
- Reason not chosen: assignment target is LLVM IR-level pass.

## Alternative C: Per-basic-block counters only

- Pros: finer granularity.
- Cons: more overhead, larger IR changes, noisier output.
- Reason not chosen: requirement is function-level plus loop-header counts.

## Risks and Mitigations

1. Risk: instrumenting runtime functions causes recursion/noise.
   - Mitigation: skip `__profiler_*` and `dumpProfile` names.
2. Risk: malformed symbol names from unusual function names.
   - Mitigation: sanitize names for generated globals.
3. Risk: missing output if `main` has multiple returns.
   - Mitigation: inject `dumpProfile()` before each `ret` in `main`.
4. Risk: command fragility across Windows/WSL line endings.
   - Mitigation: shell scripts normalized and tested on Ubuntu WSL.
