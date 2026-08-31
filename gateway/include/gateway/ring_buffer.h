#ifndef GATEWAY_RING_BUFFER_H
#define GATEWAY_RING_BUFFER_H

#include "gateway/error.h"
#include "gateway/stats.h"
#include "gateway/telemetry_record.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gateway_ring_buffer gateway_ring_buffer;

typedef struct {
    size_t capacity;
    size_t count;
    bool closed;
} gateway_ring_buffer_snapshot;

gateway_error_code gateway_ring_buffer_create(gateway_ring_buffer **queue,
                                              size_t capacity,
                                              gateway_stats *stats);
void gateway_ring_buffer_destroy(gateway_ring_buffer *queue);
gateway_error_code gateway_ring_buffer_push(gateway_ring_buffer *queue,
                                            const telemetry_record *record,
                                            uint32_t timeout_ms);
gateway_error_code gateway_ring_buffer_pop(gateway_ring_buffer *queue,
                                           telemetry_record *record);
gateway_error_code gateway_ring_buffer_pop_timed(gateway_ring_buffer *queue,
                                                 telemetry_record *record,
                                                 uint32_t timeout_ms);
gateway_error_code gateway_ring_buffer_close(gateway_ring_buffer *queue);
void gateway_ring_buffer_read(gateway_ring_buffer *queue,
                              gateway_ring_buffer_snapshot *snapshot);

#endif
