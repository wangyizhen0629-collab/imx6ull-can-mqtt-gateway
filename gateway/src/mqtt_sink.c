#include "gateway/mqtt_sink.h"

#include "gateway/config.h"

#include <inttypes.h>
#include <limits.h>
#include <mosquitto.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

enum {
    MQTT_KEEPALIVE_SECONDS = 60,
    MQTT_LOOP_SLICE_MS = 50,
    MQTT_CLIENT_ID_SIZE = 128
};

typedef struct {
    char *data;
    size_t capacity;
    size_t length;
    bool failed;
} json_builder;

struct gateway_mqtt_sink {
    char device_id[64];
    char broker_host[256];
    uint16_t broker_port;
    char broker_username[128];
    char broker_password[256];
    char topic[256];
    uint32_t batch_interval_ms;
    uint32_t ack_timeout_ms;
    size_t max_records;
    gateway_stats *stats;
    gateway_logger *logger;
    struct mosquitto *mosq;
    telemetry_record *records;
    char *payload;
    size_t record_count;
    struct timespec batch_deadline;
    bool batch_deadline_set;
    bool library_initialized;
    bool connect_callback_seen;
    int connect_result;
    bool explicit_disconnect;
    bool have_last_gateway_seq;
    uint64_t last_gateway_seq;
    pthread_mutex_t snapshot_mutex;
    bool snapshot_mutex_initialized;
    gateway_mqtt_sink_snapshot snapshot;
};

static void builder_append(json_builder *builder, const char *format, ...)
{
    int written;
    size_t available;
    va_list arguments;

    if (builder->failed) {
        return;
    }
    available = builder->capacity - builder->length;
    va_start(arguments, format);
    written = vsnprintf(builder->data + builder->length, available, format,
                        arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= available) {
        builder->failed = true;
        return;
    }
    builder->length += (size_t)written;
}

static void builder_append_json_string(json_builder *builder, const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;

    builder_append(builder, "\"");
    while (!builder->failed && *cursor != '\0') {
        switch (*cursor) {
        case '"':
            builder_append(builder, "\\\"");
            break;
        case '\\':
            builder_append(builder, "\\\\");
            break;
        case '\b':
            builder_append(builder, "\\b");
            break;
        case '\f':
            builder_append(builder, "\\f");
            break;
        case '\n':
            builder_append(builder, "\\n");
            break;
        case '\r':
            builder_append(builder, "\\r");
            break;
        case '\t':
            builder_append(builder, "\\t");
            break;
        default:
            if (*cursor < 0x20U) {
                builder_append(builder, "\\u%04x", (unsigned int)*cursor);
            } else {
                builder_append(builder, "%c", (int)*cursor);
            }
            break;
        }
        cursor++;
    }
    builder_append(builder, "\"");
}

static void builder_append_hex(json_builder *builder,
                               const uint8_t *bytes,
                               size_t byte_count)
{
    size_t index;

    builder_append(builder, "\"");
    for (index = 0; index < byte_count; index++) {
        builder_append(builder, "%02x", (unsigned int)bytes[index]);
    }
    builder_append(builder, "\"");
}

