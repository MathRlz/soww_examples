#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// Porównanie do qsort
int compare(const void *a, const void *b) {
  int x = *(const int *)a;
  int y = *(const int *)b;
  return x - y;
}

// Usuwa duplikaty z tablicy, zwraca nowy rozmiar
int remove_duplicates(int *arr, int n) {
  if (n == 0)
    return 0;
  int j = 0;
  for (int i = 1; i < n; i++) {
    if (arr[i] != arr[j]) {
      j++;
      arr[j] = arr[i];
    }
  }
  return j + 1;
}

// Mergesort implementation
void merge(int *arr, int *temp, int left, int mid, int right) {
  int i = left, j = mid + 1, k = left;
  while (i <= mid && j <= right) {
    if (arr[i] <= arr[j]) {
      temp[k++] = arr[i++];
    } else {
      temp[k++] = arr[j++];
    }
  }
  while (i <= mid)
    temp[k++] = arr[i++];
  while (j <= right)
    temp[k++] = arr[j++];
  for (i = left; i <= right; i++)
    arr[i] = temp[i];
}

void mergesort_recursive(int *arr, int *temp, int left, int right) {
  if (left < right) {
    int mid = left + (right - left) / 2;
    mergesort_recursive(arr, temp, left, mid);
    mergesort_recursive(arr, temp, mid + 1, right);
    merge(arr, temp, left, mid, right);
  }
}

void mergesort(int *arr, int n) {
  int *temp = malloc(n * sizeof(int));
  mergesort_recursive(arr, temp, 0, n - 1);
  free(temp);
}

int main(int argc, char **argv) {
  int rank, size, N;
  int *liczby = NULL;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  // Wczytaj dane na masterze
  if (rank == 0) {
    /*
    printf("Podaj rozmiar tablicy N: ");
    fflush(stdout);
    scanf("%d", &N);
    liczby = malloc(N * sizeof(int));
    printf("Podaj %d liczb:\n", N);
    for (int i = 0; i < N; i++) scanf("%d", &liczby[i]);
    */
    N = 1000;
    liczby = malloc(N * sizeof(int));
    srand(42);
    printf("Wylosowana tablica:\n");
    for (int i = 0; i < N; i++) {
      liczby[i] = rand() % 1000 + 1;
      printf("%d ", liczby[i]);
    }
    printf("\n");
  }

  // Rozesłanie rozmiaru i danych
  MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
  if (rank != 0)
    liczby = malloc(N * sizeof(int));
  MPI_Bcast(liczby, N, MPI_INT, 0, MPI_COMM_WORLD);

  // Calculate sendcounts and displacements
  int *sendcounts = malloc(size * sizeof(int));
  int *displs = malloc(size * sizeof(int));
  int base = N / size, rem = N % size, offset = 0;
  for (int i = 0; i < size; i++) {
    sendcounts[i] = base + (i < rem ? 1 : 0);
    displs[i] = offset;
    offset += sendcounts[i];
  }

  // Scatter
  int *local_data = malloc(sendcounts[rank] * sizeof(int));
  MPI_Scatterv(liczby, sendcounts, displs, MPI_INT, local_data,
               sendcounts[rank], MPI_INT, 0, MPI_COMM_WORLD);

  // Local sort
  mergesort(local_data, sendcounts[rank]);

  // Gather
  MPI_Gatherv(local_data, sendcounts[rank], MPI_INT, liczby, sendcounts, displs,
              MPI_INT, 0, MPI_COMM_WORLD);

  // K-way merge on root
  if (rank == 0) {
    int *indices = calloc(size, sizeof(int));
    int *sorted = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) {
      int min_val = __INT_MAX__, min_proc = -1;
      for (int p = 0; p < size; p++) {
        if (indices[p] < sendcounts[p]) {
          int idx = displs[p] + indices[p];
          if (liczby[idx] < min_val) {
            min_val = liczby[idx];
            min_proc = p;
          }
        }
      }
      sorted[i] = min_val;
      indices[min_proc]++;
    }
    // Copy back to liczby
    for (int i = 0; i < N; i++)
      liczby[i] = sorted[i];
    free(sorted);
    free(indices);
  }

  // Broadcast sorted array to all processes
  MPI_Bcast(liczby, N, MPI_INT, 0, MPI_COMM_WORLD);

  free(local_data);
  free(sendcounts);
  free(displs);

  double start_time = MPI_Wtime();

  // Wyznacz zakres dla procesu
  int seg_start = rank * N / size;
  int seg_end = (rank + 1) * N / size;
  if (seg_end > N)
    seg_end = N;

  // Bufor na wyniki lokalne (maksymalnie seg_end - seg_start)
  int *local_result = malloc((seg_end - seg_start) * sizeof(int));
  int local_count = 0;

  // Szukaj x = y - z dla każdej liczby x w swoim segmencie
  for (int idx = seg_start; idx < seg_end; idx++) {
    int x = liczby[idx];
    int left = 0, right = N - 1;
    bool found = false;
    while (left < right) {
      int diff = liczby[right] - liczby[left];
      if (diff == x) {
        found = true;
        break;
      } else if (diff > x) {
        right--;
      } else {
        left++;
      }
    }
    if (found) {
      local_result[local_count++] = x;
    }
  }

  // Usuń duplikaty lokalnie
  qsort(local_result, local_count, sizeof(int), compare);
  local_count = remove_duplicates(local_result, local_count);

  // Zbierz rozmiary wyników
  int *recvcounts = NULL, *displs_result = NULL;
  if (rank == 0) {
    recvcounts = malloc(size * sizeof(int));
    displs_result = malloc(size * sizeof(int));
  }
  MPI_Gather(&local_count, 1, MPI_INT, recvcounts, 1, MPI_INT, 0,
             MPI_COMM_WORLD);

  // Oblicz displacements na masterze
  int total = 0;
  if (rank == 0) {
    displs_result[0] = 0;
    for (int i = 0; i < size; i++) {
      if (i > 0)
        displs_result[i] = displs_result[i - 1] + recvcounts[i - 1];
      total += recvcounts[i];
    }
  }

  // Zbierz wyniki do mastera
  int *all_results = NULL;
  if (rank == 0)
    all_results = malloc(total * sizeof(int));
  MPI_Gatherv(local_result, local_count, MPI_INT, all_results, recvcounts,
              displs_result, MPI_INT, 0, MPI_COMM_WORLD);

  // Master scala i usuwa duplikaty
  if (rank == 0) {
    qsort(all_results, total, sizeof(int), compare);
    int unique = remove_duplicates(all_results, total);
    printf("Liczby x takie, że x = y - z: ");
    for (int i = 0; i < unique; i++)
      printf("%d ", all_results[i]);
    printf("\n");

    double end_time = MPI_Wtime();
    printf("Czas wykonania: %f sekund\n", end_time - start_time);

    free(all_results);
    free(recvcounts);
    free(displs_result);
  }

  free(local_result);
  free(liczby);
  MPI_Finalize();
  return 0;
}