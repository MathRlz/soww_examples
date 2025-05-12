#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

int is_prime(int x) {
  if (x < 2)
    return 0;
  if (x == 2)
    return 1;
  if (x % 2 == 0)
    return 0;
  int sqrt_x = (int)sqrt((double)x);
  for (int i = 3; i <= sqrt_x; i += 2) {
    if (x % i == 0)
      return 0;
  }
  return 1;
}

int main(int argc, char *argv[]) {
  int range_start = 1;
  int range_end = 10000;
  int num_threads = 4;

  // Usage: ./master_slaves [num_threads] [range_start] [range_end]
  if (argc > 1)
    num_threads = atoi(argv[1]);
  if (argc > 2)
    range_start = atoi(argv[2]);
  if (argc > 3)
    range_end = atoi(argv[3]);

  int N = range_end - range_start + 1;
  int *data = (int *)malloc(N * sizeof(int));
  int chunk_size = N / num_threads;
  int total_primes = 0;

  double start_time = omp_get_wtime();

#pragma omp parallel num_threads(num_threads) shared(data)                     \
    reduction(+ : total_primes)
  {
// Master fills the data array
#pragma omp master
    {
      for (int i = 0; i < N; i++) {
        data[i] = range_start + i;
      }
    }

#pragma omp barrier // Ensure data is ready

    int tid = omp_get_thread_num();
    int start = tid * chunk_size;
    int end = (tid == num_threads - 1) ? N : start + chunk_size;

    int local_count = 0;
    for (int i = start; i < end; i++) {
      if (is_prime(data[i])) {
        local_count++;
      }
    }

    total_primes += local_count;
  }

  double end_time = omp_get_wtime();

  printf("Liczb pierwszych w zakresie [%d, %d]: %d\n", range_start, range_end,
         total_primes);
  printf("Czas wykonania: %.6f sekund\n", end_time - start_time);

  free(data);
  return 0;
}