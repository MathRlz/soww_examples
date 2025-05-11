#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <unistd.h>  // For sleep function

#define BARRIER_TAG 420

void basic_barrier() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    int dummy_msg = 1; 
    MPI_Status status;
    
    // Phase 1: All to root
    if (rank != 0) {
        MPI_Send(&dummy_msg, 1, MPI_INT, 0, BARRIER_TAG, MPI_COMM_WORLD);
    } else {
        for (int i = 1; i < size; i++) {
            MPI_Recv(&dummy_msg, 1, MPI_INT, i, BARRIER_TAG, MPI_COMM_WORLD, &status);
        }
    }
    
    // Phase 2: Root to all
    if (rank == 0) {
        for (int i = 1; i < size; i++) {
            MPI_Send(&dummy_msg, 1, MPI_INT, i, BARRIER_TAG, MPI_COMM_WORLD);
        }
    } else {
        MPI_Recv(&dummy_msg, 1, MPI_INT, 0, BARRIER_TAG, MPI_COMM_WORLD, &status);
    }
}

void tree_barrier(int rank, int size) {
    int dummy = 1;

    // Compute children
    int left = 2 * rank + 1;
    int right = 2 * rank + 2;

    // ===== Fan-in Phase =====
    if (left < size) {
        MPI_Recv(&dummy, 1, MPI_INT, left, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    if (right < size) {
        MPI_Recv(&dummy, 1, MPI_INT, right, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    if (rank != 0) {
        int parent = (rank - 1) / 2;
        MPI_Send(&dummy, 1, MPI_INT, parent, 0, MPI_COMM_WORLD);
    }

    // ===== Fan-out Phase =====
    if (rank != 0) {
        int parent = (rank - 1) / 2;
        MPI_Recv(&dummy, 1, MPI_INT, parent, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    if (left < size) {
        MPI_Send(&dummy, 1, MPI_INT, left, 1, MPI_COMM_WORLD);
    }
    if (right < size) {
        MPI_Send(&dummy, 1, MPI_INT, right, 1, MPI_COMM_WORLD);
    }
}

int main(int argc, char** argv) {
    int rank, size;
    
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // Simulate different processing times for each process
    int process_time = rank + 1;  // Process 0 takes 1 second, Process 1 takes 2 seconds, etc.
    
    printf("Process %d: Starting phase 1, will take %d seconds\n", rank, process_time);
    sleep(process_time);  // Simulate work with different durations
    printf("Process %d: Completed phase 1\n", rank);
    
    //basic_barrier();
    tree_barrier(rank, size);
    
    // Phase 2 - All processes must start this together
    printf("Process %d: All processes synchronized!\n", rank);

    MPI_Finalize();
    return 0;
}