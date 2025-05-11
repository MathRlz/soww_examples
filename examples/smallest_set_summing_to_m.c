#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Obliczanie liczby kombinacji (n po k)
long long n_choose_k(int n, int k) {
  if (k > n)
    return 0;
  if (k == 0 || k == n)
    return 1;
  long long res = 1;
  for (int i = 1; i <= k; i++)
    res = res * (n - i + 1) / i;
  return res;
}

// Generowanie k-elementowej kombinacji o zadanym indeksie (leksykograficznie)
void get_combination(int n, int k, long long idx, int *comb) {
  int x = 0;
  for (int i = 0; i < k; i++) {
    for (int j = x; j < n; j++) {
      long long c = n_choose_k(n - j - 1, k - i - 1);
      if (idx < c) {
        comb[i] = j;
        x = j + 1;
        break;
      } else {
        idx -= c;
      }
    }
  }
}

int main(int argc, char **argv) {
  int rank, size, N, M;
  int *array = NULL;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  // Przygotowanie danych (losowe dane na procesie 0)
  if (rank == 0) {
    N = 37;
    array = malloc(N * sizeof(int));
    srand(time(NULL));
    printf("Tablica: ");
    for (int i = 0; i < N; i++) {
      array[i] = rand() % 20 + 1;
      printf("%d ", array[i]);
    }
    printf("\n");
    M = 40;
    printf("Szukana suma M: %d\n", M);
  }

  // Rozesłanie danych do wszystkich procesów
  MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
  if (rank != 0)
    array = malloc(N * sizeof(int));
  MPI_Bcast(array, N, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&M, 1, MPI_INT, 0, MPI_COMM_WORLD);

  int result_size = -1;
  long long result_idx = -1;
  int stop_search = 0;

  double start_time = MPI_Wtime();

  // Szukamy najmniejszego rozmiaru podzbioru
  for (int k = 1; k <= N && !stop_search; k++) {
    long long total = n_choose_k(N, k);
    long long per_proc = (total + size - 1) / size;
    long long start = rank * per_proc;
    long long end = (start + per_proc < total) ? (start + per_proc) : total;

    long long found_idx = -1;
    int *comb = malloc(k * sizeof(int));
    for (long long idx = start; idx < end && !stop_search; idx++) {
      get_combination(N, k, idx, comb);
      int sum = 0;
      for (int i = 0; i < k; i++)
        sum += array[comb[i]];
      if (sum == M) {
        found_idx = idx;
        break;
      }
    }
    free(comb);

    // Redukcja: znajdź dowolny indeks znalezionego podzbioru
    long long global_idx = -1;
    MPI_Allreduce(&found_idx, &global_idx, 1, MPI_LONG_LONG, MPI_MAX,
                  MPI_COMM_WORLD);

    // Broadcast stop_search flag to all processes
    stop_search = (global_idx != -1) ? 1 : 0;
    MPI_Bcast(&stop_search, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (global_idx != -1) {
      if (rank == 0) {
        int *solution = malloc(k * sizeof(int));
        get_combination(N, k, global_idx, solution);
        printf("Najmniejszy podzbiór o sumie %d ma rozmiar: %d\n", M, k);
        printf("Podzbiór: ");
        for (int i = 0; i < k; i++)
          printf("%d ", array[solution[i]]);
        printf("\n");
        free(solution);
        result_size = k;
      }
      break;
    }
  }

  double end_time = MPI_Wtime();

  if (rank == 0 && result_size == -1) {
    printf("Nie znaleziono podzbioru o sumie %d\n", M);
  }
  if (rank == 0) {
    printf("Czas wykonania: %f sekund\n", end_time - start_time);
  }

  free(array);
  MPI_Finalize();
  return 0;
}