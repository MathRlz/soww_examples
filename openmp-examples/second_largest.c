#include <limits.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[]) {
  int num_threads = 10;
  long long N = 100000000;

  // ./second_largest [num_threads] [N]
  if (argc > 1) num_threads = atoi(argv[1]);
  if (argc > 2) N = atoll(argv[2]);

  omp_set_num_threads(num_threads);
  int *data = (int *)malloc(N * sizeof(int));
  int top = INT_MIN, top2 = INT_MIN;

  srand((unsigned int)time(NULL));

  // Generate random data
  for (long long i = 0; i < N; i++) {
    data[i] = rand();
  }

  double start = omp_get_wtime();

  int thread_top, thread_top2;
#pragma omp parallel private(thread_top, thread_top2)
  {
    thread_top = INT_MIN;
    thread_top2 = INT_MIN;

#pragma omp for nowait
    for (long long i = 0; i < N; i++) {
      if (data[i] > thread_top) {
        thread_top2 = thread_top;
        thread_top = data[i];
      } else if (data[i] > thread_top2 && data[i] < thread_top) {
        thread_top2 = data[i];
      }
    }

    // Combine results from all threads
#pragma omp critical
    {
      if (thread_top > top) {
        if (top > top2)
          top2 = top;
        top = thread_top;
      } else if (thread_top > top2 && thread_top < top) {
        top2 = thread_top;
      }

      if (thread_top2 > top2 && thread_top2 < top) {
        top2 = thread_top2;
      }
    }
  }

  double end = omp_get_wtime();

  printf("Largest: %d\n", top);
  if (top2 != INT_MIN) {
    printf("Second largest: %d\n", top2);
  } else {
    printf("No second largest value found.\n");
  }
  printf("Execution time: %.6f seconds\n", end - start);

  free(data);
  return 0;
}