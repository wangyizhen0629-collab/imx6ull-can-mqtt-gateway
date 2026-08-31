#include "gateway/mock_sink.h"

#include "gateway/vehicle_decoder.h"

#include <errno.h>
#include <string.h>
#include <time.h>

static void sleep_milliseconds(uint32_t delay_ms)
{
    struct timespec remaining;

    remaining.tv_sec = (time_t)(delay_ms / 1000U);
    remaining.tv_nsec = (long)(delay_ms % 1000U) * 1000000L;
    while (nanosleep(&remaining, &remaining) != 0 && errno == EINTR) {
    }
}

gateway_error_code gateway_mock_sink_init(gateway_mock_sink *sink,
                                           uint32_t delay_ms)
{
    if (sink == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    (void)memset(sink, 0, sizeof(*sink));
    if (pthread_mutex_init(&sink->mutex, NULL) != 0) {
        return GATEWAY_ERROR_SYSTEM;
    }
    sink->delay_ms = delay_ms;
    sink->initialized = true;
    return GATEWAY_OK;
}

void gateway_mock_sink_destroy(gateway_mock_sink *sink)
{
    if (sink != NULL && sink->initialized) {
        (void)pthread_mutex_destroy(&sink->mutex);
        sink->initialized = false;
    }
}

gateway_error_code gateway_mock_sink_consume(void *context,
                                             const telemetry_record *record)
{
    gateway_mock_sink *sink = context;
    gateway_decoded_payload decoded;
    bool valid;

    if (sink == NULL || record == NULL || !sink->initialized) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    if (sink->delay_ms > 0) {
        sleep_milliseconds(sink->delay_ms);
    }

    valid = (record->status_flags &
             (GATEWAY_RECORD_STATUS_KERNEL_TIMESTAMP_VALID |
              GATEWAY_RECORD_STATUS_CHECKSUM_VALID |
              GATEWAY_RECORD_STATUS_DECODED_VALID)) ==
                (GATEWAY_RECORD_STATUS_KERNEL_TIMESTAMP_VALID |
                 GATEWAY_RECORD_STATUS_CHECKSUM_VALID |
                 GATEWAY_RECORD_STATUS_DECODED_VALID) &&
            gateway_vehicle_read_decoded_payload(record, &decoded);

    if (pthread_mutex_lock(&sink->mutex) != 0) {
        return GATEWAY_ERROR_SYSTEM;
    }
    if (sink->values.consumed == 0) {
        sink->values.first_gateway_seq = record->gateway_seq;
    } else if (record->gateway_seq <= sink->values.last_gateway_seq) {
        sink->values.non_monotonic_records++;
    } else if (record->gateway_seq > sink->values.last_gateway_seq + 1U) {
        sink->values.sequence_gap_records +=
            record->gateway_seq - sink->values.last_gateway_seq - 1U;
    }
    sink->values.last_gateway_seq = record->gateway_seq;
    sink->values.consumed++;
    if (!valid) {
        sink->values.invalid_records++;
    }
    (void)pthread_mutex_unlock(&sink->mutex);
    return valid ? GATEWAY_OK : GATEWAY_ERROR_INVALID_VALUE;
}

void gateway_mock_sink_read(gateway_mock_sink *sink,
                            gateway_mock_sink_snapshot *snapshot)
{
    if (sink == NULL || snapshot == NULL || !sink->initialized) {
        return;
    }
    (void)pthread_mutex_lock(&sink->mutex);
    *snapshot = sink->values;
    (void)pthread_mutex_unlock(&sink->mutex);
}
