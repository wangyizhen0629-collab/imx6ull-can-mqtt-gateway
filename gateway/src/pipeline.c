#include "gateway/pipeline.h"

#include "gateway/ring_buffer.h"
#include "gateway/vehicle_decoder.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

struct gateway_pipeline {
    gateway_pipeline_config config;
    gateway_ring_buffer *queue;
    pthread_t producer_thread;
    pthread_t consumer_thread;
    pthread_mutex_t state_mutex;
    bool state_mutex_initialized;
    bool producer_created;
    bool consumer_created;
    bool producer_done;
    bool consumer_done;
    bool joined;
    gateway_error_code producer_error;
    gateway_error_code consumer_error;
    uint64_t next_gateway_seq;
};

static void count_can_rejection(gateway_stats *stats,
                                gateway_can_reject_reason reason)
{
    switch (reason) {
    case GATEWAY_CAN_REJECT_DATAGRAM_LENGTH:
        gateway_stats_increment(stats, GATEWAY_STAT_CAN_REJECTED_LENGTH);
        break;
    case GATEWAY_CAN_REJECT_DLC:
        gateway_stats_increment(stats, GATEWAY_STAT_CAN_REJECTED_DLC);
        break;
    case GATEWAY_CAN_REJECT_ID:
        gateway_stats_increment(stats, GATEWAY_STAT_CAN_REJECTED_ID);
        break;
    case GATEWAY_CAN_REJECT_CONTROL_TRUNCATED:
    case GATEWAY_CAN_REJECT_TIMESTAMP_MISSING:
    case GATEWAY_CAN_REJECT_TIMESTAMP_INVALID:
        gateway_stats_increment(stats, GATEWAY_STAT_CAN_TIMESTAMP_ERRORS);
        break;
    case GATEWAY_CAN_REJECT_NONE:
    default:
        gateway_stats_increment(stats, GATEWAY_STAT_CAN_RECEIVE_ERRORS);
        break;
    }
}

static void mark_producer_done(gateway_pipeline *pipeline,
                               gateway_error_code error)
{
    (void)pthread_mutex_lock(&pipeline->state_mutex);
    pipeline->producer_error = error;
    pipeline->producer_done = true;
    (void)pthread_mutex_unlock(&pipeline->state_mutex);
}

static void mark_consumer_done(gateway_pipeline *pipeline,
                               gateway_error_code error)
{
    (void)pthread_mutex_lock(&pipeline->state_mutex);
    pipeline->consumer_error = error;
    pipeline->consumer_done = true;
    (void)pthread_mutex_unlock(&pipeline->state_mutex);
}

static void stop_after_worker_error(gateway_pipeline *pipeline)
{
    (void)gateway_lifecycle_request_stop(pipeline->config.lifecycle, 0);
    (void)gateway_ring_buffer_close(pipeline->queue);
}

