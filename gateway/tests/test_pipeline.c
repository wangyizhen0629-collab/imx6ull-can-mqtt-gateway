#include "gateway/lifecycle.h"
#include "gateway/mock_sink.h"
#include "gateway/pipeline.h"
#include "gateway/stats.h"
#include "gateway/vehicle_protocol.h"

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,         \
                    __LINE__, #condition);                                     \
            return 1;                                                          \
        }                                                                      \
    } while (0)

enum {
    SOURCE_RECORD_MAX = 256,
    BASELINE_RECORDS = 111,
    OVERLOAD_RECORDS = 200
};

typedef struct {
    uint32_t can_ids[SOURCE_RECORD_MAX];
    uint32_t due_ms[SOURCE_RECORD_MAX];
    size_t count;
    size_t index;
    size_t corrupt_index;
    bool paced;
    bool start_set;
    bool endless_timeouts;
    struct timespec start;
} synthetic_source;

typedef struct {
    gateway_stats stats;
    gateway_lifecycle lifecycle;
    gateway_mock_sink sink;
    gateway_pipeline *pipeline;
} pipeline_fixture;

static uint8_t calculate_xor(const uint8_t data[GATEWAY_CAN_DATA_SIZE])
{
    uint8_t checksum = 0;
    size_t index;

    for (index = 0; index < GATEWAY_CAN_DATA_SIZE - 1; index++) {
        checksum ^= data[index];
    }
    return checksum;
}

static void add_milliseconds(struct timespec *timestamp, uint32_t delay_ms)
{
    timestamp->tv_sec += (time_t)(delay_ms / 1000U);
    timestamp->tv_nsec += (long)(delay_ms % 1000U) * 1000000L;
    if (timestamp->tv_nsec >= 1000000000L) {
        timestamp->tv_sec++;
        timestamp->tv_nsec -= 1000000000L;
    }
}

static void sleep_milliseconds(uint32_t delay_ms)
{
    struct timespec remaining;

    remaining.tv_sec = (time_t)(delay_ms / 1000U);
    remaining.tv_nsec = (long)(delay_ms % 1000U) * 1000000L;
    while (nanosleep(&remaining, &remaining) != 0 && errno == EINTR) {
    }
}

static double milliseconds_between(const struct timespec *start,
                                   const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) * 1000.0 +
           (double)(end->tv_nsec - start->tv_nsec) / 1000000.0;
}

static gateway_error_code receive_synthetic_record(
    void *context,
    int timeout_ms,
    uint64_t gateway_seq,
    telemetry_record *record,
    gateway_can_reject_reason *reject_reason)
{
    synthetic_source *source = context;
    size_t source_index;

    if (source == NULL || record == NULL || reject_reason == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    *reject_reason = GATEWAY_CAN_REJECT_NONE;
    if (source->endless_timeouts) {
        sleep_milliseconds(timeout_ms > 10 ? 10U : (uint32_t)timeout_ms);
        return GATEWAY_ERROR_TIMEOUT;
    }
    if (source->index >= source->count) {
        return GATEWAY_ERROR_CLOSED;
    }
    if (source->paced) {
        struct timespec deadline;
        int sleep_status;

        if (!source->start_set) {
            if (clock_gettime(CLOCK_MONOTONIC, &source->start) != 0) {
                return GATEWAY_ERROR_SYSTEM;
            }
            source->start_set = true;
        }
        deadline = source->start;
        add_milliseconds(&deadline, source->due_ms[source->index]);
        do {
            sleep_status = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                                           &deadline, NULL);
        } while (sleep_status == EINTR);
        if (sleep_status != 0) {
            return GATEWAY_ERROR_SYSTEM;
        }
    }

    source_index = source->index++;
    (void)memset(record, 0, sizeof(*record));
    record->gateway_seq = gateway_seq;
    record->kernel_timestamp_ns = (int64_t)gateway_seq;
    record->can_id = source->can_ids[source_index];
    record->dlc = GATEWAY_CAN_DATA_SIZE;
    record->ecu_counter = (uint8_t)source_index;
    record->data[6] = record->ecu_counter;
    record->data[7] = calculate_xor(record->data);
    if (source_index == source->corrupt_index) {
        record->data[7] ^= UINT8_C(0xff);
    }
    record->status_flags = GATEWAY_RECORD_STATUS_KERNEL_TIMESTAMP_VALID;
    return GATEWAY_OK;
}

