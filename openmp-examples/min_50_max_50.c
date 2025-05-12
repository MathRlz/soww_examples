#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>

#define K 50

// Funkcja pomocnicza do wstawiania liczby do posortowanej tablicy (rosnąco)
void insert_min(int *arr, int val) {
    if (val >= arr[K-1]) return;
    int i = K-1;
    while (i > 0 && arr[i-1] > val) {
        arr[i] = arr[i-1];
        i--;
    }
    arr[i] = val;
}

// Funkcja pomocnicza do wstawiania liczby do posortowanej tablicy (malejąco)
void insert_max(int *arr, int val) {
    if (val <= arr[K-1]) return;
    int i = K-1;
    while (i > 0 && arr[i-1] < val) {
        arr[i] = arr[i-1];
        i--;
    }
    arr[i] = val;
}

int main(int argc, char *argv[]) {
    int num_threads = 4;
    int N = 20000;

    // Użycie: ./min_50_max_50 [num_threads] [N]
    if (argc > 1) num_threads = atoi(argv[1]);
    if (argc > 2) N = atoi(argv[2]);

    int *data = malloc(N * sizeof(int));
    srand((unsigned int)time(NULL));
    for (int i = 0; i < N; i++) {
        data[i] = rand();
    }

    int min_global[K];
    int max_global[K];
    for (int i = 0; i < K; i++) {
        min_global[i] = __INT_MAX__;
        max_global[i] = -__INT_MAX__;
    }

    double start_time = omp_get_wtime();

    #pragma omp parallel num_threads(num_threads)
    {
        int min_local[K];
        int max_local[K];
        for (int i = 0; i < K; i++) {
            min_local[i] = __INT_MAX__;
            max_local[i] = -__INT_MAX__;
        }

        #pragma omp for
        for (int i = 0; i < N; i++) {
            insert_min(min_local, data[i]);
            insert_max(max_local, data[i]);
        }

        // Redukcja: scalanie lokalnych wyników do globalnych
        #pragma omp critical
        {
            for (int i = 0; i < K; i++) {
                insert_min(min_global, min_local[i]);
                insert_max(max_global, max_local[i]);
            }
        }
    }

    double end_time = omp_get_wtime();

    printf("50 najmniejszych liczb:\n");
    for (int i = 0; i < K; i++) {
        printf("%d ", min_global[i]);
    }
    printf("\n50 największych liczb:\n");
    for (int i = 0; i < K; i++) {
        printf("%d ", max_global[i]);
    }
    printf("\n");

    printf("Czas wykonania: %.6f sekund\n", end_time - start_time);

    free(data);
    return 0;
}