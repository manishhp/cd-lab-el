#include <stdio.h>
#include <stdlib.h>

// Structure to hold counter info
struct Counter {
    const char* name;
    long long* value;
};

// Array to store registered counters
#define MAX_COUNTERS 100
static struct Counter counters[MAX_COUNTERS];
static int counter_count = 0;

// Function to register a counter
void registerCounter(const char* name, long long* value) {
    if (counter_count < MAX_COUNTERS) {
        counters[counter_count].name = name;
        counters[counter_count].value = value;
        counter_count++;
    }
}

// Comparison function for qsort (descending order)
int compare(const void* a, const void* b) {
    long long va = *(*(struct Counter**)a)->value;
    long long vb = *(*(struct Counter**)b)->value;
    if (va > vb) return -1;
    if (va < vb) return 1;
    return 0;
}

// Function to dump profile data
void dumpProfile() {
    // Sort counters by frequency (descending)
    struct Counter* sorted[MAX_COUNTERS];
    for (int i = 0; i < counter_count; i++) {
        sorted[i] = &counters[i];
    }
    qsort(sorted, counter_count, sizeof(struct Counter*), compare);

    printf("Function call counts (sorted by frequency):\n");
    for (int i = 0; i < counter_count; i++) {
        printf("%s: %lld\n", sorted[i]->name, *sorted[i]->value);
    }
}