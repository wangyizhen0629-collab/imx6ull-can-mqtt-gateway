#include "gateway/ring_buffer.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

struct gateway_ring_buffer {
    telemetry_record *records;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    bool closed;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    clockid_t condition_clock;
    gateway_stats *stats;
};

static void deadline_after(clockid_t clock_id, uint32_t timeout_ms,
                           struct timespec *deadline)
{
    (void)clock_gettime(clock_id, deadline);
    deadline->tv_sec += (time_t)(timeout_ms / 1000U);
    deadline->tv_nsec += (long)(timeout_ms % 1000U) * 1000000L;
    if (deadline->tv_nsec >= 1000000000L) {
        deadline->tv_sec++;
        deadline->tv_nsec -= 1000000000L;
    }
}

gateway_error_code gateway_ring_buffer_create(gateway_ring_buffer **queue,
                                              size_t capacity,
                                              gateway_stats *stats)
{
    gateway_ring_buffer *created;
    pthread_condattr_t attributes;

    if (queue == NULL || capacity == 0 || stats == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    *queue = NULL;
    created = calloc(1, sizeof(*created));
    if (created == NULL) {
        return GATEWAY_ERROR_SYSTEM;
    }
    created->records = calloc(capacity, sizeof(*created->records));
    if (created->records == NULL) {
        free(created);
        return GATEWAY_ERROR_SYSTEM;
    }
    created->capacity = capacity;
    created->stats = stats;
    created->condition_clock = CLOCK_REALTIME;
    if (pthread_mutex_init(&created->mutex, NULL) != 0) {
        free(created->records);
        free(created);
        return GATEWAY_ERROR_SYSTEM;
    }
    if (pthread_condattr_init(&attributes) != 0) {
        (void)pthread_mutex_destroy(&created->mutex);
        free(created->records);
        free(created);
        return GATEWAY_ERROR_SYSTEM;
    }
    if (pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC) == 0) {
        created->condition_clock = CLOCK_MONOTONIC;
    }
    if (pthread_cond_init(&created->not_empty, &attributes) != 0) {
        (void)pthread_condattr_destroy(&attributes);
        (void)pthread_mutex_destroy(&created->mutex);
        free(created->records);
        free(created);
        return GATEWAY_ERROR_SYSTEM;
    }
    if (pthread_cond_init(&created->not_full, &attributes) != 0) {
        (void)pthread_cond_destroy(&created->not_empty);
        (void)pthread_condattr_destroy(&attributes);
        (void)pthread_mutex_destroy(&created->mutex);
        free(created->records);
        free(created);
        return GATEWAY_ERROR_SYSTEM;
    }
    (void)pthread_condattr_destroy(&attributes);
    *queue = created;
    return GATEWAY_OK;
}

void gateway_ring_buffer_destroy(gateway_ring_buffer *queue)
{
    if (queue == NULL) {
        return;
    }
    (void)pthread_cond_destroy(&queue->not_full);
    (void)pthread_cond_destroy(&queue->not_empty);
    (void)pthread_mutex_destroy(&queue->mutex);
    free(queue->records);
    free(queue);
}

