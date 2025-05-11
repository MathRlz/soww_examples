#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define INT_TOTAL 50000000
#define FLOAT_TOTAL 120000000

int get_chunk_size(int total, int iterations) {
  return (total + iterations - 1) / iterations;
}

int main(int argc, char **argv) {
  int rank, size;
  int iterations = 10;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (argc > 1) {
    iterations = atoi(argv[1]);
    if (iterations < 1)
      iterations = 1;
  }

  int *int_data = NULL;
  float *float_data = NULL;
  if (rank == 0) {
    int_data = malloc(INT_TOTAL * sizeof(int));
    float_data = malloc(FLOAT_TOTAL * sizeof(float));
    for (int i = 0; i < INT_TOTAL; i++)
      int_data[i] = i;
    for (int i = 0; i < FLOAT_TOTAL; i++)
      float_data[i] = (float)i;
  } else {
    int_data = malloc(INT_TOTAL * sizeof(int));
    float_data = malloc(FLOAT_TOTAL * sizeof(float));
  }

  int int_chunk = get_chunk_size(INT_TOTAL, iterations);
  int float_chunk = get_chunk_size(FLOAT_TOTAL, iterations);

  double start_time = MPI_Wtime();

  for (int c = 0; c < iterations; c++) {
    // Wysyłaj fragment intów
    int int_offset = c * int_chunk;
    int int_count = (int_offset + int_chunk <= INT_TOTAL)
                        ? int_chunk
                        : (INT_TOTAL - int_offset);
    if (int_count > 0)
      MPI_Bcast(int_data + int_offset, int_count, MPI_INT, 0, MPI_COMM_WORLD);

    // Wysyłaj fragment floatów
    int float_offset = c * float_chunk;
    int float_count = (float_offset + float_chunk <= FLOAT_TOTAL)
                          ? float_chunk
                          : (FLOAT_TOTAL - float_offset);
    if (float_count > 0)
      MPI_Bcast(float_data + float_offset, float_count, MPI_FLOAT, 0,
                MPI_COMM_WORLD);
  }

  double end_time = MPI_Wtime();

  if (rank == 0) {
    printf("Wszystkie dane rozesłane przez Bcast w %d kawałkach.\n",
           iterations);
    printf("Czas wykonania: %f sekund\n", end_time - start_time);
  }

  free(int_data);
  free(float_data);
  MPI_Finalize();
  return 0;
}