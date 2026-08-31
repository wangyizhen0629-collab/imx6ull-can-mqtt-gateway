#include "gateway/log.h"
#include "gateway/mqtt_sink.h"
#include "gateway/stats.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static bool parse_port(const char *text, uint16_t *port)
{
    char *end = NULL;
    unsigned long value;

    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || value == 0 ||
        value > 65535) {
        return false;
    }
    *port = (uint16_t)value;
    return true;
}

static double elapsed_milliseconds(const struct timespec *start,
                                   const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) * 1000.0 +
           (double)(end->tv_nsec - start->tv_nsec) / 1000000.0;
}

int main(int argc, char **argv)
{
    gateway_mqtt_sink_config config;
    gateway_mqtt_sink_snapshot snapshot;
    gateway_mqtt_sink *sink = NULL;
    gateway_stats stats;
    gateway_logger logger;
    telemetry_record record;
    struct timespec start;
    struct timespec end;
    struct timespec sleep_time = {0, 10000000L};
    uint16_t port;
    int result = 1;

    if (argc != 5 || !parse_port(argv[2], &port)) {
        fprintf(stderr, "Usage: %s HOST PORT TOPIC DEVICE_ID\n", argv[0]);
        return 2;
    }
    if (gateway_stats_init(&stats) != GATEWAY_OK ||
        gateway_logger_init(&logger, stderr, GATEWAY_LOG_INFO) != GATEWAY_OK) {
        return 1;
    }
    (void)memset(&config, 0, sizeof(config));
    config.device_id = argv[4];
    config.broker_host = argv[1];
    config.broker_port = port;
    config.broker_username = "";
    config.broker_password = "";
    config.topic = argv[3];
    config.batch_interval_ms = 100;
    config.ack_timeout_ms = 2000;
    config.max_records = GATEWAY_MQTT_BATCH_MAX_RECORDS;
    config.stats = &stats;
    config.logger = &logger;
    if (gateway_mqtt_sink_create(&sink, &config) != GATEWAY_OK ||
        gateway_mqtt_sink_connect(sink) != GATEWAY_OK) {
        goto cleanup;
    }

    (void)memset(&record, 0, sizeof(record));
    record.gateway_seq = 1;
    record.kernel_timestamp_ns = 1;
    record.can_id = 0x100;
    record.dlc = GATEWAY_CAN_DATA_SIZE;
    record.status_flags = GATEWAY_RECORD_STATUS_KERNEL_TIMESTAMP_VALID |
                          GATEWAY_RECORD_STATUS_CHECKSUM_VALID |
                          GATEWAY_RECORD_STATUS_DECODED_VALID;
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0 ||
        gateway_mqtt_sink_consume(sink, &record) != GATEWAY_OK) {
        goto cleanup;
    }
    for (;;) {
        double elapsed;

        if (gateway_mqtt_sink_poll(sink) != GATEWAY_OK ||
            clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
            goto cleanup;
        }
        gateway_mqtt_sink_read(sink, &snapshot);
        elapsed = elapsed_milliseconds(&start, &end);
        if (snapshot.batches_acked == 1) {
            printf("M6_MQTT_TIMING_SUMMARY interval_ms=100 elapsed_ms=%.3f "
                   "batches=%" PRIu64 " records=%" PRIu64
                   " puback_matched=%" PRIu64 " buffered=%zu failed=%d\n",
                   elapsed, snapshot.batches_acked, snapshot.records_acked,
                   snapshot.puback_matched, snapshot.buffered_records,
                   snapshot.failed ? 1 : 0);
            if (elapsed >= 70.0 && elapsed < 2000.0 &&
                snapshot.records_acked == 1 &&
                snapshot.puback_matched == 1 &&
                snapshot.puback_unexpected == 0 &&
                snapshot.buffered_records == 0 && !snapshot.failed) {
                result = 0;
            }
            break;
        }
        if (elapsed >= 2000.0) {
            break;
        }
        (void)nanosleep(&sleep_time, NULL);
    }

cleanup:
    if (sink != NULL) {
        (void)gateway_mqtt_sink_disconnect(sink);
    }
    gateway_mqtt_sink_destroy(sink);
    gateway_logger_destroy(&logger);
    gateway_stats_destroy(&stats);
    return result;
}
