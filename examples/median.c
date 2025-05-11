#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

void merge(int *arr, int *left, int left_count, int *right, int right_count) {
  int i = 0, j = 0, k = 0;
  while (i < left_count && j < right_count) {
    if (left[i] < right[j])
      arr[k++] = left[i++];
    else
      arr[k++] = right[j++];
  }
  while (i < left_count)
    arr[k++] = left[i++];
  while (j < right_count)
    arr[k++] = right[j++];
}

void mergesort(int *arr, int n) {
  if (n < 2)
    return;
  int mid = n / 2;
  int *left = (int *)malloc(mid * sizeof(int));
  int *right = (int *)malloc((n - mid) * sizeof(int));
  for (int i = 0; i < mid; i++)
    left[i] = arr[i];
  for (int i = mid; i < n; i++)
    right[i - mid] = arr[i];
  mergesort(left, mid);
  mergesort(right, n - mid);
  merge(arr, left, mid, right, n - mid);
  free(left);
  free(right);
}

int main(int argc, char **argv) {
  int rank, size;
  int *data = NULL;
  int *local_data = NULL;
  int N;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (argc < 2) {
    if (rank == 0)
      printf("Usage: %s <array_size>\n", argv[0]);
    MPI_Finalize();
    return 1;
  }
  N = atoi(argv[1]);

  // Calculate send counts and displacements for scatterv/gatherv
  int *sendcounts = malloc(size * sizeof(int));
  int *displs = malloc(size * sizeof(int));
  int base = N / size, rem = N % size;
  int offset = 0;
  for (int i = 0; i < size; i++) {
    sendcounts[i] = base + (i < rem ? 1 : 0);
    displs[i] = offset;
    offset += sendcounts[i];
  }

  // Allocate and initialize data on root
  if (rank == 0) {
    data = (int *)malloc(N * sizeof(int));
    srand(42);
    for (int i = 0; i < N; i++) {
      data[i] = rand() % 100;
    }
  }

  // Allocate local array
  local_data = (int *)malloc(sendcounts[rank] * sizeof(int));

  double start_time = MPI_Wtime();

  // Scatter the data to all processes
  MPI_Scatterv(data, sendcounts, displs, MPI_INT, local_data, sendcounts[rank],
               MPI_INT, 0, MPI_COMM_WORLD);

  // Each process sorts its local part using mergesort
  mergesort(local_data, sendcounts[rank]);

  // Gather the sorted subarrays at root
  MPI_Gatherv(local_data, sendcounts[rank], MPI_INT, data, sendcounts, displs,
              MPI_INT, 0, MPI_COMM_WORLD);

  // Root merges the sorted subarrays using k-way merge
  if (rank == 0) {
    int *indices = calloc(size, sizeof(int));
    int *final_sorted = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) {
      int min_val = __INT_MAX__, min_proc = -1;
      for (int p = 0; p < size; p++) {
        if (indices[p] < sendcounts[p]) {
          int idx = displs[p] + indices[p];
          if (data[idx] < min_val) {
            min_val = data[idx];
            min_proc = p;
          }
        }
      }
      final_sorted[i] = min_val;
      indices[min_proc]++;
    }

    if (N % 2 == 0) {
      printf("Median: %f\n",
             (final_sorted[N / 2 - 1] + final_sorted[N / 2]) / 2.0);
    } else {
      printf("Median: %d\n", final_sorted[N / 2]);
    }

    double end_time = MPI_Wtime();
    printf("Finding median took %f seconds.\n", end_time - start_time);

    free(final_sorted);
    free(indices);
    free(data);
  }

  MPI_Finalize();
  free(displs);
  free(sendcounts);
  free(local_data);
  return 0;
}