static int fixture_init_at_sequence(pipeline_fixture *fixture,
                                    synthetic_source *source,
                                    size_t capacity,
                                    uint32_t push_timeout_ms,
                                    uint32_t sink_delay_ms,
                                    int receive_timeout_ms,
                                    uint64_t initial_gateway_seq)
{
    gateway_pipeline_config config;

    (void)memset(fixture, 0, sizeof(*fixture));
    CHECK(gateway_stats_init(&fixture->stats) == GATEWAY_OK);
    CHECK(gateway_lifecycle_init(&fixture->lifecycle) == GATEWAY_OK);
    CHECK(gateway_mock_sink_init(&fixture->sink, sink_delay_ms) == GATEWAY_OK);
    (void)memset(&config, 0, sizeof(config));
    config.queue_capacity = capacity;
    config.queue_push_timeout_ms = push_timeout_ms;
    config.receive_timeout_ms = receive_timeout_ms;
    config.receive = receive_synthetic_record;
    config.receive_context = source;
    config.consume = gateway_mock_sink_consume;
    config.consume_context = &fixture->sink;
    config.lifecycle = &fixture->lifecycle;
    config.stats = &fixture->stats;
    config.initial_gateway_seq = initial_gateway_seq;
    CHECK(gateway_pipeline_create(&fixture->pipeline, &config) == GATEWAY_OK);
    return 0;
}

static int fixture_init(pipeline_fixture *fixture,
                        synthetic_source *source,
                        size_t capacity,
                        uint32_t push_timeout_ms,
                        uint32_t sink_delay_ms,
                        int receive_timeout_ms)
{
    return fixture_init_at_sequence(fixture, source, capacity, push_timeout_ms,
                                    sink_delay_ms, receive_timeout_ms, 0);
}

static void fixture_destroy(pipeline_fixture *fixture)
{
    gateway_pipeline_destroy(fixture->pipeline);
    gateway_mock_sink_destroy(&fixture->sink);
    gateway_lifecycle_destroy(&fixture->lifecycle);
    gateway_stats_destroy(&fixture->stats);
}

static void prepare_baseline_source(synthetic_source *source)
{
    size_t tick;

    (void)memset(source, 0, sizeof(*source));
    source->corrupt_index = SIZE_MAX;
    source->paced = true;
    for (tick = 0; tick < 100; tick++) {
        source->can_ids[source->count] =
            GATEWAY_CAN_ID_VEHICLE_DYNAMICS;
        source->due_ms[source->count++] = (uint32_t)tick * 10U;
        if (tick % 10U == 0) {
            source->can_ids[source->count] =
                GATEWAY_CAN_ID_POWER_STATUS;
            source->due_ms[source->count++] = (uint32_t)tick * 10U;
        }
        if (tick == 0) {
            source->can_ids[source->count] = GATEWAY_CAN_ID_BODY_STATUS;
            source->due_ms[source->count++] = 0;
        }
    }
}

