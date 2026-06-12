#include <instrumentation_control.h>

#include <stdlib.h>

int** __attribute__((noinline)) CreateMatrix(int rows, int cols) {
    int** matrix = (int**)malloc(rows * sizeof(int*));
    for (int i = 0; i < rows; i++) {
        matrix[i] = (int*)malloc(cols * sizeof(int));
    }
    return matrix;
}

void __attribute__((noinline)) FillMatrix(int** matrix, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            matrix[i][j] = i + j;
        }
    }
}

void __attribute__((noinline)) FreeMatrix(int** matrix, int rows) {
    for (int i = 0; i < rows; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

void __attribute__((noinline)) Multiply(int** A, int** B, int** C, int rowsA, int colsA, int colsB) {
    for (int i = 0; i < rowsA; i++) {
        for (int j = 0; j < colsB; j++) {
            C[i][j] = 0;
            for (int k = 0; k < colsA; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

int main() {
    int rowsA = 1000;
    int colsA = 1000;
    int rowsB = 1000;
    int colsB = 1000;

    int** A = CreateMatrix(rowsA, colsA);
    int** B = CreateMatrix(rowsB, colsB);
    int** C = CreateMatrix(rowsA, colsB);

    FillMatrix(A, rowsA, colsA);
    FillMatrix(B, rowsB, colsB);

    BeginInstrumentationBlock();

    Multiply(A, B, C, rowsA, colsA, colsB);

    EndInstrumentationBlock();

    FreeMatrix(A, rowsA);
    FreeMatrix(B, rowsB);
    FreeMatrix(C, rowsA);

    return 0;
}