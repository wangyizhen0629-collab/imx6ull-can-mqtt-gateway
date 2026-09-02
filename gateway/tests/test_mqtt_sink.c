#include "gateway/log.h"
#include "gateway/mqtt_sink.h"
#include "gateway/stats.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

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

static void sleep_milliseconds(unsigned int milliseconds)
{
    struct timespec remaining;

    remaining.tv_sec = (time_t)(milliseconds / 1000U);
    remaining.tv_nsec = (long)(milliseconds % 1000U) * 1000000L;
    while (nanosleep(&remaining, &remaining) != 0 && errno == EINTR) {
    }
}

static double elapsed_milliseconds(const struct timespec *start,
                                   const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) * 1000.0 +
           (double)(end->tv_nsec - start->tv_nsec) / 1000000.0;
}

static int create_silent_loopback_listener(uint16_t *port)
{
    struct sockaddr_in address;
    socklen_t address_size = sizeof(address);
    int descriptor;

    descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (descriptor < 0) {
        return -1;
    }
    (void)memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(descriptor, (const struct sockaddr *)&address,
             sizeof(address)) != 0 ||
        getsockname(descriptor, (struct sockaddr *)&address,
                    &address_size) != 0) {
        (void)close(descriptor);
        return -1;
    }
    if (listen(descriptor, 8) != 0) {
        (void)close(descriptor);
        return -1;
    }
    *port = ntohs(address.sin_port);
    return descriptor;
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

static int test_durable_offline_append_and_sequence_recovery(void)
{
    char directory[] = "/tmp/gateway-mqtt-spool-test-XXXXXX";
    char spool_path[256];
    gateway_mqtt_sink_config config;
    gateway_mqtt_sink_snapshot snapshot;
    gateway_mqtt_sink *sink = NULL;
    gateway_stats stats;
    gateway_logger logger;
    telemetry_record record;

    CHECK(mkdtemp(directory) != NULL);
    CHECK(snprintf(spool_path, sizeof(spool_path), "%s/spool.data",
                   directory) > 0);
    CHECK(gateway_stats_init(&stats) == GATEWAY_OK);
    CHECK(gateway_logger_init(&logger, stderr, GATEWAY_LOG_ERROR) ==
          GATEWAY_OK);
    (void)memset(&config, 0, sizeof(config));
    config.device_id = "unit-gateway";
    config.broker_host = "127.0.0.1";
    config.broker_port = 1883;
    config.broker_username = "";
    config.broker_password = "";
    config.topic = "test/m7/unit";
    config.batch_interval_ms = 1000;
    config.ack_timeout_ms = 1000;
    config.reconnect_interval_ms = 100;
    config.spool_path = spool_path;
    config.max_records = 2;
    config.stats = &stats;
    config.logger = &logger;
    CHECK(gateway_mqtt_sink_create(&sink, &config) == GATEWAY_OK);
    CHECK(gateway_mqtt_sink_next_gateway_seq(sink) == 1);
    record = make_record(1, 1);
    CHECK(gateway_mqtt_sink_consume(sink, &record) == GATEWAY_OK);
    record = make_record(2, 2);
    CHECK(gateway_mqtt_sink_consume(sink, &record) == GATEWAY_OK);
    gateway_mqtt_sink_read(sink, &snapshot);
    CHECK(snapshot.durable);
    CHECK(snapshot.spool_total_records == 2);
    CHECK(snapshot.spool_pending_records == 2);
    CHECK(snapshot.spool_records_appended == 2);
    CHECK(gateway_mqtt_sink_next_gateway_seq(sink) == 3);
    gateway_mqtt_sink_destroy(sink);
    sink = NULL;

    CHECK(gateway_mqtt_sink_create(&sink, &config) == GATEWAY_OK);
    gateway_mqtt_sink_read(sink, &snapshot);
    CHECK(snapshot.spool_pending_records == 2);
    CHECK(snapshot.spool_state_recoveries == 1);
    CHECK(gateway_mqtt_sink_next_gateway_seq(sink) == 3);
    gateway_mqtt_sink_destroy(sink);
    gateway_logger_destroy(&logger);
    gateway_stats_destroy(&stats);
    CHECK(unlink(spool_path) == 0);
    CHECK(rmdir(directory) == 0);
    return 0;
}

