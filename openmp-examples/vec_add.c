#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Usage: %s <num_threads> <n>\n", argv[0]);
    return 1;
  }

  int threads = atoi(argv[1]);
  int n = atoi(argv[2]);

  int *a = malloc(n * sizeof(int));
  int *b = malloc(n * sizeof(int));
  int *c = malloc(n * sizeof(int));

  // Initialize vectors
  for (int i = 0; i < n; i++) {
    a[i] = i;
    b[i] = n - i;
  }

  omp_set_num_threads(threads);

  double start = omp_get_wtime();

#pragma omp parallel for
  for (int i = 0; i < n; i++) {
    c[i] = a[i] + b[i];
  }

  double end = omp_get_wtime();

  for (int i = 0; i < n && i < 10; i++) {
    printf("c[%d] = %d\n", i, c[i]);
  }
  printf("Execution time: %f seconds\n", end - start);

  free(a);
  free(b);
  free(c);
  return 0;
}