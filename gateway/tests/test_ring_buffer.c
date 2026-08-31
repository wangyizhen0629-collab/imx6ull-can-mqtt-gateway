#include "gateway/ring_buffer.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,         \
                    __LINE__, #condition);                                     \
            return 1;                                                          \
        }                                                                      \
    } while (0)

typedef struct {
    gateway_ring_buffer *queue;
    gateway_error_code result;
} blocked_context;

static void sleep_milliseconds(long milliseconds)
{
    struct timespec duration;
    duration.tv_sec = milliseconds / 1000;
    duration.tv_nsec = (milliseconds % 1000) * 1000000L;
    (void)nanosleep(&duration, NULL);
}

static double milliseconds_between(const struct timespec *start,
                                   const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) * 1000.0 +
           (double)(end->tv_nsec - start->tv_nsec) / 1000000.0;
}

static void *blocked_pop(void *argument)
{
    blocked_context *context = argument;
    telemetry_record record;
    context->result = gateway_ring_buffer_pop(context->queue, &record);
    return NULL;
}

static void *blocked_push(void *argument)
{
    blocked_context *context = argument;
    telemetry_record record = {0};
    context->result = gateway_ring_buffer_push(context->queue, &record, 5000);
    return NULL;
}

static int test_fifo_full_timeout_and_stats(void)
{
    gateway_stats stats;
    gateway_stats_snapshot stats_snapshot;
    gateway_ring_buffer *queue;
    gateway_ring_buffer_snapshot queue_snapshot;
    telemetry_record input = {0};
    telemetry_record output = {0};
    struct timespec start;
    struct timespec end;

    CHECK(gateway_stats_init(&stats) == GATEWAY_OK);
    CHECK(gateway_ring_buffer_create(&queue, 3, &stats) == GATEWAY_OK);
    input.gateway_seq = 1;
    CHECK(gateway_ring_buffer_push(queue, &input, 0) == GATEWAY_OK);
    input.gateway_seq = 2;
    CHECK(gateway_ring_buffer_push(queue, &input, 0) == GATEWAY_OK);
    input.gateway_seq = 3;
    CHECK(gateway_ring_buffer_push(queue, &input, 0) == GATEWAY_OK);
    input.gateway_seq = 4;
    CHECK(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
    CHECK(gateway_ring_buffer_push(queue, &input, 40) == GATEWAY_ERROR_TIMEOUT);
    CHECK(clock_gettime(CLOCK_MONOTONIC, &end) == 0);
    CHECK(milliseconds_between(&start, &end) >= 20.0);
    CHECK(milliseconds_between(&start, &end) < 1000.0);

    CHECK(gateway_ring_buffer_pop(queue, &output) == GATEWAY_OK);
    CHECK(output.gateway_seq == 1);
    CHECK(gateway_ring_buffer_pop(queue, &output) == GATEWAY_OK);
    CHECK(output.gateway_seq == 2);
    CHECK(gateway_ring_buffer_pop(queue, &output) == GATEWAY_OK);
    CHECK(output.gateway_seq == 3);
    gateway_ring_buffer_read(queue, &queue_snapshot);
    CHECK(queue_snapshot.capacity == 3);
    CHECK(queue_snapshot.count == 0);
    CHECK(!queue_snapshot.closed);
    gateway_stats_read(&stats, &stats_snapshot);
    CHECK(stats_snapshot.counters[GATEWAY_STAT_QUEUE_PUSH_ATTEMPTS] == 4);
    CHECK(stats_snapshot.counters[GATEWAY_STAT_QUEUE_PUSH_SUCCESS] == 3);
    CHECK(stats_snapshot.counters[GATEWAY_STAT_QUEUE_PUSH_TIMEOUTS] == 1);
    CHECK(stats_snapshot.counters[GATEWAY_STAT_QUEUE_POP_SUCCESS] == 3);
    CHECK(stats_snapshot.queue_high_watermark == 3);
    CHECK(gateway_ring_buffer_close(queue) == GATEWAY_OK);
    CHECK(gateway_ring_buffer_pop(queue, &output) == GATEWAY_ERROR_CLOSED);
    gateway_ring_buffer_destroy(queue);
    gateway_stats_destroy(&stats);
    return 0;
}

static int test_close_broadcast(void)
{
    gateway_stats stats;
    gateway_ring_buffer *queue;
    pthread_t thread;
    blocked_context context = {0};
    telemetry_record record = {0};

    CHECK(gateway_stats_init(&stats) == GATEWAY_OK);
    CHECK(gateway_ring_buffer_create(&queue, 1, &stats) == GATEWAY_OK);
    context.queue = queue;
    CHECK(pthread_create(&thread, NULL, blocked_pop, &context) == 0);
    sleep_milliseconds(20);
    CHECK(gateway_ring_buffer_close(queue) == GATEWAY_OK);
    CHECK(pthread_join(thread, NULL) == 0);
    CHECK(context.result == GATEWAY_ERROR_CLOSED);
    gateway_ring_buffer_destroy(queue);
    gateway_stats_destroy(&stats);

    CHECK(gateway_stats_init(&stats) == GATEWAY_OK);
    CHECK(gateway_ring_buffer_create(&queue, 1, &stats) == GATEWAY_OK);
    CHECK(gateway_ring_buffer_push(queue, &record, 0) == GATEWAY_OK);
    context.queue = queue;
    context.result = GATEWAY_OK;
    CHECK(pthread_create(&thread, NULL, blocked_push, &context) == 0);
    sleep_milliseconds(20);
    CHECK(gateway_ring_buffer_close(queue) == GATEWAY_OK);
    CHECK(pthread_join(thread, NULL) == 0);
    CHECK(context.result == GATEWAY_ERROR_CLOSED);
    CHECK(gateway_ring_buffer_pop(queue, &record) == GATEWAY_OK);
    CHECK(gateway_ring_buffer_pop(queue, &record) == GATEWAY_ERROR_CLOSED);
    gateway_ring_buffer_destroy(queue);
    gateway_stats_destroy(&stats);
    return 0;
}

static int test_timed_pop(void)
{
    gateway_stats stats;
    gateway_ring_buffer *queue;
    telemetry_record record = {0};
    struct timespec start;
    struct timespec end;

    CHECK(gateway_stats_init(&stats) == GATEWAY_OK);
    CHECK(gateway_ring_buffer_create(&queue, 1, &stats) == GATEWAY_OK);
    CHECK(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
    CHECK(gateway_ring_buffer_pop_timed(queue, &record, 30) ==
          GATEWAY_ERROR_TIMEOUT);
    CHECK(clock_gettime(CLOCK_MONOTONIC, &end) == 0);
    CHECK(milliseconds_between(&start, &end) >= 15.0);
    CHECK(milliseconds_between(&start, &end) < 1000.0);
    CHECK(gateway_ring_buffer_close(queue) == GATEWAY_OK);
    CHECK(gateway_ring_buffer_pop_timed(queue, &record, 30) ==
          GATEWAY_ERROR_CLOSED);
    gateway_ring_buffer_destroy(queue);
    gateway_stats_destroy(&stats);
    return 0;
}

enum {
    PRODUCER_COUNT = 3,
    CONSUMER_COUNT = 2,
    RECORDS_PER_PRODUCER = 2000,
    TOTAL_RECORDS = PRODUCER_COUNT * RECORDS_PER_PRODUCER
};

typedef struct {
    gateway_ring_buffer *queue;
    unsigned int producer_index;
    gateway_error_code error;
} producer_context;

typedef struct {
    gateway_ring_buffer *queue;
    bool *seen;
    pthread_mutex_t *seen_mutex;
    bool duplicate;
    gateway_error_code error;
} consumer_context;

static void *produce_records(void *argument)
{
    producer_context *context = argument;
    unsigned int index;

    context->error = GATEWAY_OK;
    for (index = 0; index < RECORDS_PER_PRODUCER; index++) {
        telemetry_record record = {0};
        record.gateway_seq =
            (uint64_t)context->producer_index * RECORDS_PER_PRODUCER + index;
        context->error = gateway_ring_buffer_push(context->queue, &record, 10000);
        if (context->error != GATEWAY_OK) {
            break;
        }
    }
    return NULL;
}

static void *consume_records(void *argument)
{
    consumer_context *context = argument;

    context->error = GATEWAY_OK;
    for (;;) {
        telemetry_record record;
        gateway_error_code result = gateway_ring_buffer_pop(context->queue,
                                                             &record);
        if (result == GATEWAY_ERROR_CLOSED) {
            break;
        }
        if (result != GATEWAY_OK || record.gateway_seq >= TOTAL_RECORDS) {
            context->error = result == GATEWAY_OK ? GATEWAY_ERROR_INVALID_VALUE
                                                   : result;
            break;
        }
        (void)pthread_mutex_lock(context->seen_mutex);
        if (context->seen[record.gateway_seq]) {
            context->duplicate = true;
        }
        context->seen[record.gateway_seq] = true;
        (void)pthread_mutex_unlock(context->seen_mutex);
    }
    return NULL;
}

static int test_concurrent_invariants(void)
{
    gateway_stats stats;
    gateway_stats_snapshot stats_snapshot;
    gateway_ring_buffer *queue;
    gateway_ring_buffer_snapshot queue_snapshot;
    producer_context producers[PRODUCER_COUNT];
    consumer_context consumers[CONSUMER_COUNT];
    pthread_t producer_threads[PRODUCER_COUNT];
    pthread_t consumer_threads[CONSUMER_COUNT];
    pthread_mutex_t seen_mutex;
    bool *seen = calloc(TOTAL_RECORDS, sizeof(*seen));
    unsigned int index;

    CHECK(seen != NULL);
    CHECK(pthread_mutex_init(&seen_mutex, NULL) == 0);
    CHECK(gateway_stats_init(&stats) == GATEWAY_OK);
    CHECK(gateway_ring_buffer_create(&queue, 32, &stats) == GATEWAY_OK);

    for (index = 0; index < CONSUMER_COUNT; index++) {
        consumers[index].queue = queue;
        consumers[index].seen = seen;
        consumers[index].seen_mutex = &seen_mutex;
        consumers[index].duplicate = false;
        consumers[index].error = GATEWAY_OK;
        CHECK(pthread_create(&consumer_threads[index], NULL, consume_records,
                             &consumers[index]) == 0);
    }
    for (index = 0; index < PRODUCER_COUNT; index++) {
        producers[index].queue = queue;
        producers[index].producer_index = index;
        producers[index].error = GATEWAY_OK;
        CHECK(pthread_create(&producer_threads[index], NULL, produce_records,
                             &producers[index]) == 0);
    }
    for (index = 0; index < PRODUCER_COUNT; index++) {
        CHECK(pthread_join(producer_threads[index], NULL) == 0);
        CHECK(producers[index].error == GATEWAY_OK);
    }
    CHECK(gateway_ring_buffer_close(queue) == GATEWAY_OK);
    for (index = 0; index < CONSUMER_COUNT; index++) {
        CHECK(pthread_join(consumer_threads[index], NULL) == 0);
        CHECK(consumers[index].error == GATEWAY_OK);
        CHECK(!consumers[index].duplicate);
    }
    for (index = 0; index < TOTAL_RECORDS; index++) {
        CHECK(seen[index]);
    }

    gateway_ring_buffer_read(queue, &queue_snapshot);
    CHECK(queue_snapshot.closed);
    CHECK(queue_snapshot.count == 0);
    gateway_stats_read(&stats, &stats_snapshot);
    CHECK(stats_snapshot.counters[GATEWAY_STAT_QUEUE_PUSH_ATTEMPTS] ==
          TOTAL_RECORDS);
    CHECK(stats_snapshot.counters[GATEWAY_STAT_QUEUE_PUSH_SUCCESS] ==
          TOTAL_RECORDS);
    CHECK(stats_snapshot.counters[GATEWAY_STAT_QUEUE_PUSH_TIMEOUTS] == 0);
    CHECK(stats_snapshot.counters[GATEWAY_STAT_QUEUE_POP_SUCCESS] ==
          TOTAL_RECORDS);
    CHECK(stats_snapshot.counters[GATEWAY_STAT_QUEUE_POP_CLOSED] ==
          CONSUMER_COUNT);
    CHECK(stats_snapshot.queue_high_watermark >= 1);
    CHECK(stats_snapshot.queue_high_watermark <= 32);

    gateway_ring_buffer_destroy(queue);
    gateway_stats_destroy(&stats);
    CHECK(pthread_mutex_destroy(&seen_mutex) == 0);
    free(seen);
    return 0;
}

int main(void)
{
    CHECK(test_fifo_full_timeout_and_stats() == 0);
    CHECK(test_close_broadcast() == 0);
    CHECK(test_timed_pop() == 0);
    CHECK(test_concurrent_invariants() == 0);
    return 0;
}