static int test_durable_reconnect_during_continuous_consume(void)
{
    char directory[] = "/tmp/gateway-mqtt-reconnect-test-XXXXXX";
    char spool_path[256];
    gateway_mqtt_sink_config config;
    gateway_mqtt_sink_snapshot snapshot;
    gateway_stats_snapshot stats_snapshot;
    gateway_mqtt_sink *sink = NULL;
    gateway_stats stats;
    gateway_logger logger;
    uint64_t sequence;
    uint16_t closed_port = 0;
    int reserved_socket;
    struct timespec consume_start;
    struct timespec consume_end;

    CHECK(mkdtemp(directory) != NULL);
    CHECK(snprintf(spool_path, sizeof(spool_path), "%s/spool.data",
                   directory) > 0);
    CHECK(gateway_stats_init(&stats) == GATEWAY_OK);
    CHECK(gateway_logger_init(&logger, stderr, GATEWAY_LOG_ERROR) ==
          GATEWAY_OK);
    (void)memset(&config, 0, sizeof(config));
    config.device_id = "unit-gateway";
    config.broker_host = "127.0.0.1";
    /* TCP握手成功但不返回CONNACK，模拟网络连接阶段静默。 */
    reserved_socket = create_silent_loopback_listener(&closed_port);
    CHECK(reserved_socket >= 0);
    config.broker_port = closed_port;
    config.broker_username = "";
    config.broker_password = "";
    config.topic = "test/m8/continuous-reconnect";
    config.batch_interval_ms = 1000;
    config.ack_timeout_ms = 500;
    config.reconnect_interval_ms = 1;
    config.spool_path = spool_path;
    config.max_records = 100;
    config.stats = &stats;
    config.logger = &logger;
    CHECK(gateway_mqtt_sink_create(&sink, &config) == GATEWAY_OK);
    CHECK(gateway_mqtt_sink_connect(sink) == GATEWAY_OK);
    for (sequence = 1; sequence <= 4; sequence++) {
        telemetry_record record = make_record(sequence, (uint8_t)sequence);

        sleep_milliseconds(2);
        /* 不调用 poll，模拟持续 CAN 输入使 consumer 永远不 idle。 */
        CHECK(clock_gettime(CLOCK_MONOTONIC, &consume_start) == 0);
        CHECK(gateway_mqtt_sink_consume(sink, &record) == GATEWAY_OK);
        CHECK(clock_gettime(CLOCK_MONOTONIC, &consume_end) == 0);
        CHECK(elapsed_milliseconds(&consume_start, &consume_end) < 250.0);
    }
    gateway_mqtt_sink_read(sink, &snapshot);
    gateway_stats_read(&stats, &stats_snapshot);
    CHECK(!snapshot.connected);
    CHECK(snapshot.spool_pending_records == 4);
    CHECK(stats_snapshot.counters[GATEWAY_STAT_MQTT_CONNECT_ATTEMPTS] > 1);
    gateway_mqtt_sink_destroy(sink);
    CHECK(close(reserved_socket) == 0);
    gateway_logger_destroy(&logger);
    gateway_stats_destroy(&stats);
    CHECK(unlink(spool_path) == 0);
    CHECK(rmdir(directory) == 0);
    return 0;
}

static int test_v2_durable_offline_reopen(void)
{
    char parent[] = "/tmp/gateway-mqtt-v2-test-XXXXXX";
    char spool_path[256];
    char file_path[512];
    gateway_mqtt_sink_config config;
    gateway_mqtt_sink_snapshot snapshot;
    gateway_mqtt_sink *sink = NULL;
    gateway_stats stats;
    gateway_logger logger;
    uint64_t sequence;

    CHECK(mkdtemp(parent) != NULL);
    CHECK(snprintf(spool_path, sizeof(spool_path), "%s/spool-v2", parent) > 0);
    CHECK(gateway_stats_init(&stats) == GATEWAY_OK);
    CHECK(gateway_logger_init(&logger, stderr, GATEWAY_LOG_ERROR) ==
          GATEWAY_OK);
    (void)memset(&config, 0, sizeof(config));
    config.device_id = "unit-gateway";
    config.broker_host = "127.0.0.1";
    config.broker_port = 1883;
    config.broker_username = "";
    config.broker_password = "";
    config.topic = "test/m10/v2-offline";
    config.batch_interval_ms = 1000;
    config.ack_timeout_ms = 1000;
    config.reconnect_interval_ms = 100;
    config.spool_path = spool_path;
    config.spool_format = GATEWAY_SPOOL_FORMAT_V2;
    config.spool_max_bytes = GATEWAY_SPOOL_MAX_BYTES_DEFAULT;
    config.max_records = 4;
    config.stats = &stats;
    config.logger = &logger;
    CHECK(gateway_mqtt_sink_create(&sink, &config) == GATEWAY_OK);
    for (sequence = 1; sequence <= 3; sequence++) {
        telemetry_record record = make_record(sequence, (uint8_t)sequence);

        CHECK(gateway_mqtt_sink_consume(sink, &record) == GATEWAY_OK);
    }
    gateway_mqtt_sink_read(sink, &snapshot);
    CHECK(snapshot.spool_v2);
    CHECK(snapshot.spool_pending_records == 3);
    CHECK(snapshot.spool_physical_bytes ==
          3 * GATEWAY_SPOOL_ENTRY_SIZE);
    CHECK(snapshot.spool_segment_count == 1);
    gateway_mqtt_sink_destroy(sink);
    sink = NULL;
    CHECK(gateway_mqtt_sink_create(&sink, &config) == GATEWAY_OK);
    gateway_mqtt_sink_read(sink, &snapshot);
    CHECK(snapshot.spool_v2 && snapshot.spool_pending_records == 3);
    CHECK(gateway_mqtt_sink_next_gateway_seq(sink) == 4);
    gateway_mqtt_sink_destroy(sink);
    gateway_logger_destroy(&logger);
    gateway_stats_destroy(&stats);
    CHECK(snprintf(file_path, sizeof(file_path),
                   "%s/segment-00000000000000000001.gsp2", spool_path) > 0);
    CHECK(unlink(file_path) == 0);
    CHECK(snprintf(file_path, sizeof(file_path), "%s/state.v2", spool_path) >
          0);
    CHECK(unlink(file_path) == 0);
    CHECK(snprintf(file_path, sizeof(file_path), "%s/lock", spool_path) > 0);
    CHECK(unlink(file_path) == 0);
    CHECK(rmdir(spool_path) == 0);
    CHECK(rmdir(parent) == 0);
    return 0;
}

int main(void)
{
    CHECK(test_batch_encoding() == 0);
    CHECK(test_library_and_create_validation() == 0);
    CHECK(test_durable_offline_append_and_sequence_recovery() == 0);
    CHECK(test_v2_durable_offline_reopen() == 0);
    CHECK(test_durable_reconnect_during_continuous_consume() == 0);
    return 0;
}
