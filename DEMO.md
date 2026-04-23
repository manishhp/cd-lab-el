# DEMO

This document provides reproducible demo commands and captured outputs for both working and failure cases.

## Environment Used

- Ubuntu on WSL2
- LLVM/Clang/CMake toolchain installed via apt

## Working Case Demo

### Command

```bash
bash run.sh tests/test4.c
cat outputs/test4_profile.txt
```

### Captured Output

```text
Profile saved to /mnt/d/RVCE/6th sem/CD/Lab/project/outputs/test4_profile.txt
=== Profile Report ===
Rank  Count       Function
   1  6          recursive
   2  1          main
=====================
```

This shows successful instrumentation and sorted profile dump with expected recursion count behavior.

## Failure Case Demo

### Command

```bash
bash run.sh
```

### Captured Output

```text
Usage: run.sh <source.c> [output_profile.txt]
```

This demonstrates input validation and controlled failure handling when required arguments are missing.

## Optional Submission Media

If your course requires visual media files, add:

1. A short screen recording for the working case.
2. One screenshot/video segment for the failure case.

Recommended file names:

- `demo-working.mp4`
- `demo-failure.mp4`

Or screenshot alternatives:

- `demo-working.png`
- `demo-failure.png`

## Video Evidence Template (Fill Before Submission)

Use one of the following styles.

### Style A: Files Inside Repository

- Working demo: media/demo-working.mp4
- Failure demo: media/demo-failure.mp4

### Style B: External Links (Drive/YouTube Unlisted)

- Working demo: REPLACE_WITH_WORKING_DEMO_LINK
- Failure demo: REPLACE_WITH_FAILURE_DEMO_LINK

## Recording Checklist

For the working demo video, ensure the recording visibly includes:

1. `bash build.sh`
2. `bash run.sh tests/test4.c`
3. `cat outputs/test4_profile.txt`

For the failure demo video, ensure the recording visibly includes:

1. `bash run.sh`
2. Output: `Usage: run.sh <source.c> [output_profile.txt]`
