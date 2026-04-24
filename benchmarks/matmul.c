// File: matmul.c
// Description: Matrix multiplication benchmark — naive O(n³) on 512×512 matrices
// Part of: LLVM Profiling Pass Project

#include <stdio.h>
#include <stdlib.h>

#define N 512

static float A[N][N];
static float B[N][N];
static float C[N][N];

static void fill_matrix(float mat[N][N], float val) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            mat[i][j] = val + (float)(i * N + j) * 0.0001f;
}

static void zero_matrix(float mat[N][N]) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            mat[i][j] = 0.0f;
}

static void matmul(float a[N][N], float b[N][N], float c[N][N]) {
    for (int i = 0; i < N; i++)
        for (int k = 0; k < N; k++)
            for (int j = 0; j < N; j++)
                c[i][j] += a[i][k] * b[k][j];
}

int main(void) {
    fill_matrix(A, 1.0f);
    fill_matrix(B, 2.0f);
    zero_matrix(C);

    matmul(A, B, C);

    /* spot-check to prevent dead-code elimination */
    if (C[0][0] == 0.0f) {
        fprintf(stderr, "unexpected zero result\n");
        return 1;
    }
    return 0;
}
