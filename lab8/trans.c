/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 *
 * Name: Jianbang Yang
 * ID :515030910223
 *
 */
#include <stdio.h>
#include "cachelab.h"

/* begin 0f block*/
#define BOB -1

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    /* Total var numbers: 12 */
    int i, j, tmp;
    int block_i, block_j;
    int diagonal_pos;
    int buffer_i, buffer_j;
    int next_buffer_i, next_buffer_j;

    /* Strategy for 32x32: only left cold miss. a 32-byte block can contain 8 ints,
     * so devide the matrix into 8x8 submatrix.
     */
    if (M == 32 && N == 32) {
        for (block_i = 0; block_i < N; block_i += 8) {
            for (block_j = 0; block_j < M; block_j += 8) {
                for (i = block_i; i < block_i + 8; i++) {
                    for (j = block_j; j < block_j + 8; j++) {
                        /* The elements with the same index in A and B are stored in the same set
                         * which means we have to deal with the diagonal.
                         */
                        if (i != j) {
                            B[j][i] = A[i][j];
                        }
                        else {
                            tmp = A[i][j];
                            diagonal_pos = i;
                        }
                    }
                    /* only the diagonal submatrix have to do the operation below */
                    if (block_i == block_j) {
                        B[diagonal_pos][diagonal_pos] = tmp;
                    }
                }
            }
        }
    }

    /* Strategy for 64x64: devide matrix into 8x8 blocks. Generally use the next block that will
     * written as a buffer block and then move the datas to the current block. A buffer(except 
     * the diagonal blocks) should hold the conditions below:
     *      1. It is the next block will be written in B.
     *      2. It's column index should be different from the current read/wrriten block of A or B
     */
    else if (M == 64 && N == 64) {
        block_i = 0;
        block_j = 0;
        while (1) {

            /* get buffer block */
            if (block_i == block_j && block_i < 48) {
                buffer_i = -1;
                buffer_j = -1;

            }
            else if (block_i == block_j && block_i >= 48) {
                buffer_i = 0;
                buffer_j = 8;
            }
            else if (block_j > block_i && block_j < 56) {
                buffer_i = block_i;
                buffer_j = block_j + 8;
            }
            else if (block_j > block_i && block_j == 56 && block_i < 40) {
                buffer_i = block_i + 8;
                buffer_j = block_i + 16;
            }
            else if (block_j > block_i && block_j == 56 && block_i >= 40) {
                buffer_i = 56;
                buffer_j = 36;
            }
            else if (block_j < block_i && block_j >= 8) {
                buffer_i = block_i;
                buffer_j = block_j - 8;
            }
            else if (block_j < block_i && block_j == 0 && block_i >= 24) {
                buffer_i = block_i - 8;
                buffer_j = block_i - 16;
            }
            else {
                buffer_i = -1;
                buffer_j = -1;
            }

            /* Get buffer blocks index - the next two blocks will be the buffer blocks */
            if (!(buffer_i == -1 || buffer_j == -1)) {
                /* store the first 4 lines to the first buffer block */
                for (i = 0; i < 4; i++) {
                    for (j = 0; j < 8; j++) {
                        if (j < 4) {
                            B[block_i + j][block_j + i] = A[block_j + i][block_i + j];
                        }
                        else {
                            B[buffer_i + j - 4][buffer_j + i] = A[block_j + i][block_i + j];
                        }
                    }
                }

                /* store the next 4 lines to the second buffer block */
                for (i = 4; i < 8; i++) {
                    for (j = 0; j < 8; j++) {
                        if (j < 4) {
                            B[block_i + j][block_j + i] = A[block_j + i][block_i + j];
                        }
                        else {
                            B[buffer_i + j - 4][buffer_j + i] = A[block_j + i][block_i + j];
                        }
                    }
                }

                /* write buffer into B */
                for (i = 0; i < 4; i++) {
                    for (j = 0; j < 8; j++) {
                        B[block_i + i + 4][block_j + j] = B[buffer_i + i][buffer_j + j];
                    }
                }

            }
            else if (block_i == block_j) {
                if (block_i <= 40) {
                    buffer_i = 48;
                    buffer_j = 48;
                    next_buffer_i = 56;
                    next_buffer_j = 56;
                }
                else {
                    buffer_i = 0;
                    buffer_j = 8;
                    next_buffer_i = 0;
                    next_buffer_j = 16;
                }
                for (i = 0; i < 4; i++) {
                    for (j = 0; j < 8; j++) {
                        if (j < 4) {
                            B[buffer_i + j][buffer_j + i] = A[block_j + i][block_i + j];
                        }
                        else {
                            B[buffer_i + j - 4][buffer_j + 4 + i] = A[block_j + i][block_i + j];
                        }
                    }
                }

                /* store the next 4 lines to the second buffer block */
                for (i = 4; i < 8; i++) {
                    for (j = 0; j < 8; j++) {
                        if (j < 4) {
                            B[next_buffer_i + j][next_buffer_j + i - 4] = A[block_j + i][block_i + j];
                        }
                        else {
                            B[next_buffer_i + j - 4][next_buffer_j + i] = A[block_j + i][block_i + j];
                        }
                    }
                }

                /* write buffer into B */
                for (i = 0; i < 8; i++) {
                    for (j = 0; j < 8; j++) {
                        if (i < 4 && j < 4) {
                            B[block_i + i][block_j + j] = B[buffer_i + i][buffer_j + j];
                        }
                        else if (i < 4 && j >= 4) {
                            B[block_i + i][block_j + j] = B[next_buffer_i + i][next_buffer_j + j - 4];
                        }
                        else if (i >= 4 && j < 4) {
                            B[block_i + i][block_j + j] = B[buffer_i + i - 4][buffer_j + j + 4];
                        }
                        else if (i >= 4 && j >= 4) {
                            B[block_i + i][block_j + j] = B[next_buffer_i + i - 4][next_buffer_j + j];
                        }
                    }
                }
            }
            else {
                for (i = block_i; i < block_i + 8; i++) {
                    for (j = block_j; j < block_j + 8; j++) {
                        /* The elements with the same index in A and B are stored in the same set
                         * which means we have to deal with the diagonal.
                         */
                        B[i][j] = A[j][i];
                    }
                }
            }

            /* get next block */
            if (block_i == block_j && block_i < 56) {
                block_i += 8;
                block_j += 8;
            }
            else if (block_i == block_j && block_i >= 56) {
                block_i = 0;
                block_j = 8;
            }
            else if (block_j > block_i && block_j < 56) {
                block_j += 8;
            }
            else if (block_j > block_i && block_j >= 56 && block_i < 48) {
                block_j = block_i + 16;
                block_i += 8;
            }
            else if (block_j > block_i && block_j >= 56 && block_i >= 48) {
                block_i = 56;
                block_j = 48;
            }
            else if (block_j < block_i && block_j > 0) {
                block_j -= 8;
            }
            else if (block_j < block_i && block_j == 0 && block_i > 8) {
                block_j = block_i - 16;
                block_i -= 8;
            }
            else {
                break;
            }

        }

    }

    /* Strategy for 61x67: use naive strategy as 32x32 becasue the datas are not tidy */
    else if (M == 61 && N == 67) {
        for (block_i = 0; block_i < N; block_i += 17) {
            for (block_j = 0; block_j < M; block_j += 17) {
                for (i = block_i; i < block_i + 17; i++) {
                    if (i >= N) {
                        break;
                    }
                    for (j = block_j; j < block_j + 17 ; j++) {
                        /* The elements with the same index in A and B are stored in the same set
                         * which means we have to deal with the diagonal.
                         */
                        if (j >= M) {
                            break;
                        }
                        if (i != j) {
                            B[j][i] = A[i][j];
                        }
                        else {
                            tmp = A[i][j];
                            diagonal_pos = i;
                        }
                    }
                    /* only the diagonal submatrix have to do the operation below */
                    if (block_i == block_j) {
                        B[diagonal_pos][diagonal_pos] = tmp;
                    }
                }
            }
        }
    }

    else {
        for (i = 0; i < N; i++) {
            for (j = 0; j < M; j++) {
                tmp = A[j][i];
                B[i][j] = tmp;
            }
        }
    }
}

/*
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started.
 */

/*
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc);

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc);

}

/*
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

