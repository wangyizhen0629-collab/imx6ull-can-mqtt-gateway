#ifndef GATEWAY_MOCK_SINK_H
#define GATEWAY_MOCK_SINK_H

#include "gateway/error.h"
#include "gateway/telemetry_record.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t consumed;
    uint64_t sequence_gap_records;
    uint64_t non_monotonic_records;
    uint64_t invalid_records;
    uint64_t first_gateway_seq;
    uint64_t last_gateway_seq;
} gateway_mock_sink_snapshot;

typedef struct {
    pthread_mutex_t mutex;
    gateway_mock_sink_snapshot values;
    uint32_t delay_ms;
    bool initialized;
} gateway_mock_sink;

gateway_error_code gateway_mock_sink_init(gateway_mock_sink *sink,
                                           uint32_t delay_ms);
void gateway_mock_sink_destroy(gateway_mock_sink *sink);
gateway_error_code gateway_mock_sink_consume(void *context,
                                             const telemetry_record *record);
void gateway_mock_sink_read(gateway_mock_sink *sink,
                            gateway_mock_sink_snapshot *snapshot);

#endif