gateway_error_code gateway_mqtt_encode_batch(
    char *payload,
    size_t payload_capacity,
    const char *device_id,
    uint64_t batch_seq,
    const telemetry_record *records,
    size_t record_count,
    size_t *payload_size)
{
    json_builder builder;
    size_t index;

    if (payload == NULL || payload_capacity == 0 || device_id == NULL ||
        device_id[0] == '\0' || batch_seq == 0 || records == NULL ||
        record_count == 0 || record_count > GATEWAY_MQTT_BATCH_MAX_RECORDS ||
        payload_size == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    for (index = 1; index < record_count; index++) {
        if (records[index].gateway_seq <= records[index - 1].gateway_seq) {
            return GATEWAY_ERROR_INVALID_VALUE;
        }
    }

    builder.data = payload;
    builder.capacity = payload_capacity;
    builder.length = 0;
    builder.failed = false;
    builder_append(&builder, "{\"schema\":\"gateway.telemetry.v1\","
                             "\"device_id\":");
    builder_append_json_string(&builder, device_id);
    builder_append(&builder,
                   ",\"batch_seq\":%" PRIu64
                   ",\"record_count\":%zu,\"first_seq\":%" PRIu64
                   ",\"last_seq\":%" PRIu64 ",\"records\":[",
                   batch_seq, record_count, records[0].gateway_seq,
                   records[record_count - 1].gateway_seq);
    for (index = 0; index < record_count; index++) {
        const telemetry_record *record = &records[index];

        if (index != 0) {
            builder_append(&builder, ",");
        }
        builder_append(&builder,
                       "{\"seq\":%" PRIu64
                       ",\"timestamp_ns\":%" PRId64
                       ",\"can_id\":%" PRIu32
                       ",\"dlc\":%u,\"data\":",
                       record->gateway_seq, record->kernel_timestamp_ns,
                       record->can_id, (unsigned int)record->dlc);
        builder_append_hex(&builder, record->data, GATEWAY_CAN_DATA_SIZE);
        builder_append(&builder,
                       ",\"ecu_counter\":%u,\"status_flags\":%u,"
                       "\"decoded_payload\":",
                       (unsigned int)record->ecu_counter,
                       (unsigned int)record->status_flags);
        builder_append_hex(&builder, record->decoded_payload,
                           GATEWAY_DECODED_PAYLOAD_SIZE);
        builder_append(&builder, "}");
    }
    builder_append(&builder, "]}");
    if (builder.failed) {
        return GATEWAY_ERROR_RANGE;
    }
    *payload_size = builder.length;
    return GATEWAY_OK;
}

static bool timespec_reached(const struct timespec *deadline)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return false;
    }
    return now.tv_sec > deadline->tv_sec ||
           (now.tv_sec == deadline->tv_sec &&
            now.tv_nsec >= deadline->tv_nsec);
}

static bool deadline_after_milliseconds(uint32_t timeout_ms,
                                        struct timespec *deadline)
{
    if (clock_gettime(CLOCK_MONOTONIC, deadline) != 0) {
        return false;
    }
    deadline->tv_sec += (time_t)(timeout_ms / 1000U);
    deadline->tv_nsec += (long)(timeout_ms % 1000U) * 1000000L;
    if (deadline->tv_nsec >= 1000000000L) {
        deadline->tv_sec++;
        deadline->tv_nsec -= 1000000000L;
    }
    return true;
}

static int milliseconds_until(const struct timespec *deadline)
{
    struct timespec now;
    int64_t milliseconds;
    int64_t nanoseconds;
    int64_t seconds;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return -1;
    }
    seconds = (int64_t)deadline->tv_sec - (int64_t)now.tv_sec;
    nanoseconds = (int64_t)deadline->tv_nsec - (int64_t)now.tv_nsec;
    if (nanoseconds < 0) {
        seconds--;
        nanoseconds += INT64_C(1000000000);
    }
    if (seconds < 0 || (seconds == 0 && nanoseconds == 0)) {
        return 0;
    }
    milliseconds = seconds * INT64_C(1000) +
                   (nanoseconds + INT64_C(999999)) / INT64_C(1000000);
    return milliseconds > INT_MAX ? INT_MAX : (int)milliseconds;
}

static void snapshot_set_failed(gateway_mqtt_sink *sink)
{
    (void)pthread_mutex_lock(&sink->snapshot_mutex);
    sink->snapshot.failed = true;
    (void)pthread_mutex_unlock(&sink->snapshot_mutex);
    gateway_stats_increment(sink->stats, GATEWAY_STAT_MQTT_ERRORS);
}

static gateway_error_code mqtt_failure(gateway_mqtt_sink *sink,
                                       gateway_error_code code,
                                       int mosquitto_code,
                                       const char *operation)
{
    snapshot_set_failed(sink);
    if (mosquitto_code == MOSQ_ERR_SUCCESS) {
        gateway_log(sink->logger, GATEWAY_LOG_ERROR, "mqtt", "%s failed: %s",
                    operation, gateway_error_string(code));
    } else {
        gateway_log(sink->logger, GATEWAY_LOG_ERROR, "mqtt", "%s failed: %s",
                    operation, mosquitto_strerror(mosquitto_code));
    }
    return code;
}

static void on_connect(struct mosquitto *mosq, void *context, int result)
{
    gateway_mqtt_sink *sink = context;

    (void)mosq;
    sink->connect_callback_seen = true;
    sink->connect_result = result;
    (void)pthread_mutex_lock(&sink->snapshot_mutex);
    sink->snapshot.connected = result == 0;
    (void)pthread_mutex_unlock(&sink->snapshot_mutex);
}

