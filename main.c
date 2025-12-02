/**
 * \author {MINGHAO CHEN}
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "config.h"
#include "sbuffer.h"

// Global shared buffer
sbuffer_t *sbuf;

// File pointer for output CSV
FILE *csv_file;
pthread_mutex_t csv_mutex; // Mutex to ensure lines in CSV don't get mixed

/**
 * Writer Thread
 * Reads 'sensor_data' binary file and inserts into sbuffer.
 */
void *writer_thread(void *arg) {
    FILE *fp = fopen("sensor_data", "rb");
    if (!fp) {
        perror("Failed to open sensor_data");
        pthread_exit(NULL);
    }

    sensor_data_t data;

    // Read raw bytes into the struct until EOF
    while (fread(&data.id, sizeof(sensor_id_t), 1, fp) &&
           fread(&data.value, sizeof(sensor_value_t), 1, fp) &&
           fread(&data.ts, sizeof(sensor_ts_t), 1, fp)) {

        // Insert data into shared buffer
        sbuffer_insert(sbuf, &data);

        // Wait 10ms (also used in EE3)
        usleep(10000);
    }

    // Insert End-of-Stream marker (ID = 0)
    data.id = 0;
    sbuffer_insert(sbuf, &data);

    fclose(fp);
    printf("Writer thread finished.\n");
    return NULL;
}

/**
 * Reader Thread
 * Removes data from sbuffer and writes to 'sensor_data_out.csv'.
 */
void *reader_thread(void *arg) {
    sensor_data_t data;
    int result;

    while (1) {
        // blocking remove
        result = sbuffer_remove(sbuf, &data);

        if (result == SBUFFER_NO_DATA) {
            // Buffer is empty and writer has finished
            break;
        }

        if (result == SBUFFER_SUCCESS) {
            // Write to CSV (protect file access)
            pthread_mutex_lock(&csv_mutex);
            fprintf(csv_file, "%d,%.4f,%ld\n", data.id, data.value, data.ts);
            pthread_mutex_unlock(&csv_mutex);

            // Wait 25ms
            usleep(25000);
        }
    }

    printf("Reader thread finished.\n");
    return NULL;
}

int main() {
    // 1. Initialize Buffer
    if (sbuffer_init(&sbuf) != SBUFFER_SUCCESS) {
        fprintf(stderr, "Failed to init buffer\n");
        return -1;
    }

    // 2. Initialize CSV output and CSV mutex
    csv_file = fopen("sensor_data_out.csv", "w");
    if (!csv_file) {
        perror("Failed to open output file");
        sbuffer_free(&sbuf);
        return -1;
    }
    // Write CSV header
    fprintf(csv_file, "id,value,timestamp\n");
    pthread_mutex_init(&csv_mutex, NULL);

    // 3. Create Threads
    pthread_t writer;
    pthread_t reader1, reader2;

    printf("Starting threads...\n");
    pthread_create(&writer, NULL, writer_thread, NULL);
    pthread_create(&reader1, NULL, reader_thread, NULL);
    pthread_create(&reader2, NULL, reader_thread, NULL);

    // 4. Wait for threads to finish
    pthread_join(writer, NULL);
    pthread_join(reader1, NULL);
    pthread_join(reader2, NULL);

    // 5. Cleanup
    fclose(csv_file);
    pthread_mutex_destroy(&csv_mutex);
    sbuffer_free(&sbuf);

    printf("Main process exiting.\n");
    return 0;
}