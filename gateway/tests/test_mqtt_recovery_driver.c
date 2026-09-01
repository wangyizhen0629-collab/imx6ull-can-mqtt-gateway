#include "gateway/log.h"
#include "gateway/mqtt_sink.h"
#include "gateway/stats.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static telemetry_record make_record(uint64_t sequence)
{
    telemetry_record record;

    (void)memset(&record, 0, sizeof(record));
    record.gateway_seq = sequence;
    record.kernel_timestamp_ns = (int64_t)(sequence * 1000000U);
    record.can_id = 0x100U + (uint32_t)(sequence % 3U);
    record.data[0] = (uint8_t)sequence;
    record.dlc = 8;
    record.ecu_counter = (uint8_t)sequence;
    record.status_flags = GATEWAY_RECORD_STATUS_KERNEL_TIMESTAMP_VALID |
                          GATEWAY_RECORD_STATUS_CHECKSUM_VALID |
                          GATEWAY_RECORD_STATUS_DECODED_VALID;
    record.decoded_payload[0] = (uint8_t)(sequence + 1U);
    return record;
}

static void sleep_milliseconds(unsigned int milliseconds)
{
    struct timespec remaining;

    remaining.tv_sec = (time_t)(milliseconds / 1000U);
    remaining.tv_nsec = (long)(milliseconds % 1000U) * 1000000L;
    while (nanosleep(&remaining, &remaining) != 0 && errno == EINTR) {
    }
}

int main(int argc, char **argv)
{
    gateway_mqtt_sink_config config;
    gateway_mqtt_sink_snapshot snapshot;
    gateway_mqtt_sink *sink = NULL;
    gateway_stats stats;
    gateway_logger logger;
    unsigned long port;
    unsigned long append_count;
    uint64_t next_sequence;
    unsigned int loops = 0;
    int result = 1;

    if (argc != 7 ||
        (strcmp(argv[6], "drain") != 0 && strcmp(argv[6], "hold") != 0)) {
        fprintf(stderr,
                "usage: %s SPOOL PORT DEVICE TOPIC APPEND_COUNT drain|hold\n",
                argv[0]);
        return 2;
    }
    port = strtoul(argv[2], NULL, 10);
    append_count = strtoul(argv[5], NULL, 10);
    if (port == 0 || port > UINT16_MAX || append_count > 100000U) {
        return 2;
    }
    if (gateway_stats_init(&stats) != GATEWAY_OK ||
        gateway_logger_init(&logger, stderr, GATEWAY_LOG_INFO) != GATEWAY_OK) {
        return 1;
    }
    (void)memset(&config, 0, sizeof(config));
    config.device_id = argv[3];
    config.broker_host = "127.0.0.1";
    config.broker_port = (uint16_t)port;
    config.broker_username = "";
    config.broker_password = "";
    config.topic = argv[4];
    config.batch_interval_ms = 100;
    config.ack_timeout_ms = 500;
    config.reconnect_interval_ms = 100;
    config.spool_path = argv[1];
    config.max_records = 2;
    config.stats = &stats;
    config.logger = &logger;
    if (gateway_mqtt_sink_create(&sink, &config) != GATEWAY_OK ||
        gateway_mqtt_sink_connect(sink) != GATEWAY_OK) {
        goto cleanup;
    }
    next_sequence = gateway_mqtt_sink_next_gateway_seq(sink);
    while (append_count-- > 0) {
        telemetry_record record = make_record(next_sequence++);

        if (gateway_mqtt_sink_consume(sink, &record) != GATEWAY_OK) {
            goto cleanup;
        }
    }
    gateway_mqtt_sink_read(sink, &snapshot);
    printf("DRIVER_READY next_seq=%" PRIu64 " pending=%" PRIu64 "\n",
           gateway_mqtt_sink_next_gateway_seq(sink),
           snapshot.spool_pending_records);
    (void)fflush(stdout);

    for (;;) {
        if (gateway_mqtt_sink_poll(sink) != GATEWAY_OK) {
            goto cleanup;
        }
        gateway_mqtt_sink_read(sink, &snapshot);
        if (strcmp(argv[6], "drain") == 0 &&
            snapshot.spool_pending_records == 0 && !snapshot.in_flight) {
            printf("DRIVER_DRAINED last_acked_seq=%" PRIu64
                   " batches_acked=%" PRIu64
                   " reactor_enabled=%d epoll_waits=%" PRIu64
                   " wake_events=%" PRIu64 " socket_events=%" PRIu64
                   " loop_read=%" PRIu64 " loop_write=%" PRIu64
                   " loop_misc=%" PRIu64 "\n",
                   snapshot.last_acked_gateway_seq, snapshot.batches_acked,
                   snapshot.reactor_enabled ? 1 : 0,
                   snapshot.reactor_epoll_waits,
                   snapshot.reactor_wake_events,
                   snapshot.reactor_socket_events,
                   snapshot.reactor_loop_read_calls,
                   snapshot.reactor_loop_write_calls,
                   snapshot.reactor_loop_misc_calls);
            if (snapshot.reactor_enabled &&
                snapshot.reactor_epoll_waits > 0 &&
                snapshot.reactor_wake_events > 0 &&
                snapshot.reactor_socket_events > 0 &&
                snapshot.reactor_loop_read_calls > 0 &&
                snapshot.reactor_loop_write_calls > 0) {
                result = 0;
            }
            break;
        }
        if (strcmp(argv[6], "drain") == 0 && ++loops >= 2000U) {
            fprintf(stderr, "drain timeout pending=%" PRIu64 "\n",
                    snapshot.spool_pending_records);
            break;
        }
        sleep_milliseconds(10);
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