static void on_disconnect(struct mosquitto *mosq, void *context, int result)
{
    gateway_mqtt_sink *sink = context;

    (void)mosq;
    (void)pthread_mutex_lock(&sink->snapshot_mutex);
    sink->snapshot.connected = false;
    (void)pthread_mutex_unlock(&sink->snapshot_mutex);
    if (!sink->explicit_disconnect && result != 0) {
        snapshot_set_failed(sink);
    }
}

static void on_publish(struct mosquitto *mosq, void *context, int mid)
{
    gateway_mqtt_sink *sink = context;

    (void)mosq;
    (void)pthread_mutex_lock(&sink->snapshot_mutex);
    if (sink->snapshot.in_flight && sink->snapshot.in_flight_mid == mid) {
        sink->snapshot.in_flight = false;
        sink->snapshot.puback_matched++;
        gateway_stats_increment(sink->stats,
                                GATEWAY_STAT_MQTT_PUBACK_MATCHED);
    } else {
        sink->snapshot.puback_unexpected++;
        gateway_stats_increment(sink->stats,
                                GATEWAY_STAT_MQTT_PUBACK_UNEXPECTED);
    }
    (void)pthread_mutex_unlock(&sink->snapshot_mutex);
}

static bool required_record_status_valid(const telemetry_record *record)
{
    const uint16_t required =
        GATEWAY_RECORD_STATUS_KERNEL_TIMESTAMP_VALID |
        GATEWAY_RECORD_STATUS_CHECKSUM_VALID |
        GATEWAY_RECORD_STATUS_DECODED_VALID;

    return record->gateway_seq != 0 && record->dlc == GATEWAY_CAN_DATA_SIZE &&
           (record->status_flags & required) == required;
}

gateway_error_code gateway_mqtt_sink_create(
    gateway_mqtt_sink **sink,
    const gateway_mqtt_sink_config *config)
{
    gateway_mqtt_sink *created;
    char client_id[MQTT_CLIENT_ID_SIZE];
    int code;

    if (sink == NULL || config == NULL || config->device_id == NULL ||
        config->device_id[0] == '\0' || config->broker_host == NULL ||
        config->broker_host[0] == '\0' || config->broker_port == 0 ||
        config->broker_username == NULL || config->broker_password == NULL ||
        config->topic == NULL || config->topic[0] == '\0' ||
        config->batch_interval_ms == 0 || config->ack_timeout_ms == 0 ||
        config->max_records == 0 ||
        config->max_records > GATEWAY_MQTT_BATCH_MAX_RECORDS ||
        config->stats == NULL || config->logger == NULL ||
        strlen(config->device_id) >= GATEWAY_DEVICE_ID_SIZE ||
        strlen(config->broker_host) >= GATEWAY_BROKER_HOST_SIZE ||
        strlen(config->broker_username) >= GATEWAY_BROKER_USERNAME_SIZE ||
        strlen(config->broker_password) >= GATEWAY_BROKER_PASSWORD_SIZE ||
        strlen(config->topic) >= GATEWAY_MQTT_TOPIC_SIZE ||
        (config->broker_username[0] == '\0' &&
         config->broker_password[0] != '\0')) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    *sink = NULL;
    created = calloc(1, sizeof(*created));
    if (created == NULL) {
        return GATEWAY_ERROR_SYSTEM;
    }
    created->records = calloc(config->max_records, sizeof(*created->records));
    created->payload = calloc(GATEWAY_MQTT_PAYLOAD_CAPACITY, 1);
    if (created->records == NULL || created->payload == NULL) {
        free(created->payload);
        free(created->records);
        free(created);
        return GATEWAY_ERROR_SYSTEM;
    }
    if (pthread_mutex_init(&created->snapshot_mutex, NULL) != 0) {
        free(created->payload);
        free(created->records);
        free(created);
        return GATEWAY_ERROR_SYSTEM;
    }
    created->snapshot_mutex_initialized = true;
    (void)snprintf(created->device_id, sizeof(created->device_id), "%s",
                   config->device_id);
    (void)snprintf(created->broker_host, sizeof(created->broker_host), "%s",
                   config->broker_host);
    (void)snprintf(created->broker_username,
                   sizeof(created->broker_username), "%s",
                   config->broker_username);
    (void)snprintf(created->broker_password,
                   sizeof(created->broker_password), "%s",
                   config->broker_password);
    (void)snprintf(created->topic, sizeof(created->topic), "%s",
                   config->topic);
    created->broker_port = config->broker_port;
    created->batch_interval_ms = config->batch_interval_ms;
    created->ack_timeout_ms = config->ack_timeout_ms;
    created->max_records = config->max_records;
    created->stats = config->stats;
    created->logger = config->logger;
    created->snapshot.next_batch_seq = 1;
    created->snapshot.in_flight_mid = -1;

    code = mosquitto_lib_init();
    if (code != MOSQ_ERR_SUCCESS) {
        gateway_mqtt_sink_destroy(created);
        return GATEWAY_ERROR_SYSTEM;
    }
    created->library_initialized = true;
    (void)snprintf(client_id, sizeof(client_id), "gatewayd-%s-%ld",
                   created->device_id, (long)getpid());
    created->mosq = mosquitto_new(client_id, true, created);
    if (created->mosq == NULL) {
        gateway_mqtt_sink_destroy(created);
        return GATEWAY_ERROR_SYSTEM;
    }
    mosquitto_connect_callback_set(created->mosq, on_connect);
    mosquitto_disconnect_callback_set(created->mosq, on_disconnect);
    mosquitto_publish_callback_set(created->mosq, on_publish);
    code = mosquitto_max_inflight_messages_set(created->mosq, 1);
    if (code != MOSQ_ERR_SUCCESS) {
        gateway_mqtt_sink_destroy(created);
        return GATEWAY_ERROR_SYSTEM;
    }
    if (created->broker_username[0] != '\0') {
        code = mosquitto_username_pw_set(created->mosq,
                                         created->broker_username,
                                         created->broker_password);
        if (code != MOSQ_ERR_SUCCESS) {
            gateway_mqtt_sink_destroy(created);
            return GATEWAY_ERROR_INVALID_VALUE;
        }
    }
    *sink = created;
    return GATEWAY_OK;
}

