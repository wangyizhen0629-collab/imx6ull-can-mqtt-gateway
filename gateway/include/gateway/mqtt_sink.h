#ifndef GATEWAY_MQTT_SINK_H
#define GATEWAY_MQTT_SINK_H

#include "gateway/error.h"
#include "gateway/log.h"
#include "gateway/stats.h"
#include "gateway/telemetry_record.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    GATEWAY_MQTT_BATCH_MAX_RECORDS = 256,
    GATEWAY_MQTT_PAYLOAD_CAPACITY = 131072
};

typedef struct {
    const char *device_id;
    const char *broker_host;
    uint16_t broker_port;
    const char *broker_username;
    const char *broker_password;
    const char *topic;
    uint32_t batch_interval_ms;
    uint32_t ack_timeout_ms;
    size_t max_records;
    gateway_stats *stats;
    gateway_logger *logger;
} gateway_mqtt_sink_config;

typedef struct {
    bool connected;
    bool failed;
    bool in_flight;
    int in_flight_mid;
    size_t buffered_records;
    uint64_t next_batch_seq;
    uint64_t last_acked_batch_seq;
    uint64_t last_acked_gateway_seq;
    uint64_t batches_acked;
    uint64_t records_acked;
    uint64_t puback_matched;
    uint64_t puback_unexpected;
} gateway_mqtt_sink_snapshot;

typedef struct gateway_mqtt_sink gateway_mqtt_sink;

gateway_error_code gateway_mqtt_sink_create(
    gateway_mqtt_sink **sink,
    const gateway_mqtt_sink_config *config);
void gateway_mqtt_sink_destroy(gateway_mqtt_sink *sink);
gateway_error_code gateway_mqtt_sink_connect(gateway_mqtt_sink *sink);
gateway_error_code gateway_mqtt_sink_consume(
    void *context,
    const telemetry_record *record);
gateway_error_code gateway_mqtt_sink_poll(void *context);
gateway_error_code gateway_mqtt_sink_flush(gateway_mqtt_sink *sink);
gateway_error_code gateway_mqtt_sink_disconnect(gateway_mqtt_sink *sink);
void gateway_mqtt_sink_read(gateway_mqtt_sink *sink,
                            gateway_mqtt_sink_snapshot *snapshot);

gateway_error_code gateway_mqtt_encode_batch(
    char *payload,
    size_t payload_capacity,
    const char *device_id,
    uint64_t batch_seq,
    const telemetry_record *records,
    size_t record_count,
    size_t *payload_size);

void gateway_mqtt_library_version(int *major, int *minor, int *revision);

#endif
