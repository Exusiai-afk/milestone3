/**
 * \author {MINGHAO CHEN}
 */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "sbuffer.h"

/**
 * basic node for the buffer, these nodes are linked together to create the buffer
 */
typedef struct sbuffer_node {
    struct sbuffer_node *next;  /**< a pointer to the next node*/
    sensor_data_t data;         /**< a structure containing the data */
} sbuffer_node_t;

/**
 * a structure to keep track of the buffer
 */
struct sbuffer {
    sbuffer_node_t *head;       /**< a pointer to the first node in the buffer */
    sbuffer_node_t *tail;       /**< a pointer to the last node in the buffer */
    pthread_mutex_t mutex;      /**< mutex to protect the buffer */
    pthread_cond_t can_read;    /**< condition variable to signal readers */
    int end_of_stream;          /**< flag to indicate writer has finished */
};

int sbuffer_init(sbuffer_t **buffer) {
    *buffer = malloc(sizeof(sbuffer_t));
    if (*buffer == NULL) return SBUFFER_FAILURE;

    (*buffer)->head = NULL;
    (*buffer)->tail = NULL;
    (*buffer)->end_of_stream = 0;

    if (pthread_mutex_init(&(*buffer)->mutex, NULL) != 0) {
        free(*buffer);
        return SBUFFER_FAILURE;
    }

    if (pthread_cond_init(&(*buffer)->can_read, NULL) != 0) {
        pthread_mutex_destroy(&(*buffer)->mutex);
        free(*buffer);
        return SBUFFER_FAILURE;
    }

    return SBUFFER_SUCCESS;
}

int sbuffer_free(sbuffer_t **buffer) {
    sbuffer_node_t *dummy;
    if ((buffer == NULL) || (*buffer == NULL)) {
        return SBUFFER_FAILURE;
    }

    // Lock to ensure we don't free while someone is using it (though main should join threads first)
    pthread_mutex_lock(&(*buffer)->mutex);

    while ((*buffer)->head) {
        dummy = (*buffer)->head;
        (*buffer)->head = (*buffer)->head->next;
        free(dummy);
    }

    pthread_mutex_unlock(&(*buffer)->mutex);
    pthread_cond_destroy(&(*buffer)->can_read);
    pthread_mutex_destroy(&(*buffer)->mutex);

    free(*buffer);
    *buffer = NULL;
    return SBUFFER_SUCCESS;
}

int sbuffer_remove(sbuffer_t *buffer, sensor_data_t *data) {
    sbuffer_node_t *dummy;
    if (buffer == NULL) return SBUFFER_FAILURE;

    pthread_mutex_lock(&buffer->mutex);

    // Blocking wait: Wait while buffer is empty AND not end-of-stream
    while (buffer->head == NULL) {
        if (buffer->end_of_stream) {
            // Buffer is empty and writer is done -> Readers should stop
            pthread_mutex_unlock(&buffer->mutex);
            return SBUFFER_NO_DATA;
        }
        pthread_cond_wait(&buffer->can_read, &buffer->mutex);
    }

    // Read data
    *data = buffer->head->data;
    dummy = buffer->head;

    if (buffer->head == buffer->tail) { // buffer has only one node
        buffer->head = buffer->tail = NULL;
    } else { // buffer has many nodes
        buffer->head = buffer->head->next;
    }

    free(dummy);

    pthread_mutex_unlock(&buffer->mutex);
    return SBUFFER_SUCCESS;
}

int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data) {
    sbuffer_node_t *dummy;
    if (buffer == NULL) return SBUFFER_FAILURE;

    pthread_mutex_lock(&buffer->mutex);

    // Check for End-Of-Stream marker
    if (data->id == 0) {
        buffer->end_of_stream = 1;
        // Broadcast to wake up ALL waiting readers so they can see the flag and exit
        pthread_cond_broadcast(&buffer->can_read);
        pthread_mutex_unlock(&buffer->mutex);
        return SBUFFER_SUCCESS;
    }

    dummy = malloc(sizeof(sbuffer_node_t));
    if (dummy == NULL) {
        pthread_mutex_unlock(&buffer->mutex);
        return SBUFFER_FAILURE;
    }

    dummy->data = *data;
    dummy->next = NULL;

    if (buffer->tail == NULL) { // buffer empty
        buffer->head = buffer->tail = dummy;
    } else { // buffer not empty
        buffer->tail->next = dummy;
        buffer->tail = buffer->tail->next;
    }

    // Signal a waiting reader that data is available
    pthread_cond_signal(&buffer->can_read);
    pthread_mutex_unlock(&buffer->mutex);

    return SBUFFER_SUCCESS;
}