static int test_baseline_111_frames_per_second(void)
{
    synthetic_source source;
    pipeline_fixture fixture;
    gateway_stats_snapshot stats;
    gateway_mock_sink_snapshot sink;
    gateway_pipeline_snapshot pipeline;
    struct timespec start;
    struct timespec end;
    double elapsed_ms;

    prepare_baseline_source(&source);
    CHECK(source.count == BASELINE_RECORDS);
    CHECK(fixture_init(&fixture, &source, 16, 50, 0, 20) == 0);
    CHECK(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
    CHECK(gateway_pipeline_start(fixture.pipeline) == GATEWAY_OK);
    CHECK(gateway_pipeline_join(fixture.pipeline) == GATEWAY_OK);
    CHECK(clock_gettime(CLOCK_MONOTONIC, &end) == 0);
    elapsed_ms = milliseconds_between(&start, &end);

    gateway_stats_read(&fixture.stats, &stats);
    gateway_mock_sink_read(&fixture.sink, &sink);
    gateway_pipeline_read(fixture.pipeline, &pipeline);
    CHECK(elapsed_ms >= 800.0);
    CHECK(elapsed_ms < 3000.0);
    CHECK(stats.counters[GATEWAY_STAT_CAN_RECEIVE_ATTEMPTS] ==
          BASELINE_RECORDS);
    CHECK(stats.counters[GATEWAY_STAT_CAN_RECEIVE_SUCCESS] ==
          BASELINE_RECORDS);
    CHECK(stats.counters[GATEWAY_STAT_CAN_DECODE_SUCCESS] ==
          BASELINE_RECORDS);
    CHECK(stats.counters[GATEWAY_STAT_CAN_DECODE_ERRORS] == 0);
    CHECK(stats.counters[GATEWAY_STAT_QUEUE_PUSH_ATTEMPTS] ==
          BASELINE_RECORDS);
    CHECK(stats.counters[GATEWAY_STAT_QUEUE_PUSH_SUCCESS] ==
          BASELINE_RECORDS);
    CHECK(stats.counters[GATEWAY_STAT_QUEUE_PUSH_TIMEOUTS] == 0);
    CHECK(stats.counters[GATEWAY_STAT_QUEUE_POP_SUCCESS] ==
          BASELINE_RECORDS);
    CHECK(stats.queue_high_watermark >= 1);
    CHECK(stats.queue_high_watermark <= 16);
    CHECK(sink.consumed == BASELINE_RECORDS);
    CHECK(sink.sequence_gap_records == 0);
    CHECK(sink.non_monotonic_records == 0);
    CHECK(sink.invalid_records == 0);
    CHECK(pipeline.queue_closed);
    CHECK(pipeline.queue_count == 0);
    CHECK(pipeline.producer_error == GATEWAY_OK);
    CHECK(pipeline.consumer_error == GATEWAY_OK);
    printf("M5_HOST_BASELINE frames=%u elapsed_ms=%.3f queue_drop=%" PRIu64
           " high_watermark=%zu sink_consumed=%" PRIu64 "\n",
           BASELINE_RECORDS, elapsed_ms,
           stats.counters[GATEWAY_STAT_QUEUE_PUSH_TIMEOUTS],
           stats.queue_high_watermark, sink.consumed);
    fixture_destroy(&fixture);
    return 0;
}

static int test_slow_consumer_overload_invariants(void)
{
    synthetic_source source;
    pipeline_fixture fixture;
    gateway_stats_snapshot stats;
    gateway_mock_sink_snapshot sink;
    gateway_pipeline_snapshot pipeline;
    size_t index;

    (void)memset(&source, 0, sizeof(source));
    source.count = OVERLOAD_RECORDS;
    source.corrupt_index = SIZE_MAX;
    for (index = 0; index < source.count; index++) {
        source.can_ids[index] = index % 3U == 0
                                    ? GATEWAY_CAN_ID_VEHICLE_DYNAMICS
                                : index % 3U == 1
                                    ? GATEWAY_CAN_ID_POWER_STATUS
                                    : GATEWAY_CAN_ID_BODY_STATUS;
    }
    CHECK(fixture_init(&fixture, &source, 4, 0, 2, 20) == 0);
    CHECK(gateway_pipeline_start(fixture.pipeline) == GATEWAY_OK);
    CHECK(gateway_pipeline_join(fixture.pipeline) == GATEWAY_OK);
    gateway_stats_read(&fixture.stats, &stats);
    gateway_mock_sink_read(&fixture.sink, &sink);
    gateway_pipeline_read(fixture.pipeline, &pipeline);

    CHECK(stats.counters[GATEWAY_STAT_QUEUE_PUSH_ATTEMPTS] ==
          OVERLOAD_RECORDS);
    CHECK(stats.counters[GATEWAY_STAT_QUEUE_PUSH_TIMEOUTS] > 0);
    CHECK(stats.counters[GATEWAY_STAT_QUEUE_PUSH_SUCCESS] +
              stats.counters[GATEWAY_STAT_QUEUE_PUSH_TIMEOUTS] ==
          OVERLOAD_RECORDS);
    CHECK(stats.counters[GATEWAY_STAT_QUEUE_POP_SUCCESS] ==
          stats.counters[GATEWAY_STAT_QUEUE_PUSH_SUCCESS]);
    CHECK(sink.consumed ==
          stats.counters[GATEWAY_STAT_QUEUE_PUSH_SUCCESS]);
    CHECK(stats.queue_high_watermark == 4);
    CHECK(sink.non_monotonic_records == 0);
    CHECK(sink.invalid_records == 0);
    CHECK(pipeline.queue_closed);
    CHECK(pipeline.queue_count == 0);
    printf("M5_HOST_OVERLOAD produced=%u queued=%" PRIu64
           " queue_drop=%" PRIu64 " consumed=%" PRIu64
           " high_watermark=%zu\n",
           OVERLOAD_RECORDS,
           stats.counters[GATEWAY_STAT_QUEUE_PUSH_SUCCESS],
           stats.counters[GATEWAY_STAT_QUEUE_PUSH_TIMEOUTS], sink.consumed,
           stats.queue_high_watermark);
    fixture_destroy(&fixture);
    return 0;
}

static int test_decode_rejection_is_not_enqueued(void)
{
    synthetic_source source;
    pipeline_fixture fixture;
    gateway_stats_snapshot stats;
    gateway_mock_sink_snapshot sink;

    (void)memset(&source, 0, sizeof(source));
    source.count = 3;
    source.corrupt_index = 1;
    source.can_ids[0] = GATEWAY_CAN_ID_VEHICLE_DYNAMICS;
    source.can_ids[1] = GATEWAY_CAN_ID_POWER_STATUS;
    source.can_ids[2] = GATEWAY_CAN_ID_BODY_STATUS;
    CHECK(fixture_init(&fixture, &source, 4, 0, 0, 20) == 0);
    CHECK(gateway_pipeline_start(fixture.pipeline) == GATEWAY_OK);
    CHECK(gateway_pipeline_join(fixture.pipeline) == GATEWAY_OK);
    gateway_stats_read(&fixture.stats, &stats);
    gateway_mock_sink_read(&fixture.sink, &sink);

    CHECK(stats.counters[GATEWAY_STAT_CAN_RECEIVE_SUCCESS] == 3);
    CHECK(stats.counters[GATEWAY_STAT_CAN_DECODE_SUCCESS] == 2);
    CHECK(stats.counters[GATEWAY_STAT_CAN_DECODE_ERRORS] == 1);
    CHECK(stats.counters[GATEWAY_STAT_QUEUE_PUSH_ATTEMPTS] == 2);
    CHECK(stats.counters[GATEWAY_STAT_QUEUE_PUSH_SUCCESS] == 2);
    CHECK(sink.consumed == 2);
    CHECK(sink.sequence_gap_records == 1);
    CHECK(sink.invalid_records == 0);
    fixture_destroy(&fixture);
    return 0;
}

static int test_sigterm_wakes_and_stops_pipeline(void)
{
    synthetic_source source;
    pipeline_fixture fixture;
    gateway_pipeline_snapshot pipeline;
    gateway_stats_snapshot stats;
    struct timespec start;
    struct timespec end;
    int signal_number = 0;

    (void)memset(&source, 0, sizeof(source));
    source.corrupt_index = SIZE_MAX;
    source.endless_timeouts = true;
    CHECK(fixture_init(&fixture, &source, 4, 10, 0, 20) == 0);
    CHECK(gateway_lifecycle_install_signal_handlers(&fixture.lifecycle) ==
          GATEWAY_OK);
    CHECK(gateway_pipeline_start(fixture.pipeline) == GATEWAY_OK);
    CHECK(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
    CHECK(raise(SIGTERM) == 0);
    CHECK(gateway_lifecycle_wait_signal(&fixture.lifecycle, 1000,
                                        &signal_number) == GATEWAY_OK);
    CHECK(signal_number == SIGTERM);
    CHECK(gateway_pipeline_request_stop(fixture.pipeline, signal_number) ==
          GATEWAY_OK);
    CHECK(gateway_pipeline_join(fixture.pipeline) == GATEWAY_OK);
    CHECK(clock_gettime(CLOCK_MONOTONIC, &end) == 0);
    CHECK(milliseconds_between(&start, &end) < 1000.0);
    gateway_pipeline_read(fixture.pipeline, &pipeline);
    gateway_stats_read(&fixture.stats, &stats);
    CHECK(pipeline.producer_done);
    CHECK(pipeline.consumer_done);
    CHECK(pipeline.queue_closed);
    CHECK(pipeline.queue_count == 0);
    CHECK(pipeline.producer_error == GATEWAY_OK);
    CHECK(pipeline.consumer_error == GATEWAY_OK);
    CHECK(stats.counters[GATEWAY_STAT_QUEUE_POP_CLOSED] == 1);
    printf("M5_HOST_SIGTERM signal=%d exit_ms=%.3f producer_done=1 "
           "consumer_done=1\n",
           signal_number, milliseconds_between(&start, &end));
    fixture_destroy(&fixture);
    return 0;
}

static int test_initial_gateway_sequence_is_honored(void)
{
    synthetic_source source;
    pipeline_fixture fixture;
    gateway_mock_sink_snapshot sink;
    gateway_pipeline_snapshot pipeline;

    (void)memset(&source, 0, sizeof(source));
    source.count = 1;
    source.corrupt_index = SIZE_MAX;
    source.can_ids[0] = GATEWAY_CAN_ID_VEHICLE_DYNAMICS;
    CHECK(fixture_init_at_sequence(&fixture, &source, 4, 0, 0, 20, 42) == 0);
    CHECK(gateway_pipeline_start(fixture.pipeline) == GATEWAY_OK);
    CHECK(gateway_pipeline_join(fixture.pipeline) == GATEWAY_OK);
    gateway_mock_sink_read(&fixture.sink, &sink);
    gateway_pipeline_read(fixture.pipeline, &pipeline);
    CHECK(sink.consumed == 1);
    CHECK(sink.first_gateway_seq == 42);
    CHECK(sink.last_gateway_seq == 42);
    CHECK(pipeline.next_gateway_seq == 43);
    fixture_destroy(&fixture);
    return 0;
}

int main(void)
{
    CHECK(test_baseline_111_frames_per_second() == 0);
    CHECK(test_slow_consumer_overload_invariants() == 0);
    CHECK(test_decode_rejection_is_not_enqueued() == 0);
    CHECK(test_sigterm_wakes_and_stops_pipeline() == 0);
    CHECK(test_initial_gateway_sequence_is_honored() == 0);
    return 0;
}
