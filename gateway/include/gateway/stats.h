#ifndef GATEWAY_STATS_H
#define GATEWAY_STATS_H

#include "gateway/error.h"

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    GATEWAY_STAT_QUEUE_PUSH_ATTEMPTS = 0,
    GATEWAY_STAT_QUEUE_PUSH_SUCCESS = 1,
    GATEWAY_STAT_QUEUE_PUSH_TIMEOUTS = 2,
    GATEWAY_STAT_QUEUE_PUSH_CLOSED = 3,
    GATEWAY_STAT_QUEUE_POP_SUCCESS = 4,
    GATEWAY_STAT_QUEUE_POP_CLOSED = 5,
    GATEWAY_STAT_CAN_RECEIVE_ATTEMPTS = 6,
    GATEWAY_STAT_CAN_RECEIVE_SUCCESS = 7,
    GATEWAY_STAT_CAN_RECEIVE_TIMEOUTS = 8,
    GATEWAY_STAT_CAN_REJECTED_LENGTH = 9,
    GATEWAY_STAT_CAN_REJECTED_DLC = 10,
    GATEWAY_STAT_CAN_REJECTED_ID = 11,
    GATEWAY_STAT_CAN_TIMESTAMP_ERRORS = 12,
    GATEWAY_STAT_CAN_RECEIVE_ERRORS = 13,
    GATEWAY_STAT_COUNT = 14
} gateway_stat_counter;

typedef struct {
    uint64_t counters[GATEWAY_STAT_COUNT];
    size_t queue_high_watermark;
} gateway_stats_snapshot;

typedef struct {
    pthread_mutex_t mutex;
    gateway_stats_snapshot values;
} gateway_stats;

gateway_error_code gateway_stats_init(gateway_stats *stats);
void gateway_stats_destroy(gateway_stats *stats);
void gateway_stats_increment(gateway_stats *stats, gateway_stat_counter counter);
void gateway_stats_update_queue_high_watermark(gateway_stats *stats,
                                               size_t count);
void gateway_stats_read(gateway_stats *stats, gateway_stats_snapshot *snapshot);

#endif