static void *run_producer(void *argument)
{
    gateway_pipeline *pipeline = argument;
    gateway_error_code worker_error = GATEWAY_OK;

    for (;;) {
        gateway_can_reject_reason reject_reason = GATEWAY_CAN_REJECT_NONE;
        telemetry_record record;
        gateway_error_code code;
        uint64_t sequence;

        if (gateway_lifecycle_is_stop_requested(pipeline->config.lifecycle,
                                                NULL)) {
            break;
        }
        (void)pthread_mutex_lock(&pipeline->state_mutex);
        sequence = pipeline->next_gateway_seq;
        (void)pthread_mutex_unlock(&pipeline->state_mutex);

        code = pipeline->config.receive(
            pipeline->config.receive_context,
            pipeline->config.receive_timeout_ms,
            sequence,
            &record,
            &reject_reason);
        if (code == GATEWAY_ERROR_CLOSED) {
            break;
        }
        gateway_stats_increment(pipeline->config.stats,
                                GATEWAY_STAT_CAN_RECEIVE_ATTEMPTS);
        if (code == GATEWAY_ERROR_TIMEOUT) {
            gateway_stats_increment(pipeline->config.stats,
                                    GATEWAY_STAT_CAN_RECEIVE_TIMEOUTS);
            continue;
        }
        if (code == GATEWAY_ERROR_INVALID_VALUE) {
            count_can_rejection(pipeline->config.stats, reject_reason);
            continue;
        }
        if (code != GATEWAY_OK) {
            gateway_stats_increment(pipeline->config.stats,
                                    GATEWAY_STAT_CAN_RECEIVE_ERRORS);
            worker_error = code;
            stop_after_worker_error(pipeline);
            break;
        }

        gateway_stats_increment(pipeline->config.stats,
                                GATEWAY_STAT_CAN_RECEIVE_SUCCESS);
        (void)pthread_mutex_lock(&pipeline->state_mutex);
        pipeline->next_gateway_seq++;
        (void)pthread_mutex_unlock(&pipeline->state_mutex);

        if (gateway_vehicle_decode_record(&record) != GATEWAY_DECODE_OK) {
            gateway_stats_increment(pipeline->config.stats,
                                    GATEWAY_STAT_CAN_DECODE_ERRORS);
            continue;
        }
        gateway_stats_increment(pipeline->config.stats,
                                GATEWAY_STAT_CAN_DECODE_SUCCESS);
        code = gateway_ring_buffer_push(pipeline->queue, &record,
                                        pipeline->config.queue_push_timeout_ms);
        if (code == GATEWAY_ERROR_TIMEOUT) {
            /* 满队列时按已冻结策略丢弃新记录，producer 继续接收。 */
            continue;
        }
        if (code == GATEWAY_ERROR_CLOSED &&
            gateway_lifecycle_is_stop_requested(pipeline->config.lifecycle,
                                                NULL)) {
            break;
        }
        if (code != GATEWAY_OK) {
            worker_error = code;
            stop_after_worker_error(pipeline);
            break;
        }
    }
    (void)gateway_ring_buffer_close(pipeline->queue);
    mark_producer_done(pipeline, worker_error);
    return NULL;
}

static void *run_consumer(void *argument)
{
    gateway_pipeline *pipeline = argument;
    gateway_error_code worker_error = GATEWAY_OK;

    for (;;) {
        telemetry_record record;
        gateway_error_code code;

        if (pipeline->config.consume_idle == NULL) {
            code = gateway_ring_buffer_pop(pipeline->queue, &record);
        } else {
            code = gateway_ring_buffer_pop_timed(
                pipeline->queue, &record,
                pipeline->config.consumer_idle_timeout_ms);
        }

        if (code == GATEWAY_ERROR_CLOSED) {
            break;
        }
        if (code == GATEWAY_ERROR_TIMEOUT &&
            pipeline->config.consume_idle != NULL) {
            code = pipeline->config.consume_idle(
                pipeline->config.consume_context);
            if (code == GATEWAY_OK) {
                continue;
            }
        }
        if (code != GATEWAY_OK) {
            worker_error = code;
            stop_after_worker_error(pipeline);
            break;
        }
        code = pipeline->config.consume(pipeline->config.consume_context,
                                        &record);
        if (code != GATEWAY_OK) {
            worker_error = code;
            stop_after_worker_error(pipeline);
            break;
        }
    }
    mark_consumer_done(pipeline, worker_error);
    return NULL;
}

gateway_error_code gateway_pipeline_create(gateway_pipeline **pipeline,
                                           const gateway_pipeline_config *config)
{
    gateway_pipeline *created;

    if (pipeline == NULL || config == NULL || config->queue_capacity == 0 ||
        config->receive_timeout_ms < 0 || config->receive == NULL ||
        config->consume == NULL || config->lifecycle == NULL ||
        config->stats == NULL ||
        (config->consume_idle != NULL &&
         config->consumer_idle_timeout_ms == 0)) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    *pipeline = NULL;
    created = calloc(1, sizeof(*created));
    if (created == NULL) {
        return GATEWAY_ERROR_SYSTEM;
    }
    created->config = *config;
    created->producer_error = GATEWAY_OK;
    created->consumer_error = GATEWAY_OK;
    created->next_gateway_seq = 1;
    if (pthread_mutex_init(&created->state_mutex, NULL) != 0) {
        free(created);
        return GATEWAY_ERROR_SYSTEM;
    }
    created->state_mutex_initialized = true;
    if (gateway_ring_buffer_create(&created->queue, config->queue_capacity,
                                   config->stats) != GATEWAY_OK) {
        (void)pthread_mutex_destroy(&created->state_mutex);
        free(created);
        return GATEWAY_ERROR_SYSTEM;
    }
    *pipeline = created;
    return GATEWAY_OK;
}

