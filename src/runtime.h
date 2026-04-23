// File: runtime.h
// Description: Public runtime API used by LLVM-instrumented programs.
// Part of: LLVM Profiling Pass Project

#ifndef RUNTIME_H
#define RUNTIME_H

#include <stdint.h>

void __profiler_register(const char *name, int64_t *counter);
void dumpProfile(void);

#endif
