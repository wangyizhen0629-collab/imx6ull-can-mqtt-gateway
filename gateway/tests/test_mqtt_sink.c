#include "gateway/log.h"
#include "gateway/mqtt_sink.h"
#include "gateway/stats.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,         \
                    __LINE__, #condition);                                     \
            return 1;                                                          \
        }                                                                      \
    } while (0)

static telemetry_record make_record(uint64_t sequence, uint8_t marker)
{
    telemetry_record record;

    (void)memset(&record, 0, sizeof(record));
    record.gateway_seq = sequence;
    record.kernel_timestamp_ns = (int64_t)(sequence * 1000U);
    record.can_id = 0x100U + marker % 3U;
    record.dlc = GATEWAY_CAN_DATA_SIZE;
    record.ecu_counter = marker;
    record.data[0] = marker;
    record.decoded_payload[0] = (uint8_t)(marker + 1U);
    record.status_flags = GATEWAY_RECORD_STATUS_KERNEL_TIMESTAMP_VALID |
                          GATEWAY_RECORD_STATUS_CHECKSUM_VALID |
                          GATEWAY_RECORD_STATUS_DECODED_VALID;
    return record;
}

static int test_batch_encoding(void)
{
    telemetry_record records[2];
    char payload[2048];
    char small_payload[32];
    size_t payload_size = 0;

    records[0] = make_record(7, 1);
    records[1] = make_record(9, 2);
    CHECK(gateway_mqtt_encode_batch(payload, sizeof(payload),
                                    "unit-gateway", 3, records, 2,
                                    &payload_size) == GATEWAY_OK);
    CHECK(payload_size == strlen(payload));
    CHECK(strstr(payload, "\"schema\":\"gateway.telemetry.v1\"") != NULL);
    CHECK(strstr(payload, "\"device_id\":\"unit-gateway\"") != NULL);
    CHECK(strstr(payload, "\"batch_seq\":3") != NULL);
    CHECK(strstr(payload, "\"record_count\":2") != NULL);
    CHECK(strstr(payload, "\"first_seq\":7") != NULL);
    CHECK(strstr(payload, "\"last_seq\":9") != NULL);
    CHECK(strstr(payload, "\"data\":\"0100000000000000\"") != NULL);
    CHECK(strstr(payload, "\"decoded_payload\":\"0200") != NULL);

    CHECK(gateway_mqtt_encode_batch(small_payload, sizeof(small_payload),
                                    "unit-gateway", 3, records, 2,
                                    &payload_size) == GATEWAY_ERROR_RANGE);
    records[1].gateway_seq = records[0].gateway_seq;
    CHECK(gateway_mqtt_encode_batch(payload, sizeof(payload),
                                    "unit-gateway", 3, records, 2,
                                    &payload_size) ==
          GATEWAY_ERROR_INVALID_VALUE);
    return 0;
}

static int test_library_and_create_validation(void)
{
    gateway_mqtt_sink_config config;
    gateway_mqtt_sink *sink = NULL;
    gateway_stats stats;
    gateway_logger logger;
    int major = 0;
    int minor = 0;
    int revision = 0;

    gateway_mqtt_library_version(&major, &minor, &revision);
    CHECK(major > 0);
    CHECK(minor >= 0);
    CHECK(revision >= 0);
    CHECK(gateway_stats_init(&stats) == GATEWAY_OK);
    CHECK(gateway_logger_init(&logger, stderr, GATEWAY_LOG_ERROR) ==
          GATEWAY_OK);
    (void)memset(&config, 0, sizeof(config));
    config.device_id = "unit-gateway";
    config.broker_host = "127.0.0.1";
    config.broker_port = 1883;
    config.broker_username = "";
    config.broker_password = "";
    config.topic = "test/m6/unit";
    config.batch_interval_ms = 1000;
    config.ack_timeout_ms = 1000;
    config.max_records = 2;
    config.stats = &stats;
    config.logger = &logger;
    CHECK(gateway_mqtt_sink_create(&sink, &config) == GATEWAY_OK);
    gateway_mqtt_sink_destroy(sink);
    sink = NULL;
    config.max_records = 0;
    CHECK(gateway_mqtt_sink_create(&sink, &config) ==
          GATEWAY_ERROR_ARGUMENT);
    gateway_logger_destroy(&logger);
    gateway_stats_destroy(&stats);
    return 0;
}

int main(void)
{
    CHECK(test_batch_encoding() == 0);
    CHECK(test_library_and_create_validation() == 0);
    return 0;
}