void gateway_mqtt_sink_destroy(gateway_mqtt_sink *sink)
{
    if (sink == NULL) {
        return;
    }
    if (sink->mosq != NULL) {
        mosquitto_destroy(sink->mosq);
    }
    if (sink->library_initialized) {
        (void)mosquitto_lib_cleanup();
    }
    if (sink->snapshot_mutex_initialized) {
        (void)pthread_mutex_destroy(&sink->snapshot_mutex);
    }
    free(sink->payload);
    free(sink->records);
    (void)memset(sink->broker_password, 0, sizeof(sink->broker_password));
    free(sink);
}

gateway_error_code gateway_mqtt_sink_connect(gateway_mqtt_sink *sink)
{
    struct timespec deadline;
    int code;

    if (sink == NULL || sink->mosq == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    gateway_stats_increment(sink->stats,
                            GATEWAY_STAT_MQTT_CONNECT_ATTEMPTS);
    sink->connect_callback_seen = false;
    code = mosquitto_connect(sink->mosq, sink->broker_host,
                             (int)sink->broker_port, MQTT_KEEPALIVE_SECONDS);
    if (code != MOSQ_ERR_SUCCESS) {
        return mqtt_failure(sink, GATEWAY_ERROR_IO, code, "connect");
    }
    if (!deadline_after_milliseconds(sink->ack_timeout_ms, &deadline)) {
        return mqtt_failure(sink, GATEWAY_ERROR_SYSTEM, MOSQ_ERR_SUCCESS,
                            "connect deadline");
    }
    while (!sink->connect_callback_seen) {
        int remaining = milliseconds_until(&deadline);
        int slice;

        if (remaining <= 0) {
            return mqtt_failure(sink, GATEWAY_ERROR_TIMEOUT,
                                MOSQ_ERR_SUCCESS, "CONNACK wait");
        }
        slice = remaining < MQTT_LOOP_SLICE_MS ? remaining
                                               : MQTT_LOOP_SLICE_MS;
        code = mosquitto_loop(sink->mosq, slice, 1);
        if (code != MOSQ_ERR_SUCCESS) {
            return mqtt_failure(sink, GATEWAY_ERROR_IO, code,
                                "CONNACK network loop");
        }
    }
    if (sink->connect_result != 0) {
        return mqtt_failure(sink, GATEWAY_ERROR_IO, MOSQ_ERR_SUCCESS,
                            "CONNACK rejected");
    }
    gateway_stats_increment(sink->stats, GATEWAY_STAT_MQTT_CONNECT_SUCCESS);
    gateway_log(sink->logger, GATEWAY_LOG_INFO, "mqtt",
                "connected broker=%s port=%u topic=%s qos=1 max_inflight=1",
                sink->broker_host, (unsigned int)sink->broker_port,
                sink->topic);
    return GATEWAY_OK;
}

gateway_error_code gateway_mqtt_sink_flush(gateway_mqtt_sink *sink)
{
    struct timespec deadline;
    size_t payload_size;
    size_t acknowledged_records;
    uint64_t acknowledged_last_seq;
    uint64_t batch_seq;
    int mid = -1;
    int code;

    if (sink == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    if (sink->record_count == 0) {
        return GATEWAY_OK;
    }
    (void)pthread_mutex_lock(&sink->snapshot_mutex);
    if (!sink->snapshot.connected || sink->snapshot.failed ||
        sink->snapshot.in_flight) {
        (void)pthread_mutex_unlock(&sink->snapshot_mutex);
        return GATEWAY_ERROR_CLOSED;
    }
    batch_seq = sink->snapshot.next_batch_seq;
    (void)pthread_mutex_unlock(&sink->snapshot_mutex);

    code = gateway_mqtt_encode_batch(
        sink->payload, GATEWAY_MQTT_PAYLOAD_CAPACITY, sink->device_id,
        batch_seq, sink->records, sink->record_count, &payload_size);
    if (code != GATEWAY_OK) {
        return mqtt_failure(sink, code, MOSQ_ERR_SUCCESS, "batch encode");
    }
    if (payload_size > INT_MAX) {
        return mqtt_failure(sink, GATEWAY_ERROR_RANGE, MOSQ_ERR_SUCCESS,
                            "payload length");
    }

    gateway_stats_increment(sink->stats,
                            GATEWAY_STAT_MQTT_PUBLISH_ATTEMPTS);
    code = mosquitto_publish(sink->mosq, &mid, sink->topic,
                             (int)payload_size, sink->payload, 1, false);
    if (code != MOSQ_ERR_SUCCESS) {
        return mqtt_failure(sink, GATEWAY_ERROR_IO, code, "publish");
    }
    (void)pthread_mutex_lock(&sink->snapshot_mutex);
    sink->snapshot.in_flight = true;
    sink->snapshot.in_flight_mid = mid;
    (void)pthread_mutex_unlock(&sink->snapshot_mutex);
    gateway_stats_increment(sink->stats,
                            GATEWAY_STAT_MQTT_PUBLISH_ACCEPTED);

    if (!deadline_after_milliseconds(sink->ack_timeout_ms, &deadline)) {
        return mqtt_failure(sink, GATEWAY_ERROR_SYSTEM, MOSQ_ERR_SUCCESS,
                            "PUBACK deadline");
    }
    for (;;) {
        bool in_flight;
        int remaining;
        int slice;

        (void)pthread_mutex_lock(&sink->snapshot_mutex);
        in_flight = sink->snapshot.in_flight;
        (void)pthread_mutex_unlock(&sink->snapshot_mutex);
        if (!in_flight) {
            break;
        }
        remaining = milliseconds_until(&deadline);
        if (remaining <= 0) {
            return mqtt_failure(sink, GATEWAY_ERROR_TIMEOUT,
                                MOSQ_ERR_SUCCESS, "PUBACK wait");
        }
        slice = remaining < MQTT_LOOP_SLICE_MS ? remaining
                                               : MQTT_LOOP_SLICE_MS;
        code = mosquitto_loop(sink->mosq, slice, 1);
        if (code != MOSQ_ERR_SUCCESS) {
            return mqtt_failure(sink, GATEWAY_ERROR_IO, code,
                                "PUBACK network loop");
        }
    }

    acknowledged_records = sink->record_count;
    acknowledged_last_seq = sink->records[sink->record_count - 1].gateway_seq;
    sink->record_count = 0;
    sink->batch_deadline_set = false;
    gateway_stats_add(sink->stats, GATEWAY_STAT_MQTT_RECORDS_ACKED,
                      (uint64_t)acknowledged_records);
    (void)pthread_mutex_lock(&sink->snapshot_mutex);
    sink->snapshot.buffered_records = 0;
    sink->snapshot.batches_acked++;
    sink->snapshot.records_acked += (uint64_t)acknowledged_records;
    sink->snapshot.last_acked_batch_seq = batch_seq;
    sink->snapshot.last_acked_gateway_seq = acknowledged_last_seq;
    sink->snapshot.next_batch_seq++;
    (void)pthread_mutex_unlock(&sink->snapshot_mutex);
    return GATEWAY_OK;
}

gateway_error_code gateway_mqtt_sink_consume(
    void *context,
    const telemetry_record *record)
{
    gateway_mqtt_sink *sink = context;
    gateway_error_code code;
    bool ready;

    if (sink == NULL || record == NULL || !required_record_status_valid(record)) {
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    (void)pthread_mutex_lock(&sink->snapshot_mutex);
    ready = sink->snapshot.connected && !sink->snapshot.failed &&
            !sink->snapshot.in_flight;
    (void)pthread_mutex_unlock(&sink->snapshot_mutex);
    if (!ready) {
        return GATEWAY_ERROR_CLOSED;
    }
    if (sink->have_last_gateway_seq &&
        record->gateway_seq <= sink->last_gateway_seq) {
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    if (sink->record_count > 0 && sink->batch_deadline_set &&
        timespec_reached(&sink->batch_deadline)) {
        code = gateway_mqtt_sink_flush(sink);
        if (code != GATEWAY_OK) {
            return code;
        }
    }
    if (sink->record_count >= sink->max_records) {
        return GATEWAY_ERROR_RANGE;
    }
    sink->records[sink->record_count++] = *record;
    sink->have_last_gateway_seq = true;
    sink->last_gateway_seq = record->gateway_seq;
    if (sink->record_count == 1) {
        if (!deadline_after_milliseconds(sink->batch_interval_ms,
                                         &sink->batch_deadline)) {
            return mqtt_failure(sink, GATEWAY_ERROR_SYSTEM,
                                MOSQ_ERR_SUCCESS, "batch deadline");
        }
        sink->batch_deadline_set = true;
    }
    (void)pthread_mutex_lock(&sink->snapshot_mutex);
    sink->snapshot.buffered_records = sink->record_count;
    (void)pthread_mutex_unlock(&sink->snapshot_mutex);
    if (sink->record_count == sink->max_records) {
        return gateway_mqtt_sink_flush(sink);
    }
    return GATEWAY_OK;
}

gateway_error_code gateway_mqtt_sink_poll(void *context)
{
    gateway_mqtt_sink *sink = context;
    int code;

    if (sink == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    code = mosquitto_loop(sink->mosq, 0, 1);
    if (code != MOSQ_ERR_SUCCESS) {
        return mqtt_failure(sink, GATEWAY_ERROR_IO, code, "network poll");
    }
    if (sink->record_count > 0 && sink->batch_deadline_set &&
        timespec_reached(&sink->batch_deadline)) {
        return gateway_mqtt_sink_flush(sink);
    }
    return GATEWAY_OK;
}

gateway_error_code gateway_mqtt_sink_disconnect(gateway_mqtt_sink *sink)
{
    int code;

    if (sink == NULL || sink->mosq == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    sink->explicit_disconnect = true;
    code = mosquitto_disconnect(sink->mosq);
    if (code == MOSQ_ERR_NO_CONN) {
        return GATEWAY_OK;
    }
    if (code != MOSQ_ERR_SUCCESS) {
        return mqtt_failure(sink, GATEWAY_ERROR_IO, code, "disconnect");
    }
    code = mosquitto_loop(sink->mosq, MQTT_LOOP_SLICE_MS, 1);
    if (code != MOSQ_ERR_SUCCESS && code != MOSQ_ERR_NO_CONN) {
        return mqtt_failure(sink, GATEWAY_ERROR_IO, code,
                            "disconnect network loop");
    }
    return GATEWAY_OK;
}

void gateway_mqtt_sink_read(gateway_mqtt_sink *sink,
                            gateway_mqtt_sink_snapshot *snapshot)
{
    if (sink == NULL || snapshot == NULL) {
        return;
    }
    (void)pthread_mutex_lock(&sink->snapshot_mutex);
    *snapshot = sink->snapshot;
    (void)pthread_mutex_unlock(&sink->snapshot_mutex);
}

void gateway_mqtt_library_version(int *major, int *minor, int *revision)
{
    (void)mosquitto_lib_version(major, minor, revision);
}
