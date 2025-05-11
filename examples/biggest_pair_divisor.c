#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Największy wspólny dzielnik
int gcd(int a, int b) {
  while (b) {
    int t = b;
    b = a % b;
    a = t;
  }
  return a;
}

int main(int argc, char **argv) {
  int rank, size, N;
  int *tab = NULL;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  // Generowanie/przygotowanie danych na masterze
  if (rank == 0) {
    N = 100;
    tab = malloc(N * sizeof(int));
    srand(time(NULL));
    printf("Tablica: ");
    for (int i = 0; i < N; i++) {
      tab[i] = rand() % 10000 + 1;
      printf("%d ", tab[i]);
    }
    printf("\n");
  }

  // Rozesłanie rozmiaru i danych
  MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
  if (rank != 0)
    tab = malloc(N * sizeof(int));
  MPI_Bcast(tab, N, MPI_INT, 0, MPI_COMM_WORLD);

  // Podział pracy: każdy proces sprawdza część par (i, j)
  long long total_pairs = (long long)N * (N - 1) / 2;
  long long per_proc = (total_pairs + size - 1) / size;
  long long start = rank * per_proc;
  long long end =
      (start + per_proc < total_pairs) ? (start + per_proc) : total_pairs;

  // Zamiana numeru pary na indeksy i, j
  int max_gcd = 0, best_i = -1, best_j = -1;
  int local_data[3] = {max_gcd, best_i, best_j};
  int global_data[3];
  for (long long idx = start; idx < end; idx++) {
    // Zamiana idx na (i, j)
    int i = 0, j = 0, cnt = 0;
    for (i = 0; i < N - 1; i++) {
      if (idx < cnt + N - i - 1) {
        j = i + 1 + (idx - cnt);
        break;
      }
      cnt += N - i - 1;
    }
    int d = gcd(tab[i], tab[j]);
    if (d > max_gcd) {
      max_gcd = d;
      best_i = i;
      best_j = j;
    }
  }

  local_data[0] = max_gcd;
  local_data[1] = best_i;
  local_data[2] = best_j;

  // Redukcja: znajdź największy dzielnik
  int global_gcd = 0;
  MPI_Allreduce(&max_gcd, &global_gcd, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

  int result[2] = {-1, -1};
  if (max_gcd == global_gcd) {
    result[0] = best_i;
    result[1] = best_j;
  }
  int global_result[2];
  MPI_Reduce(result, global_result, 2, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);

  if (rank == 0) {
    if (global_gcd > 0) {
      printf("Największy dzielnik to %d dla pary (%d, %d): %d i %d\n",
             global_gcd, global_result[0], global_result[1],
             tab[global_result[0]], tab[global_result[1]]);
    } else {
      printf("Nie znaleziono wspólnego dzielnika.\n");
    }
  }

  free(tab);
  MPI_Finalize();
  return 0;
}