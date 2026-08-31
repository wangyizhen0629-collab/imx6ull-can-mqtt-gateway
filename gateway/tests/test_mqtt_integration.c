#include "gateway/log.h"
#include "gateway/mqtt_sink.h"
#include "gateway/stats.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { INTEGRATION_BATCH_MAX = 1000000 };

static bool parse_unsigned(const char *text,
                           unsigned long minimum,
                           unsigned long maximum,
                           unsigned long *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (text == NULL || text[0] == '\0' || text[0] == '-') {
        return false;
    }
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || parsed < minimum ||
        parsed > maximum) {
        return false;
    }
    *value = parsed;
    return true;
}

static telemetry_record make_record(uint64_t sequence)
{
    telemetry_record record;
    size_t index;

    (void)memset(&record, 0, sizeof(record));
    record.gateway_seq = sequence;
    record.kernel_timestamp_ns = (int64_t)(sequence * 1000000U);
    record.can_id = 0x100U + (uint32_t)(sequence % 3U);
    record.dlc = GATEWAY_CAN_DATA_SIZE;
    record.ecu_counter = (uint8_t)sequence;
    for (index = 0; index < GATEWAY_CAN_DATA_SIZE; index++) {
        record.data[index] = (uint8_t)(sequence + index);
    }
    for (index = 0; index < GATEWAY_DECODED_PAYLOAD_SIZE; index++) {
        record.decoded_payload[index] = (uint8_t)(sequence + index + 1U);
    }
    record.status_flags = GATEWAY_RECORD_STATUS_KERNEL_TIMESTAMP_VALID |
                          GATEWAY_RECORD_STATUS_CHECKSUM_VALID |
                          GATEWAY_RECORD_STATUS_DECODED_VALID;
    return record;
}

int main(int argc, char **argv)
{
    gateway_mqtt_sink_config config;
    gateway_mqtt_sink_snapshot sink_snapshot;
    gateway_stats_snapshot stats_snapshot;
    gateway_mqtt_sink *sink = NULL;
    gateway_stats stats;
    gateway_logger logger;
    unsigned long port;
    unsigned long batch_count;
    unsigned long index;
    int major = 0;
    int minor = 0;
    int revision = 0;
    int result = 1;

    if (argc != 6 || !parse_unsigned(argv[2], 1, 65535, &port) ||
        !parse_unsigned(argv[4], 1, INTEGRATION_BATCH_MAX, &batch_count)) {
        fprintf(stderr,
                "Usage: %s HOST PORT TOPIC BATCH_COUNT DEVICE_ID\n",
                argv[0]);
        return 2;
    }
    if (gateway_stats_init(&stats) != GATEWAY_OK) {
        return 1;
    }
    if (gateway_logger_init(&logger, stderr, GATEWAY_LOG_INFO) != GATEWAY_OK) {
        gateway_stats_destroy(&stats);
        return 1;
    }
    (void)memset(&config, 0, sizeof(config));
    config.device_id = argv[5];
    config.broker_host = argv[1];
    config.broker_port = (uint16_t)port;
    config.broker_username = "";
    config.broker_password = "";
    config.topic = argv[3];
    config.batch_interval_ms = 60000;
    config.ack_timeout_ms = 5000;
    /* 每条记录立即形成 batch，使门禁验证 batch/PUBACK，而不伪造运行时长。 */
    config.max_records = 1;
    config.stats = &stats;
    config.logger = &logger;
    if (gateway_mqtt_sink_create(&sink, &config) != GATEWAY_OK ||
        gateway_mqtt_sink_connect(sink) != GATEWAY_OK) {
        goto cleanup;
    }
    for (index = 1; index <= batch_count; index++) {
        telemetry_record record = make_record((uint64_t)index);

        if (gateway_mqtt_sink_consume(sink, &record) != GATEWAY_OK) {
            goto cleanup;
        }
    }
    if (gateway_mqtt_sink_flush(sink) != GATEWAY_OK) {
        goto cleanup;
    }
    gateway_mqtt_sink_read(sink, &sink_snapshot);
    gateway_stats_read(&stats, &stats_snapshot);
    gateway_mqtt_library_version(&major, &minor, &revision);
    printf("M6_MQTT_SUMMARY library=%d.%d.%d batches=%" PRIu64
           " records=%" PRIu64 " publish_attempts=%" PRIu64
           " publish_accepted=%" PRIu64 " puback_matched=%" PRIu64
           " puback_unexpected=%" PRIu64 " last_batch_seq=%" PRIu64
           " last_gateway_seq=%" PRIu64 " failed=%d\n",
           major, minor, revision, sink_snapshot.batches_acked,
           sink_snapshot.records_acked,
           stats_snapshot.counters[GATEWAY_STAT_MQTT_PUBLISH_ATTEMPTS],
           stats_snapshot.counters[GATEWAY_STAT_MQTT_PUBLISH_ACCEPTED],
           stats_snapshot.counters[GATEWAY_STAT_MQTT_PUBACK_MATCHED],
           stats_snapshot.counters[GATEWAY_STAT_MQTT_PUBACK_UNEXPECTED],
           sink_snapshot.last_acked_batch_seq,
           sink_snapshot.last_acked_gateway_seq,
           sink_snapshot.failed ? 1 : 0);
    if (!sink_snapshot.failed &&
        sink_snapshot.batches_acked == (uint64_t)batch_count &&
        sink_snapshot.records_acked == (uint64_t)batch_count &&
        sink_snapshot.puback_matched == (uint64_t)batch_count &&
        sink_snapshot.puback_unexpected == 0 &&
        stats_snapshot.counters[GATEWAY_STAT_MQTT_PUBLISH_ATTEMPTS] ==
            (uint64_t)batch_count &&
        stats_snapshot.counters[GATEWAY_STAT_MQTT_PUBLISH_ACCEPTED] ==
            (uint64_t)batch_count &&
        stats_snapshot.counters[GATEWAY_STAT_MQTT_PUBACK_MATCHED] ==
            (uint64_t)batch_count &&
        stats_snapshot.counters[GATEWAY_STAT_MQTT_ERRORS] == 0) {
        result = 0;
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
