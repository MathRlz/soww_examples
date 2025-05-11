#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Porównanie malejące do qsort
int compare_desc(const void *a, const void *b) {
  int x = *(const int *)a;
  int y = *(const int *)b;
  return y - x;
}

int main(int argc, char **argv) {
  int rank, size, N;
  int *numbers = NULL;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  // Przygotowanie danych na masterze
  if (rank == 0) {
    N = 100;
    numbers = malloc(N * sizeof(int));
    srand(time(NULL));
    printf("Tablica: ");
    for (int i = 0; i < N; i++) {
      numbers[i] = rand() % 1000 + 1;
      printf("%d ", numbers[i]);
    }
    printf("\n");
  }

  // Rozesłanie rozmiaru i danych
  MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
  if (rank != 0)
    numbers = malloc(N * sizeof(int));
  MPI_Bcast(numbers, N, MPI_INT, 0, MPI_COMM_WORLD);

  // Podział pracy
  int base = N / size, rem = N % size;
  int local_n = base + (rank < rem ? 1 : 0);
  int offset = rank * base + (rank < rem ? rank : rem);

  int *local = malloc(local_n * sizeof(int));
  for (int i = 0; i < local_n; i++)
    local[i] = numbers[offset + i];

  // Sortowanie lokalne malejąco
  qsort(local, local_n, sizeof(int), compare_desc);

  // Wybranie do 17 największych z lokalnego fragmentu
  int k = local_n < 17 ? local_n : 17;
  int *local_top = malloc(17 * sizeof(int));
  for (int i = 0; i < 17; i++)
    local_top[i] = (i < k) ? local[i] : -2147483648; // INT_MIN

  // Zbieranie 17 największych z każdego procesu do mastera
  int *gathered = NULL;
  if (rank == 0)
    gathered = malloc(17 * size * sizeof(int));
  MPI_Gather(local_top, 17, MPI_INT, gathered, 17, MPI_INT, 0, MPI_COMM_WORLD);

  if (rank == 0) {
    // Usuń INT_MIN (padding) i policz ile jest realnych liczb
    int *filtered = malloc(17 * size * sizeof(int));
    int filtered_count = 0;
    for (int i = 0; i < 17 * size; i++) {
      if (gathered[i] != -2147483648) // INT_MIN
        filtered[filtered_count++] = gathered[i];
    }
    if (filtered_count < 17) {
      printf("Za mało liczb do znalezienia 17-tej największej!\n");
    } else {
      qsort(filtered, filtered_count, sizeof(int), compare_desc);
      printf("17-ta największa liczba: %d\n", filtered[16]);
    }
    free(filtered);
    free(gathered);
  }

  free(local_top);
  free(local);
  free(numbers);
  MPI_Finalize();
  return 0;
}