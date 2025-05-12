#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
  int A;
  int B;
} Pair;

int main(int argc, char *argv[]) {
  int num_threads = 4;
  int N = 100;

  // Usage: ./find_all_x_and_y_st_x_plus_10_is_y [num_threads] [N]
  if (argc > 1)
    num_threads = atoi(argv[1]);
  if (argc > 2)
    N = atoi(argv[2]);

  int *data = malloc(N * sizeof(int));
  srand((unsigned int)time(NULL));
  for (int i = 0; i < N; i++) {
    data[i] = rand() % N;
  }

  double start_time = omp_get_wtime();

  // Przygotuj bufor na pary dla każdego wątku
  Pair **thread_pairs = malloc(num_threads * sizeof(Pair *));
  int *thread_counts = calloc(num_threads, sizeof(int));
  int *thread_capacities = malloc(num_threads * sizeof(int));
  for (int t = 0; t < num_threads; t++) {
    thread_capacities[t] = 1024; // początkowa pojemność
    thread_pairs[t] = malloc(thread_capacities[t] * sizeof(Pair));
  }

#pragma omp parallel num_threads(num_threads)
  {
    int tid = omp_get_thread_num();
    int local_count = 0;
    int local_capacity = thread_capacities[tid];

    for (int i = tid; i < N; i += num_threads) {
      int A = data[i];
      for (int j = 0; j < N; j++) {
        int B = data[j];
        if (A + 10 == B) {
          // Powiększ bufor jeśli trzeba
          if (local_count >= local_capacity) {
            local_capacity *= 2;
            thread_pairs[tid] =
                realloc(thread_pairs[tid], local_capacity * sizeof(Pair));
          }
          thread_pairs[tid][local_count].A = A;
          thread_pairs[tid][local_count].B = B;
          local_count++;
        }
      }
    }
    thread_counts[tid] = local_count;
    thread_capacities[tid] = local_capacity;
  }

  // Policz sumaryczną liczbę par i połącz wyniki
  int total_pairs = 0;
  for (int t = 0; t < num_threads; t++) {
    total_pairs += thread_counts[t];
  }
  Pair *pairs = malloc(total_pairs * sizeof(Pair));
  int idx = 0;
  for (int t = 0; t < num_threads; t++) {
    for (int i = 0; i < thread_counts[t]; i++) {
      pairs[idx++] = thread_pairs[t][i];
    }
    free(thread_pairs[t]);
  }
  free(thread_pairs);
  free(thread_counts);
  free(thread_capacities);

  double end_time = omp_get_wtime();

  printf("Znalezione pary (A, B) takie, że A + 10 = B: %d\n", total_pairs);
  for (int i = 0; i < total_pairs && i < 100; i++) { // wypisz max 100 par
    printf("A = %d, B = %d\n", pairs[i].A, pairs[i].B);
  }
  if (total_pairs > 100)
    printf("... (wypisano tylko 100 z %d)\n", total_pairs);
  printf("Czas wykonania: %.6f sekund\n", end_time - start_time);

  free(data);
  free(pairs);
  return 0;
}