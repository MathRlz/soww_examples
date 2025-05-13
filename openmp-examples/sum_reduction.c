#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Użycie: %s <liczba_wątków> <n>\n", argv[0]);
    return 1;
  }

  int threads = atoi(argv[1]);
  int n = atoi(argv[2]);
  int *array = malloc(n * sizeof(int));
  int sum = 0;

  // Inicjalizacja tablicy
  for (int i = 0; i < n; i++) {
    array[i] = 1;
  }

  omp_set_num_threads(threads);

  double start = omp_get_wtime();

#pragma omp parallel for reduction(+ : sum)
  for (int i = 0; i < n; i++) {
    sum += array[i];
  }

  double end = omp_get_wtime();

  printf("Suma elementów tablicy: %d\n", sum);
  printf("Czas wykonania: %f sekund\n", end - start);

  free(array);
  return 0;
}