#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define CHUNK_SIZE 10000

int main(int argc, char *argv[]) {
  int num_threads = 4;
  int num_tasks = 10000;

  // Usage: ./master_slaves [num_threads] [num_tasks]
  if (argc > 1)
    num_threads = atoi(argv[1]);
  if (argc > 2)
    num_tasks = atoi(argv[2]);

  omp_set_num_threads(num_threads);
  double *results = (double *)malloc(num_tasks * sizeof(double));

  double start_time = omp_get_wtime();

#pragma omp parallel
  {
#pragma omp master
    {
      // Master thread assigns tasks
      for (int i = 0; i < num_tasks; i++) {
// firstprivate(i) ensures that each task gets its own copy of i
#pragma omp task firstprivate(i)
        {
          // Simulate a real calculation: sum of square roots in a chunk
          double sum = 0.0;
          int start = i * CHUNK_SIZE;
          int end = start + CHUNK_SIZE;
          for (int j = start; j < end; j++) {
            // In real example it would be sqrt(data[j])
            sum += sqrt((double)j);
          }
          results[i] = sum;
        }
      }
    }
// Wait for all tasks to complete
#pragma omp taskwait
  }

  double end_time = omp_get_wtime();
  printf("Execution time: %.6f seconds\n", end_time - start_time);

  free(results);
  return 0;
}