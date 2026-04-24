// File: sort_bench.c
// Description: Sorting benchmark — quicksort and mergesort on 1M integers
// Part of: LLVM Profiling Pass Project

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define N 1000000

static int buf_a[N];
static int buf_b[N];
static int tmp[N];

static void fill_random(int *arr, int n) {
    for (int i = 0; i < n; i++)
        arr[i] = rand();
}

/* --- Quicksort --- */

static int partition(int *arr, int lo, int hi) {
    int pivot = arr[hi];
    int i = lo - 1;
    for (int j = lo; j < hi; j++) {
        if (arr[j] <= pivot) {
            i++;
            int t = arr[i]; arr[i] = arr[j]; arr[j] = t;
        }
    }
    int t = arr[i + 1]; arr[i + 1] = arr[hi]; arr[hi] = t;
    return i + 1;
}

static void quicksort(int *arr, int lo, int hi) {
    if (lo < hi) {
        int p = partition(arr, lo, hi);
        quicksort(arr, lo, p - 1);
        quicksort(arr, p + 1, hi);
    }
}

/* --- Mergesort --- */

static void merge(int *arr, int lo, int mid, int hi) {
    int n1 = mid - lo + 1;
    int n2 = hi - mid;
    memcpy(tmp,       arr + lo,      n1 * sizeof(int));
    memcpy(tmp + n1,  arr + mid + 1, n2 * sizeof(int));
    int i = 0, j = n1, k = lo;
    while (i < n1 && j < n1 + n2) {
        if (tmp[i] <= tmp[j]) arr[k++] = tmp[i++];
        else                  arr[k++] = tmp[j++];
    }
    while (i < n1)      arr[k++] = tmp[i++];
    while (j < n1 + n2) arr[k++] = tmp[j++];
}

static void mergesort(int *arr, int lo, int hi) {
    if (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        mergesort(arr, lo, mid);
        mergesort(arr, mid + 1, hi);
        merge(arr, lo, mid, hi);
    }
}

int main(void) {
    srand(42);
    fill_random(buf_a, N);
    memcpy(buf_b, buf_a, N * sizeof(int));

    quicksort(buf_a, 0, N - 1);
    mergesort(buf_b, 0, N - 1);

    /* sanity: both sorted results must match */
    for (int i = 0; i < N; i++) {
        if (buf_a[i] != buf_b[i]) {
            fprintf(stderr, "mismatch at %d\n", i);
            return 1;
        }
    }
    return 0;
}