gateway_error_code gateway_ring_buffer_push(gateway_ring_buffer *queue,
                                            const telemetry_record *record,
                                            uint32_t timeout_ms)
{
    gateway_error_code result = GATEWAY_OK;
    struct timespec deadline;

    if (queue == NULL || record == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    gateway_stats_increment(queue->stats, GATEWAY_STAT_QUEUE_PUSH_ATTEMPTS);
    if (pthread_mutex_lock(&queue->mutex) != 0) {
        return GATEWAY_ERROR_SYSTEM;
    }
    deadline_after(queue->condition_clock, timeout_ms, &deadline);
    /* while 重新检查谓词，既处理伪唤醒，也保证 close 后不会继续入队。 */
    while (queue->count == queue->capacity && !queue->closed) {
        int status;
        if (timeout_ms == 0) {
            result = GATEWAY_ERROR_TIMEOUT;
            break;
        }
        status = pthread_cond_timedwait(&queue->not_full, &queue->mutex,
                                        &deadline);
        if (status == ETIMEDOUT) {
            result = GATEWAY_ERROR_TIMEOUT;
            break;
        }
        if (status != 0) {
            result = GATEWAY_ERROR_SYSTEM;
            break;
        }
    }
    if (result == GATEWAY_OK && queue->closed) {
        result = GATEWAY_ERROR_CLOSED;
    }
    if (result == GATEWAY_OK) {
        queue->records[queue->tail] = *record;
        queue->tail = (queue->tail + 1U) % queue->capacity;
        queue->count++;
        gateway_stats_increment(queue->stats, GATEWAY_STAT_QUEUE_PUSH_SUCCESS);
        gateway_stats_update_queue_high_watermark(queue->stats, queue->count);
        (void)pthread_cond_signal(&queue->not_empty);
    } else if (result == GATEWAY_ERROR_TIMEOUT) {
        gateway_stats_increment(queue->stats, GATEWAY_STAT_QUEUE_PUSH_TIMEOUTS);
    } else if (result == GATEWAY_ERROR_CLOSED) {
        gateway_stats_increment(queue->stats, GATEWAY_STAT_QUEUE_PUSH_CLOSED);
    }
    (void)pthread_mutex_unlock(&queue->mutex);
    return result;
}

gateway_error_code gateway_ring_buffer_pop(gateway_ring_buffer *queue,
                                           telemetry_record *record)
{
    gateway_error_code result = GATEWAY_OK;

    if (queue == NULL || record == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    if (pthread_mutex_lock(&queue->mutex) != 0) {
        return GATEWAY_ERROR_SYSTEM;
    }
    /* close 后先排空已有记录；只有 closed 且 empty 才向消费者返回 CLOSED。 */
    while (queue->count == 0 && !queue->closed) {
        int status = pthread_cond_wait(&queue->not_empty, &queue->mutex);
        if (status != 0) {
            result = GATEWAY_ERROR_SYSTEM;
            break;
        }
    }
    if (result == GATEWAY_OK && queue->count == 0 && queue->closed) {
        result = GATEWAY_ERROR_CLOSED;
    }
    if (result == GATEWAY_OK) {
        *record = queue->records[queue->head];
        queue->head = (queue->head + 1U) % queue->capacity;
        queue->count--;
        gateway_stats_increment(queue->stats, GATEWAY_STAT_QUEUE_POP_SUCCESS);
        (void)pthread_cond_signal(&queue->not_full);
    } else if (result == GATEWAY_ERROR_CLOSED) {
        gateway_stats_increment(queue->stats, GATEWAY_STAT_QUEUE_POP_CLOSED);
    }
    (void)pthread_mutex_unlock(&queue->mutex);
    return result;
}

gateway_error_code gateway_ring_buffer_close(gateway_ring_buffer *queue)
{
    if (queue == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    if (pthread_mutex_lock(&queue->mutex) != 0) {
        return GATEWAY_ERROR_SYSTEM;
    }
    queue->closed = true;
    (void)pthread_cond_broadcast(&queue->not_empty);
    (void)pthread_cond_broadcast(&queue->not_full);
    (void)pthread_mutex_unlock(&queue->mutex);
    return GATEWAY_OK;
}

void gateway_ring_buffer_read(gateway_ring_buffer *queue,
                              gateway_ring_buffer_snapshot *snapshot)
{
    if (queue == NULL || snapshot == NULL) {
        return;
    }
    (void)pthread_mutex_lock(&queue->mutex);
    snapshot->capacity = queue->capacity;
    snapshot->count = queue->count;
    snapshot->closed = queue->closed;
    (void)pthread_mutex_unlock(&queue->mutex);
}
