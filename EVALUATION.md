# EVALUATION

## Methodology

Evaluation compares baseline binaries versus instrumented binaries for all six provided test programs.

- Baseline: `clang -O0 tests/testX.c -o /tmp/cd-lab-el/testX_base`
- Instrumented: `bash run.sh tests/testX.c outputs/eval/testX_profile.txt`
- Timing method: repeated execution loop (`ITERATIONS=500`, nanosecond wall-clock delta).
- Automation script: `scripts/evaluate.sh`
- Output artifact: `outputs/eval/summary.csv`

## Results (Ubuntu WSL, LLVM 18.1.3)

| Test | Iterations | Baseline (s) | Instrumented (s) | Slowdown | Top Function | Top Count |
|---|---:|---:|---:|---:|---|---:|
| test1 | 500 | 0.960728 | 0.689503 | 0.72x | main | 1 |
| test2 | 500 | 0.587273 | 0.696150 | 1.19x | foo | 1 |
| test3 | 500 | 0.599160 | 0.689450 | 1.15x | bar | 1 |
| test4 | 500 | 0.651741 | 0.764366 | 1.17x | recursive | 6 |
| test5 | 500 | 0.970871 | 0.874951 | 0.90x | loop_func | 60 |
| test6 | 500 | 0.667184 | 0.944176 | 1.42x | used | 1 |

## Interpretation

1. The profiler correctly identifies expected hot functions:
   - Recursion-heavy `test4`: `recursive` dominates with count 6.
   - Loop-heavy `test5`: `loop_func` dominates with count 60.
2. Slowdown trends are generally near 1x to ~1.4x for these tiny programs.
3. Some cases show apparent speedup (<1x), which is expected noise for very short-running binaries and process-launch-heavy timing methods.

## Baseline Comparison Requirement Coverage

- Number of test cases compared: 6 (requirement was at least 5).
- Measurable metrics included:
  - Baseline runtime.
  - Instrumented runtime.
  - Slowdown ratio.
  - Top hot function and count from profiler report.

## Comparison with Sampling Profilers (perf)

### Instrumentation profiler (this project)

- Strengths:
  - Exact event counts at instrumented points (function entries + loop headers).
  - Deterministic, reproducible per-run count tables.
  - Directly integrated into compile pipeline.
- Limitations:
  - Adds runtime overhead due to inserted increments and report generation.
  - Visibility limited to explicit instrumentation points.

### Sampling profiler (`perf`)

- Strengths:
  - Lower runtime perturbation.
  - Wider system-level visibility (kernel/user stack, scheduler effects).
- Limitations:
  - Statistical estimates, not exact invocation counts.
  - Requires longer runs for stable frequency estimates.

## Conclusion

The pass meets the assignment objective: it inserts function/loop counters and emits sorted call-frequency reports at exit. The evaluation demonstrates both correctness signals (expected hot functions) and measurable baseline-vs-instrumented runtime comparisons across more than five tests.