void gateway_pipeline_destroy(gateway_pipeline *pipeline)
{
    if (pipeline == NULL) {
        return;
    }
    if ((pipeline->producer_created || pipeline->consumer_created) &&
        !pipeline->joined) {
        (void)gateway_pipeline_request_stop(pipeline, 0);
        (void)gateway_pipeline_join(pipeline);
    }
    gateway_ring_buffer_destroy(pipeline->queue);
    if (pipeline->state_mutex_initialized) {
        (void)pthread_mutex_destroy(&pipeline->state_mutex);
    }
    free(pipeline);
}

gateway_error_code gateway_pipeline_start(gateway_pipeline *pipeline)
{
    if (pipeline == NULL || pipeline->producer_created ||
        pipeline->consumer_created || pipeline->joined) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    if (pthread_create(&pipeline->consumer_thread, NULL, run_consumer,
                       pipeline) != 0) {
        return GATEWAY_ERROR_SYSTEM;
    }
    pipeline->consumer_created = true;
    if (pthread_create(&pipeline->producer_thread, NULL, run_producer,
                       pipeline) != 0) {
        (void)gateway_ring_buffer_close(pipeline->queue);
        (void)pthread_join(pipeline->consumer_thread, NULL);
        pipeline->consumer_created = false;
        return GATEWAY_ERROR_SYSTEM;
    }
    pipeline->producer_created = true;
    return GATEWAY_OK;
}

gateway_error_code gateway_pipeline_request_stop(gateway_pipeline *pipeline,
                                                  int signal_number)
{
    gateway_error_code lifecycle_code;
    gateway_error_code queue_code;

    if (pipeline == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    lifecycle_code = gateway_lifecycle_request_stop(pipeline->config.lifecycle,
                                                    signal_number);
    queue_code = gateway_ring_buffer_close(pipeline->queue);
    return lifecycle_code != GATEWAY_OK ? lifecycle_code : queue_code;
}

gateway_error_code gateway_pipeline_join(gateway_pipeline *pipeline)
{
    if (pipeline == NULL || !pipeline->producer_created ||
        !pipeline->consumer_created) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    if (pipeline->joined) {
        return GATEWAY_OK;
    }
    if (pthread_join(pipeline->producer_thread, NULL) != 0) {
        return GATEWAY_ERROR_SYSTEM;
    }
    (void)gateway_ring_buffer_close(pipeline->queue);
    if (pthread_join(pipeline->consumer_thread, NULL) != 0) {
        return GATEWAY_ERROR_SYSTEM;
    }
    pipeline->joined = true;
    return GATEWAY_OK;
}

void gateway_pipeline_read(gateway_pipeline *pipeline,
                           gateway_pipeline_snapshot *snapshot)
{
    gateway_ring_buffer_snapshot queue_snapshot;

    if (pipeline == NULL || snapshot == NULL) {
        return;
    }
    (void)memset(snapshot, 0, sizeof(*snapshot));
    (void)pthread_mutex_lock(&pipeline->state_mutex);
    snapshot->started = pipeline->producer_created &&
                        pipeline->consumer_created;
    snapshot->producer_done = pipeline->producer_done;
    snapshot->consumer_done = pipeline->consumer_done;
    snapshot->joined = pipeline->joined;
    snapshot->producer_error = pipeline->producer_error;
    snapshot->consumer_error = pipeline->consumer_error;
    snapshot->next_gateway_seq = pipeline->next_gateway_seq;
    (void)pthread_mutex_unlock(&pipeline->state_mutex);
    gateway_ring_buffer_read(pipeline->queue, &queue_snapshot);
    snapshot->queue_capacity = queue_snapshot.capacity;
    snapshot->queue_count = queue_snapshot.count;
    snapshot->queue_closed = queue_snapshot.closed;
}
