#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DATA_TAG 1
#define READY_TAG 2
#define TERMINATE_TAG 3
#define HISTOGRAM_TAG 4

typedef struct {
  int range_min;
  int range_max;
  int data_size;
  int chunk_size;
} Arguments;

void master(int range_min, int range_max, int data_size, int chunk_size,
            int size) {
  double start_time = MPI_Wtime();
  int bins = range_max - range_min + 1; // Number of bins in histogram

  // Create and initialize random data
  srand(time(NULL));
  int *data = (int *)malloc(data_size * sizeof(int));
  for (int i = 0; i < data_size; i++) {
    data[i] = range_min + rand() % (range_max - range_min + 1);
  }

  // Initialize global histogram
  int *global_histogram = (int *)calloc(bins, sizeof(int));

  // Counter for processed chunks
  int chunks_processed = 0;
  int total_chunks = ceil((double)data_size / chunk_size);

  printf("Master: Processing %d data items with %d chunks of size %d\n",
         data_size, total_chunks, chunk_size);

  // Determine how many slaves we'll use
  int slaves_used = (size - 1 < total_chunks) ? size - 1 : total_chunks;

  // Track active slaves and their requests
  int active_slaves = 0;
  MPI_Request *send_length_requests =
      (MPI_Request *)malloc(slaves_used * sizeof(MPI_Request));
  MPI_Request *send_data_requests =
      (MPI_Request *)malloc(slaves_used * sizeof(MPI_Request));
  MPI_Request *recv_ready_requests =
      (MPI_Request *)malloc(slaves_used * sizeof(MPI_Request));
  int *ready_flags = (int *)calloc(slaves_used, sizeof(int));
  int *active_flags = (int *)calloc(slaves_used, sizeof(int));

  // Setup buffers for non-blocking receives from slaves
  int *ready_buffers = (int *)malloc(slaves_used * sizeof(int));

  // First assignment of chunks - one for each slave
  for (int i = 1; i <= slaves_used; i++) {
    int slave_idx = i - 1;
    int start_idx = slave_idx * chunk_size;
    int length = (start_idx + chunk_size <= data_size)
                     ? chunk_size
                     : (data_size - start_idx);

    // Send chunk size and data using non-blocking sends
    MPI_Isend(&length, 1, MPI_INT, i, DATA_TAG, MPI_COMM_WORLD,
              &send_length_requests[slave_idx]);
    MPI_Isend(&data[start_idx], length, MPI_INT, i, DATA_TAG, MPI_COMM_WORLD,
              &send_data_requests[slave_idx]);

    // Post non-blocking receive for ready message
    MPI_Irecv(&ready_buffers[slave_idx], 1, MPI_INT, i, READY_TAG,
              MPI_COMM_WORLD, &recv_ready_requests[slave_idx]);

    active_flags[slave_idx] = 1;
    active_slaves++;
    chunks_processed++;
  }

  // Process all chunks
  while (chunks_processed < total_chunks || active_slaves > 0) {
    int index;
    MPI_Status status;
    // Wait for any slave to become ready
    MPI_Waitany(slaves_used, recv_ready_requests, &index, &status);

    if (index == MPI_UNDEFINED) {
      break; // No more active requests
    }

    ready_flags[index] = 1;

    if (chunks_processed < total_chunks) {
      int slave_id = index + 1;
      int start_idx = chunks_processed * chunk_size;
      int length = (start_idx + chunk_size <= data_size)
                       ? chunk_size
                       : (data_size - start_idx);

      // Wait for previous sends to complete
      MPI_Wait(&send_length_requests[index], MPI_STATUS_IGNORE);
      MPI_Wait(&send_data_requests[index], MPI_STATUS_IGNORE);

      // Send next chunk
      MPI_Isend(&length, 1, MPI_INT, slave_id, DATA_TAG, MPI_COMM_WORLD,
                &send_length_requests[index]);
      MPI_Isend(&data[start_idx], length, MPI_INT, slave_id, DATA_TAG,
                MPI_COMM_WORLD, &send_data_requests[index]);

      // Reset ready flag and repost receive
      ready_flags[index] = 0;
      MPI_Irecv(&ready_buffers[index], 1, MPI_INT, slave_id, READY_TAG,
                MPI_COMM_WORLD, &recv_ready_requests[index]);

      chunks_processed++;
    } else {
      // No more chunks, send termination signal
      int slave_id = index + 1;
      int zero_length = 0;

      // Wait for previous sends to complete
      MPI_Wait(&send_length_requests[index], MPI_STATUS_IGNORE);
      MPI_Wait(&send_data_requests[index], MPI_STATUS_IGNORE);

      // Send termination signal
      MPI_Send(&zero_length, 1, MPI_INT, slave_id, TERMINATE_TAG,
               MPI_COMM_WORLD);

      // Mark slave as inactive
      active_flags[index] = 0;
      active_slaves--;
    }
  }

  // Collect histograms from all slaves
  int *local_histogram = (int *)malloc(bins * sizeof(int));
  for (int i = 0; i < slaves_used; i++) {
    MPI_Status status;

    // Receive local histogram - this is blocking since we need the results
    MPI_Recv(local_histogram, bins, MPI_INT, i + 1, HISTOGRAM_TAG,
             MPI_COMM_WORLD, &status);

    // Update global histogram
    for (int j = 0; j < bins; j++) {
      global_histogram[j] += local_histogram[j];
    }
  }
  free(local_histogram);

  double end_time = MPI_Wtime();

  // Print histogram results
  printf("\nHistogram calculation took %.6f seconds\n", end_time - start_time);
  printf("Histogram results (showing first 10 non-empty bins):\n");

  int shown = 0;
  for (int i = 0; i < bins && shown < 10; i++) {
    if (global_histogram[i] > 0) {
      printf("%d: %d occurrences\n", range_min + i, global_histogram[i]);
      shown++;
    }
  }

  // Verify total count
  int total = 0;
  for (int i = 0; i < bins; i++) {
    total += global_histogram[i];
  }

  if (total != data_size) {
    fprintf(stderr, "Error: Total count mismatch! Expected %d, got %d\n",
            data_size, total);
  }

  // Free resources
  free(data);
  free(global_histogram);
  free(send_length_requests);
  free(send_data_requests);
  free(recv_ready_requests);
  free(ready_flags);
  free(active_flags);
  free(ready_buffers);
}

