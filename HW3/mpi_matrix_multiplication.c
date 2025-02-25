/*
 * Mat Mult
 * CS698 GPU cluster programming - MPI + CUDA 
 * Spring 2025
 * template for HW1 - 3
 * HW1 - point to point communication
 * HW2 - collective communication
 * HW3 - one-sided communication
 * Andrew Sohn
 */

 #include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define COLOR 1<<10
#define MAXDIM 1<<12        /* 4096 */
#define ROOT 0

int mat_mult(double *A, double *B, double *C, int n, int n_local);
void init_data(double *data, int data_size);
int check_result(double *C, double *D, int n);

int main(int argc, char *argv[]) {
    int i, n = 64, n_sq, flag, my_work;
    int my_rank, num_procs = 1;
    double *A, *B, *C, *D;    /* D is for local computation */
    int elms_to_comm;
    double start_time, end_time, elapsed;

    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    if (argc > 1) {
        n = atoi(argv[1]);
        if (n > MAXDIM) n = MAXDIM;
    }
    n_sq = n * n;

    my_work = n / num_procs;
    elms_to_comm = my_work * n;

    A = (double *) malloc(sizeof(double) * n_sq);
    B = (double *) malloc(sizeof(double) * n_sq);
    C = (double *) malloc(sizeof(double) * n_sq);
    D = (double *) malloc(sizeof(double) * n_sq);

    if (my_rank == ROOT)
        printf("pid=%d: num_procs=%d n=%d my_work=%d\n", my_rank, num_procs, n, my_work);

    init_data(A, n_sq);
    init_data(B, n_sq);

    MPI_Win win_A, win_C;
    MPI_Win_create(A, n_sq * sizeof(double), sizeof(double), MPI_INFO_NULL, MPI_COMM_WORLD, &win_A);
    MPI_Win_create(C, n_sq * sizeof(double), sizeof(double), MPI_INFO_NULL, MPI_COMM_WORLD, &win_C);

    start_time = MPI_Wtime();

    // Synchronize before distributing data
    MPI_Win_fence(0, win_A);

    if (my_rank == ROOT) {
        // Distribute my_work rows of A to each process using MPI_Put
        for (i = 1; i < num_procs; i++) {
            MPI_Put(&A[i * my_work * n], elms_to_comm, MPI_DOUBLE, i, 0, elms_to_comm, MPI_DOUBLE, win_A);
        }
    }

    // Synchronize after distributing data
    MPI_Win_fence(0, win_A);

    // Broadcast B to all processes
    MPI_Bcast(B, n_sq, MPI_DOUBLE, ROOT, MPI_COMM_WORLD);

    // Each process computes its own mat mult
    mat_mult(A, B, C, n, my_work);

    // Synchronize before collecting results
    MPI_Win_fence(0, win_C);

    if (my_rank == ROOT) {
        // Collect my_work rows of C from each process using MPI_Get
        for (i = 1; i < num_procs; i++) {
            MPI_Get(&C[i * my_work * n], elms_to_comm, MPI_DOUBLE, i, 0, elms_to_comm, MPI_DOUBLE, win_C);
        }
    }

    // Synchronize after collecting results
    MPI_Win_fence(0, win_C);

    if (my_rank == ROOT) {
        end_time = MPI_Wtime();
        elapsed = end_time - start_time;

        // Local computation for comparison: results in D
        mat_mult(A, B, D, n, n);

        flag = check_result(C, D, n);
        if (flag) printf("Test: FAILED\n");
        else {
            printf("Test: PASSED\n");
            printf("Total time %d: %f seconds.\n", my_rank, elapsed);
        }
    }

    MPI_Win_free(&win_A);
    MPI_Win_free(&win_C);
    MPI_Finalize();
    return 0;
}

int mat_mult(double *a, double *b, double *c, int n, int my_work) {
    int i, j, k, sum = 0;
    for (i = 0; i < my_work; i++) {
        for (j = 0; j < n; j++) {
            sum = 0;
            for (k = 0; k < n; k++)
                sum = sum + a[i * n + k] * b[k * n + j];
            c[i * n + j] = sum;
        }
    }
    return 0;
}

/* Initialize an array with random data */
void init_data(double *data, int data_size) {
    for (int i = 0; i < data_size; i++)
        data[i] = rand() & 0xf;
}

/* Compare two matrices C and D */
int check_result(double *C, double *D, int n) {
    int i, j, flag = 0;
    double *cp, *dp;

    cp = C;
    dp = D;

    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            if (*cp++ != *dp++) {
                printf("ERROR: C[%d][%d]=%f != D[%d][%d]=%f\n", i, j, C[i * n + j], i, j, D[i * n + j]);
                flag = 1;
                return flag;
            }
        }
    }
    return flag;
}