// File: runtime.c
// Description: Runtime registry and reporting for instrumented profile counters.
// Part of: LLVM Profiling Pass Project

#include "runtime.h"

#include <stdio.h>
#include <stdlib.h>

#define MAX_FUNCTIONS 4096

typedef struct {
    const char *name;
    int64_t *counter;
} FuncEntry;

static FuncEntry registry[MAX_FUNCTIONS];
static int registry_size = 0;

void __profiler_register(const char *name, int64_t *counter) {
    if (!name || !counter) {
        return;
    }
    if (registry_size >= MAX_FUNCTIONS) {
        return;
    }

    registry[registry_size].name = name;
    registry[registry_size].counter = counter;
    registry_size++;
}

static int compare_entries(const void *a, const void *b) {
    const FuncEntry *ea = *(const FuncEntry *const *)a;
    const FuncEntry *eb = *(const FuncEntry *const *)b;
    int64_t va = *ea->counter;
    int64_t vb = *eb->counter;

    if (va > vb) {
        return -1;
    }
    if (va < vb) {
        return 1;
    }
    return 0;
}

void dumpProfile(void) {
    FuncEntry *sorted[MAX_FUNCTIONS];
    int visible = 0;

    for (int i = 0; i < registry_size; ++i) {
        if (*registry[i].counter > 0) {
            sorted[visible++] = &registry[i];
        }
    }

    qsort(sorted, (size_t)visible, sizeof(FuncEntry *), compare_entries);

    int top_n = visible;
    const char *top_n_env = getenv("PROFILER_TOP_N");
    if (top_n_env) {
        int parsed = atoi(top_n_env);
        if (parsed > 0 && parsed < top_n) {
            top_n = parsed;
        }
    }

    printf("=== Profile Report ===\n");
    printf("Rank  Count       Function\n");
    for (int i = 0; i < top_n; ++i) {
        printf("%4d  %-10lld %s\n", i + 1, (long long)*sorted[i]->counter, sorted[i]->name);
    }
    printf("=====================\n");
}