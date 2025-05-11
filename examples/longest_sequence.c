#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILES 128

int occurs_in_file(FILE *f, int file_len, const char *substr, int len) {
    char *buf = malloc(len);
    int found = 0;
    for (int i = 0; i <= file_len - len; i++) {
        fseek(f, i, SEEK_SET);
        fread(buf, 1, len, f);
        if (memcmp(buf, substr, len) == 0) {
            found = 1;
            break;
        }
    }
    free(buf);
    return found;
}

int main(int argc, char **argv) {
    int rank, size, k;
    char **filenames = NULL;
    int *file_lens = NULL;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 2) {
        if (rank == 0)
            printf("Użycie: %s <liczba_plikow> <plik1> <plik2> ...\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    k = atoi(argv[1]);
    if (k < 2 || k > MAX_FILES) {
        if (rank == 0)
            printf("Nieprawidłowa liczba plików (min 2, max %d)\n", MAX_FILES);
        MPI_Finalize();
        return 1;
    }

    // Rozsyłanie nazw plików
    filenames = malloc(k * sizeof(char*));
    for (int i = 0; i < k; i++) {
        char namebuf[512] = "";
        if (rank == 0) strncpy(namebuf, argv[2 + i], 511);
        MPI_Bcast(namebuf, 512, MPI_CHAR, 0, MPI_COMM_WORLD);
        filenames[i] = strdup(namebuf);
    }

    // Odczyt długości plików lokalnie
    file_lens = malloc(k * sizeof(int));
    FILE **fps = malloc(k * sizeof(FILE *));
    for (int i = 0; i < k; i++) {
        FILE *f = fopen(filenames[i], "rb");
        if (!f) {
            fprintf(stderr, "Proces %d: Nie mogę otworzyć pliku %s\n", rank, filenames[i]);
            MPI_Abort(MPI_COMM_WORLD, 2);
        }
        fseek(f, 0, SEEK_END);
        file_lens[i] = ftell(f);
        fps[i] = f;
    }

    // Znajdź najkrótszy plik
    int min_idx = 0, min_len = file_lens[0];
    for (int i = 1; i < k; i++) {
        if (file_lens[i] < min_len) {
            min_len = file_lens[i];
            min_idx = i;
        }
    }

    // Wczytaj zawartość najkrótszego pliku do bufora
    char *min_file_content = malloc(min_len + 1);
    FILE *fmin = fopen(filenames[min_idx], "rb");
    if (!fmin) {
        fprintf(stderr, "Proces %d: Nie mogę otworzyć pliku %s\n", rank, filenames[min_idx]);
        MPI_Abort(MPI_COMM_WORLD, 3);
    }
    fread(min_file_content, 1, min_len, fmin);
    min_file_content[min_len] = '\0';
    fclose(fmin);

    int found = 0;
    for (int len = min_len; len >= 1 && !found; len--) {
        int substr_count = min_len - len + 1;
        int per_proc = (substr_count + size - 1) / size;
        int start = rank * per_proc;
        int end = (start + per_proc < substr_count) ? (start + per_proc) : substr_count;

        char local_result[min_len + 1];
        local_result[0] = '\0';

        for (int i = start; i < end; i++) {
            char candidate[min_len + 1];
            strncpy(candidate, min_file_content + i, len);
            candidate[len] = '\0';

            int ok = 1;
            for (int f = 0; f < k; f++) {
                if (f == min_idx) continue;
                rewind(fps[f]);
                if (!occurs_in_file(fps[f], file_lens[f], candidate, len)) {
                    ok = 0;
                    break;
                }
            }
            if (ok) {
                strcpy(local_result, candidate);
                break;
            }
        }

        // Zbierz wyniki od wszystkich procesów
        char *gathered = NULL;
        if (rank == 0) gathered = malloc(size * (min_len + 1));
        MPI_Gather(local_result, min_len + 1, MPI_CHAR,
                   gathered, min_len + 1, MPI_CHAR, 0, MPI_COMM_WORLD);

        if (rank == 0) {
            for (int i = 0; i < size; i++) {
                if (gathered[i * (min_len + 1)] != '\0') {
                    printf("Najdłuższy wspólny podciąg: %s\n", &gathered[i * (min_len + 1)]);
                    found = 1;
                    break;
                }
            }
            free(gathered);
        }
        // Rozgłoś flagę found do wszystkich
        MPI_Bcast(&found, 1, MPI_INT, 0, MPI_COMM_WORLD);
    }

    if (rank == 0 && !found)
        printf("Brak wspólnego podciągu!\n");

    // Sprzątanie
    for (int i = 0; i < k; i++) {
        fclose(fps[i]);
        free(filenames[i]);
    }
    free(fps);
    free(filenames);
    free(file_lens);
    free(min_file_content);

    MPI_Finalize();
    return 0;
}