void slave(int range_min, int range_max, int data_size, int chunk_size) {
  int bins = range_max - range_min + 1;
  int *local_histogram = (int *)calloc(bins, sizeof(int));

  // Create two buffers for double buffering
  int buffer_size = chunk_size;
  int *buffer_a = (int *)malloc(buffer_size * sizeof(int));
  int *buffer_b = (int *)malloc(buffer_size * sizeof(int));

  int length_a = 0, length_b = 0;
  MPI_Status status;
  MPI_Request recv_request = MPI_REQUEST_NULL;
  MPI_Request send_request = MPI_REQUEST_NULL;
  int active = 1;

  // Set up pointers for double buffering
  int *current_buf = buffer_a;
  int *next_buf = buffer_b;
  int *current_len = &length_a;
  int *next_len = &length_b;

  // Receive initial chunk size
  MPI_Recv(current_len, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

  // Check if termination signal on first message
  if (status.MPI_TAG == TERMINATE_TAG || *current_len == 0) {
    MPI_Send(local_histogram, bins, MPI_INT, 0, HISTOGRAM_TAG, MPI_COMM_WORLD);
    active = 0;
  } else {
    // Receive first chunk of data (blocking)
    MPI_Recv(current_buf, *current_len, MPI_INT, 0, DATA_TAG, MPI_COMM_WORLD, &status);

    // Non-blocking send of ready message to get next chunk
    int dummy_msg = 1;
    MPI_Isend(&dummy_msg, 1, MPI_INT, 0, READY_TAG, MPI_COMM_WORLD, &send_request);

    // Post non-blocking receive for next chunk size
    MPI_Irecv(next_len, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &recv_request);
  }

  // Main processing loop with double buffering using pointers
  while (active) {
    // Process current buffer
    for (int i = 0; i < *current_len; i++) {
      int bin_index = current_buf[i] - range_min;
      if (bin_index >= 0 && bin_index < bins) {
        local_histogram[bin_index]++;
      }
    }

    // Wait for next_len receive to complete
    MPI_Wait(&recv_request, &status);

    // Check if termination signal received
    if (status.MPI_TAG == TERMINATE_TAG || *next_len == 0) {
      MPI_Wait(&send_request, MPI_STATUS_IGNORE);
      MPI_Send(local_histogram, bins, MPI_INT, 0, HISTOGRAM_TAG, MPI_COMM_WORLD);
      active = 0;
      break;
    }

    // Receive the next chunk for next_buf
    MPI_Recv(next_buf, *next_len, MPI_INT, 0, DATA_TAG, MPI_COMM_WORLD, &status);

    // Post non-blocking receive for next chunk size (for current_buf)
    MPI_Wait(&send_request, MPI_STATUS_IGNORE);
    int dummy_msg = 1;
    MPI_Isend(&dummy_msg, 1, MPI_INT, 0, READY_TAG, MPI_COMM_WORLD, &send_request);
    MPI_Irecv(current_len, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &recv_request);

    // Swap buffers and lengths
    int *tmp_buf = current_buf;
    current_buf = next_buf;
    next_buf = tmp_buf;

    int *tmp_len = current_len;
    current_len = next_len;
    next_len = tmp_len;
  }

  // Free resources
  free(buffer_a);
  free(buffer_b);
  free(local_histogram);
}

int main(int argc, char **argv) {
  Arguments args;
  if (argc == 5) {
    args.range_min = atoi(argv[1]);
    args.range_max = atoi(argv[2]);
    args.data_size = atoi(argv[3]);
    args.chunk_size = atoi(argv[4]);
  } else {
    fprintf(stdout,
            "Usage: %s <range_min> <range_max> <data_size> <chunk_size>\n",
            argv[0]);
    return EXIT_FAILURE;
  }

  int rank, size;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (rank == 0) { // Master process
    master(args.range_min, args.range_max, args.data_size, args.chunk_size,
           size);
  } else { // Slave process
    slave(args.range_min, args.range_max, args.data_size, args.chunk_size);
  }

  MPI_Finalize();
  return 0;
}