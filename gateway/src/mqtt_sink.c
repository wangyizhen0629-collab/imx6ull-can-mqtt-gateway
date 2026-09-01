#include "gateway/mqtt_sink.h"

#include "gateway/config.h"
#include "gateway/mqtt_reactor.h"
#include "gateway/spool.h"

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
    MQTT_MISC_INTERVAL_MS = 1000,
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
    uint32_t reconnect_interval_ms;
    size_t max_records;
    gateway_stats *stats;
    gateway_logger *logger;
    struct mosquitto *mosq;
    gateway_mqtt_reactor *reactor;
    gateway_spool *spool;
    telemetry_record *records;
    char *payload;
    size_t record_count;
    struct timespec batch_deadline;
    bool batch_deadline_set;
    bool library_initialized;
    bool connect_callback_seen;
    int connect_result;
    bool explicit_disconnect;
    bool durable;
    bool ever_connected;
    bool transport_lost;
    bool reconnect_deadline_set;
    bool reconnect_in_progress;
    bool reconnect_attempt_deadline_set;
    bool replay_pending;
    struct timespec reconnect_deadline;
    struct timespec reconnect_attempt_deadline;
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

static void snapshot_mark_failed(gateway_mqtt_sink *sink)
{
    (void)pthread_mutex_lock(&sink->snapshot_mutex);
    sink->snapshot.failed = true;
    (void)pthread_mutex_unlock(&sink->snapshot_mutex);
}

static void snapshot_set_mqtt_failed(gateway_mqtt_sink *sink)
{
    snapshot_mark_failed(sink);
    gateway_stats_increment(sink->stats, GATEWAY_STAT_MQTT_ERRORS);
}

static void sync_spool_snapshot(gateway_mqtt_sink *sink)
{
    gateway_spool_snapshot spool_snapshot;

    if (!sink->durable) {
        return;
    }
    gateway_spool_read(sink->spool, &spool_snapshot);
    (void)pthread_mutex_lock(&sink->snapshot_mutex);
    sink->snapshot.durable = true;
    sink->snapshot.buffered_records =
        spool_snapshot.pending_records > SIZE_MAX
            ? SIZE_MAX
            : (size_t)spool_snapshot.pending_records;
    sink->snapshot.spool_total_records = spool_snapshot.total_records;
    sink->snapshot.spool_pending_records = spool_snapshot.pending_records;
    sink->snapshot.spool_records_appended = spool_snapshot.records_appended;
    sink->snapshot.spool_records_replayed = spool_snapshot.records_replayed;
    sink->snapshot.spool_tail_recoveries = spool_snapshot.tail_recoveries;
    sink->snapshot.spool_state_recoveries = spool_snapshot.state_recoveries;
    sink->snapshot.spool_corruptions = spool_snapshot.corruptions;
    sink->snapshot.next_batch_seq = spool_snapshot.next_batch_seq;
    sink->snapshot.last_acked_gateway_seq =
        spool_snapshot.last_acked_gateway_seq;
    (void)pthread_mutex_unlock(&sink->snapshot_mutex);
}

static void sync_reactor_snapshot(gateway_mqtt_sink *sink)
{
    gateway_mqtt_reactor_snapshot reactor_snapshot;

    if (sink->reactor == NULL) {
        return;
    }
    gateway_mqtt_reactor_read(sink->reactor, &reactor_snapshot);
    (void)pthread_mutex_lock(&sink->snapshot_mutex);
    sink->snapshot.reactor_enabled = reactor_snapshot.enabled;
    sink->snapshot.reactor_network_fd = reactor_snapshot.network_fd;
    sink->snapshot.reactor_epoll_waits = reactor_snapshot.epoll_waits;
    sink->snapshot.reactor_wake_events = reactor_snapshot.wake_events;
    sink->snapshot.reactor_timer_expirations =
        reactor_snapshot.timer_expirations;
    sink->snapshot.reactor_socket_events = reactor_snapshot.socket_events;
    sink->snapshot.reactor_loop_read_calls =
        reactor_snapshot.loop_read_calls;
    sink->snapshot.reactor_loop_write_calls =
        reactor_snapshot.loop_write_calls;
    sink->snapshot.reactor_loop_misc_calls =
        reactor_snapshot.loop_misc_calls;
    (void)pthread_mutex_unlock(&sink->snapshot_mutex);
}

static gateway_error_code reactor_step(gateway_mqtt_sink *sink,
                                       int timeout_ms,
                                       int *mosquitto_code)
{
    gateway_error_code code = gateway_mqtt_reactor_step(
        sink->reactor, timeout_ms, mosquitto_code);

    sync_reactor_snapshot(sink);
    return code;
}

static gateway_error_code spool_failure(gateway_mqtt_sink *sink,
                                        gateway_error_code code,
                                        const char *operation)
{
    snapshot_mark_failed(sink);
    gateway_stats_increment(sink->stats, GATEWAY_STAT_SPOOL_ERRORS);
    gateway_log(sink->logger, GATEWAY_LOG_ERROR, "spool", "%s failed: %s",
                operation, gateway_error_string(code));
    return code;
}

static gateway_error_code mqtt_failure(gateway_mqtt_sink *sink,
                                       gateway_error_code code,
                                       int mosquitto_code,
                                       const char *operation)
{
    snapshot_set_mqtt_failed(sink);
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
        if (sink->durable) {
            sink->transport_lost = true;
        } else {
            snapshot_set_mqtt_failed(sink);
        }
    }
}

static void on_publish(struct mosquitto *mosq, void *context, int mid);

static gateway_error_code create_mosquitto_client(gateway_mqtt_sink *sink)
{
    char client_id[MQTT_CLIENT_ID_SIZE];
    int code;

    (void)snprintf(client_id, sizeof(client_id), "gatewayd-%s-%ld",
                   sink->device_id, (long)getpid());
    sink->mosq = mosquitto_new(client_id, true, sink);
    if (sink->mosq == NULL) {
        return GATEWAY_ERROR_SYSTEM;
    }
    mosquitto_connect_callback_set(sink->mosq, on_connect);
    mosquitto_disconnect_callback_set(sink->mosq, on_disconnect);
    mosquitto_publish_callback_set(sink->mosq, on_publish);
    code = mosquitto_max_inflight_messages_set(sink->mosq, 1);
    if (code != MOSQ_ERR_SUCCESS) {
        mosquitto_destroy(sink->mosq);
        sink->mosq = NULL;
        return GATEWAY_ERROR_SYSTEM;
    }
    if (sink->broker_username[0] != '\0') {
        code = mosquitto_username_pw_set(sink->mosq, sink->broker_username,
                                         sink->broker_password);
        if (code != MOSQ_ERR_SUCCESS) {
            mosquitto_destroy(sink->mosq);
            sink->mosq = NULL;
            return GATEWAY_ERROR_INVALID_VALUE;
        }
    }
    return GATEWAY_OK;
}

static gateway_error_code reset_mosquitto_client(gateway_mqtt_sink *sink)
{
    gateway_error_code code;

    code = gateway_mqtt_reactor_bind(sink->reactor, NULL);
    if (code != GATEWAY_OK) {
        return code;
    }
    if (sink->mosq != NULL) {
        mosquitto_destroy(sink->mosq);
        sink->mosq = NULL;
    }
    sink->transport_lost = false;
    sink->connect_callback_seen = false;
    sink->explicit_disconnect = false;
    sink->reconnect_in_progress = false;
    sink->reconnect_attempt_deadline_set = false;
    code = create_mosquitto_client(sink);
    if (code != GATEWAY_OK) {
        return code;
    }
    return gateway_mqtt_reactor_bind(sink->reactor, sink->mosq);
}

static gateway_error_code durable_transport_offline(
    gateway_mqtt_sink *sink,
    int mosquitto_code,
    const char *operation)
{
    gateway_error_code code;

    (void)pthread_mutex_lock(&sink->snapshot_mutex);
    sink->snapshot.connected = false;
    sink->snapshot.in_flight = false;
    sink->snapshot.in_flight_mid = -1;
    (void)pthread_mutex_unlock(&sink->snapshot_mutex);
    gateway_spool_cancel_prepared(sink->spool);
    gateway_stats_increment(sink->stats, GATEWAY_STAT_MQTT_ERRORS);
    gateway_log(sink->logger, GATEWAY_LOG_WARN, "mqtt",
                "%s: offline, durable records retained: %s", operation,
                mosquitto_code == MOSQ_ERR_SUCCESS
                    ? "timeout"
                    : mosquitto_strerror(mosquitto_code));
    code = reset_mosquitto_client(sink);
    if (code != GATEWAY_OK) {
        return mqtt_failure(sink, code, MOSQ_ERR_SUCCESS,
                            "MQTT client reset");
    }
    if (!deadline_after_milliseconds(sink->reconnect_interval_ms,
                                     &sink->reconnect_deadline)) {
        return mqtt_failure(sink, GATEWAY_ERROR_SYSTEM, MOSQ_ERR_SUCCESS,
                            "reconnect deadline");
    }
    sink->reconnect_deadline_set = true;
    sync_spool_snapshot(sink);
    return GATEWAY_OK;
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
    gateway_spool_snapshot spool_snapshot;
    gateway_error_code spool_code;
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
        (config->spool_path != NULL && config->spool_path[0] != '\0' &&
         strlen(config->spool_path) >= GATEWAY_SPOOL_PATH_SIZE) ||
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
    created->reconnect_interval_ms = config->reconnect_interval_ms == 0
                                         ? 1000U
                                         : config->reconnect_interval_ms;
    created->max_records = config->max_records;
    created->stats = config->stats;
    created->logger = config->logger;
    created->snapshot.next_batch_seq = 1;
    created->snapshot.in_flight_mid = -1;
    created->snapshot.reactor_network_fd = -1;

    if (config->spool_path != NULL && config->spool_path[0] != '\0') {
        created->durable = true;
        spool_code = gateway_spool_open(&created->spool, config->spool_path);
        if (spool_code != GATEWAY_OK) {
            gateway_mqtt_sink_destroy(created);
            return spool_code;
        }
        gateway_spool_read(created->spool, &spool_snapshot);
        created->have_last_gateway_seq = spool_snapshot.last_gateway_seq != 0;
        created->last_gateway_seq = spool_snapshot.last_gateway_seq;
        created->replay_pending = spool_snapshot.pending_records != 0;
        gateway_stats_add(created->stats,
                          GATEWAY_STAT_SPOOL_TAIL_RECOVERIES,
                          spool_snapshot.tail_recoveries);
        gateway_stats_add(created->stats,
                          GATEWAY_STAT_SPOOL_STATE_RECOVERIES,
                          spool_snapshot.state_recoveries);
        sync_spool_snapshot(created);
    }

    code = mosquitto_lib_init();
    if (code != MOSQ_ERR_SUCCESS) {
        gateway_mqtt_sink_destroy(created);
        return GATEWAY_ERROR_SYSTEM;
    }
    created->library_initialized = true;
    if (create_mosquitto_client(created) != GATEWAY_OK) {
        gateway_mqtt_sink_destroy(created);
        return GATEWAY_ERROR_SYSTEM;
    }
    if (gateway_mqtt_reactor_create(&created->reactor, created->mosq,
                                    MQTT_MISC_INTERVAL_MS) != GATEWAY_OK) {
        gateway_mqtt_sink_destroy(created);
        return GATEWAY_ERROR_SYSTEM;
    }
    sync_reactor_snapshot(created);
    *sink = created;
    return GATEWAY_OK;
}

void gateway_mqtt_sink_destroy(gateway_mqtt_sink *sink)
{
    if (sink == NULL) {
        return;
    }
    if (sink->reactor != NULL) {
        (void)gateway_mqtt_reactor_bind(sink->reactor, NULL);
        gateway_mqtt_reactor_destroy(sink->reactor);
    }
    if (sink->mosq != NULL) {
        mosquitto_destroy(sink->mosq);
    }
    if (sink->library_initialized) {
        (void)mosquitto_lib_cleanup();
    }
    gateway_spool_close(sink->spool);
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
    gateway_error_code reactor_code;
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
        if (sink->durable) {
            return durable_transport_offline(sink, code, "connect");
        }
        return mqtt_failure(sink, GATEWAY_ERROR_IO, code, "connect");
    }
    if (gateway_mqtt_reactor_notify(sink->reactor) != GATEWAY_OK) {
        return mqtt_failure(sink, GATEWAY_ERROR_SYSTEM, MOSQ_ERR_SUCCESS,
                            "reactor connect notification");
    }
    if (!deadline_after_milliseconds(sink->ack_timeout_ms, &deadline)) {
        return mqtt_failure(sink, GATEWAY_ERROR_SYSTEM, MOSQ_ERR_SUCCESS,
                            "connect deadline");
    }
    while (!sink->connect_callback_seen) {
        int remaining = milliseconds_until(&deadline);
        int slice;

        if (remaining <= 0) {
            if (sink->durable) {
                return durable_transport_offline(sink, MOSQ_ERR_SUCCESS,
                                                 "CONNACK wait");
            }
            return mqtt_failure(sink, GATEWAY_ERROR_TIMEOUT,
                                MOSQ_ERR_SUCCESS, "CONNACK wait");
        }
        slice = remaining < MQTT_LOOP_SLICE_MS ? remaining
                                               : MQTT_LOOP_SLICE_MS;
        reactor_code = reactor_step(sink, slice, &code);
        if (reactor_code != GATEWAY_OK) {
            if (sink->durable) {
                return durable_transport_offline(sink, code,
                                                 "CONNACK external loop");
            }
            return mqtt_failure(sink, reactor_code, code,
                                "CONNACK external loop");
        }
    }
    if (sink->connect_result != 0) {
        return mqtt_failure(sink, GATEWAY_ERROR_IO, MOSQ_ERR_SUCCESS,
                            "CONNACK rejected");
    }
    gateway_stats_increment(sink->stats, GATEWAY_STAT_MQTT_CONNECT_SUCCESS);
    if (sink->ever_connected) {
        gateway_stats_increment(sink->stats, GATEWAY_STAT_MQTT_RECONNECTS);
    }
    sink->ever_connected = true;
    sink->reconnect_deadline_set = false;
    gateway_log(sink->logger, GATEWAY_LOG_INFO, "mqtt",
                "connected broker=%s port=%u topic=%s qos=1 max_inflight=1",
                sink->broker_host, (unsigned int)sink->broker_port,
                sink->topic);
    return GATEWAY_OK;
}

static gateway_error_code flush_durable(gateway_mqtt_sink *sink)
{
    struct timespec deadline;
    gateway_spool_snapshot spool_snapshot;
    size_t payload_size;
    size_t record_count = 0;
    uint64_t batch_seq = 0;
    uint64_t acknowledged_last_seq;
    bool connected;
    int mid = -1;
    int code;
    gateway_error_code gateway_code;

    gateway_spool_read(sink->spool, &spool_snapshot);
    if (spool_snapshot.pending_records == 0) {
        sink->replay_pending = false;
        sink->batch_deadline_set = false;
        sync_spool_snapshot(sink);
        return GATEWAY_OK;
    }
    (void)pthread_mutex_lock(&sink->snapshot_mutex);
    connected = sink->snapshot.connected && !sink->snapshot.failed &&
                !sink->snapshot.in_flight;
    (void)pthread_mutex_unlock(&sink->snapshot_mutex);
    if (!connected) {
        return GATEWAY_OK;
    }

    gateway_code = gateway_spool_prepare_batch(
        sink->spool, sink->records, sink->max_records, &record_count,
        &batch_seq);
    if (gateway_code != GATEWAY_OK) {
        return spool_failure(sink, gateway_code, "prepare batch");
    }
    if (record_count == 0) {
        sync_spool_snapshot(sink);
        return GATEWAY_OK;
    }
    gateway_stats_add(sink->stats, GATEWAY_STAT_SPOOL_RECORDS_REPLAYED,
                      (uint64_t)record_count);
    acknowledged_last_seq = sink->records[record_count - 1].gateway_seq;
    gateway_code = gateway_mqtt_encode_batch(
        sink->payload, GATEWAY_MQTT_PAYLOAD_CAPACITY, sink->device_id,
        batch_seq, sink->records, record_count, &payload_size);
    if (gateway_code != GATEWAY_OK || payload_size > INT_MAX) {
        gateway_spool_cancel_prepared(sink->spool);
        return mqtt_failure(sink,
                            gateway_code == GATEWAY_OK ? GATEWAY_ERROR_RANGE
                                                       : gateway_code,
                            MOSQ_ERR_SUCCESS, "batch encode");
    }
    gateway_stats_increment(sink->stats,
                            GATEWAY_STAT_MQTT_PUBLISH_ATTEMPTS);
    code = mosquitto_publish(sink->mosq, &mid, sink->topic,
                             (int)payload_size, sink->payload, 1, false);
    if (code != MOSQ_ERR_SUCCESS) {
        return durable_transport_offline(sink, code, "publish");
    }
    (void)pthread_mutex_lock(&sink->snapshot_mutex);
    sink->snapshot.in_flight = true;
    sink->snapshot.in_flight_mid = mid;
    (void)pthread_mutex_unlock(&sink->snapshot_mutex);
    gateway_stats_increment(sink->stats,
                            GATEWAY_STAT_MQTT_PUBLISH_ACCEPTED);
    if (gateway_mqtt_reactor_notify(sink->reactor) != GATEWAY_OK) {
        return durable_transport_offline(sink, MOSQ_ERR_SUCCESS,
                                         "reactor publish notification");
    }

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
            return durable_transport_offline(sink, MOSQ_ERR_SUCCESS,
                                             "PUBACK wait");
        }
        slice = remaining < MQTT_LOOP_SLICE_MS ? remaining
                                               : MQTT_LOOP_SLICE_MS;
        gateway_code = reactor_step(sink, slice, &code);
        if (gateway_code != GATEWAY_OK || sink->transport_lost) {
            return durable_transport_offline(sink, code,
                                             "PUBACK external loop");
        }
    }

    /* 只有匹配的 PUBACK 到达后，才原子推进 state 游标。 */
    gateway_code = gateway_spool_ack_prepared(sink->spool);
    if (gateway_code != GATEWAY_OK) {
        return spool_failure(sink, gateway_code, "persist ACK cursor");
    }
    gateway_stats_add(sink->stats, GATEWAY_STAT_SPOOL_RECORDS_ACKED,
                      (uint64_t)record_count);
    gateway_stats_add(sink->stats, GATEWAY_STAT_MQTT_RECORDS_ACKED,
                      (uint64_t)record_count);
    (void)pthread_mutex_lock(&sink->snapshot_mutex);
    sink->snapshot.batches_acked++;
    sink->snapshot.records_acked += (uint64_t)record_count;
    sink->snapshot.last_acked_batch_seq = batch_seq;
    sink->snapshot.last_acked_gateway_seq = acknowledged_last_seq;
    sink->snapshot.in_flight_mid = -1;
    (void)pthread_mutex_unlock(&sink->snapshot_mutex);
    gateway_spool_read(sink->spool, &spool_snapshot);
    sink->replay_pending = spool_snapshot.pending_records != 0;
    sink->batch_deadline_set = false;
    sync_spool_snapshot(sink);
    return GATEWAY_OK;
}

static gateway_error_code complete_reconnect(gateway_mqtt_sink *sink)
{
    if (!sink->connect_callback_seen) {
        return GATEWAY_OK;
    }
    sink->reconnect_in_progress = false;
    sink->reconnect_attempt_deadline_set = false;
    if (sink->connect_result != 0) {
        return mqtt_failure(sink, GATEWAY_ERROR_IO, MOSQ_ERR_SUCCESS,
                            "reconnect CONNACK rejected");
    }
    gateway_stats_increment(sink->stats, GATEWAY_STAT_MQTT_CONNECT_SUCCESS);
    if (sink->ever_connected) {
        gateway_stats_increment(sink->stats, GATEWAY_STAT_MQTT_RECONNECTS);
    }
    sink->ever_connected = true;
    sink->reconnect_deadline_set = false;
    sink->replay_pending = true;
    gateway_log(sink->logger, GATEWAY_LOG_INFO, "mqtt",
                "connected broker=%s port=%u topic=%s qos=1 max_inflight=1",
                sink->broker_host, (unsigned int)sink->broker_port,
                sink->topic);
    return GATEWAY_OK;
}

static gateway_error_code durable_reconnect_step(gateway_mqtt_sink *sink)
{
    gateway_error_code reactor_code;
    bool connected;
    int code = MOSQ_ERR_SUCCESS;

    (void)pthread_mutex_lock(&sink->snapshot_mutex);
    connected = sink->snapshot.connected;
    (void)pthread_mutex_unlock(&sink->snapshot_mutex);
    if (connected) {
        return GATEWAY_OK;
    }
    if (!sink->reconnect_in_progress &&
        (!sink->reconnect_deadline_set ||
         timespec_reached(&sink->reconnect_deadline))) {
        gateway_stats_increment(sink->stats,
                                GATEWAY_STAT_MQTT_CONNECT_ATTEMPTS);
        sink->connect_callback_seen = false;
        sink->connect_result = 0;
        code = mosquitto_connect_async(sink->mosq, sink->broker_host,
                                       (int)sink->broker_port,
                                       MQTT_KEEPALIVE_SECONDS);
        if (code != MOSQ_ERR_SUCCESS) {
            return durable_transport_offline(sink, code,
                                             "async reconnect");
        }
        if (gateway_mqtt_reactor_notify(sink->reactor) != GATEWAY_OK) {
            return mqtt_failure(sink, GATEWAY_ERROR_SYSTEM,
                                MOSQ_ERR_SUCCESS,
                                "async reconnect notification");
        }
        if (!deadline_after_milliseconds(
                sink->ack_timeout_ms,
                &sink->reconnect_attempt_deadline)) {
            return mqtt_failure(sink, GATEWAY_ERROR_SYSTEM,
                                MOSQ_ERR_SUCCESS,
                                "async reconnect deadline");
        }
        sink->reconnect_in_progress = true;
        sink->reconnect_attempt_deadline_set = true;
    }
    if (!sink->reconnect_in_progress) {
        return GATEWAY_OK;
    }
    reactor_code = reactor_step(sink, 0, &code);
    if (reactor_code != GATEWAY_OK || sink->transport_lost) {
        return durable_transport_offline(sink, code,
                                         "async reconnect external loop");
    }
    if (sink->connect_callback_seen) {
        return complete_reconnect(sink);
    }
    if (sink->reconnect_attempt_deadline_set &&
        timespec_reached(&sink->reconnect_attempt_deadline)) {
        return durable_transport_offline(sink, MOSQ_ERR_SUCCESS,
                                         "async reconnect wait");
    }
    return GATEWAY_OK;
}

gateway_error_code gateway_mqtt_sink_flush(gateway_mqtt_sink *sink)
{
    struct timespec deadline;
    gateway_error_code reactor_code;
    size_t payload_size;
    size_t acknowledged_records;
    uint64_t acknowledged_last_seq;
    uint64_t batch_seq;
    int mid = -1;
    int code;

    if (sink == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    if (sink->durable) {
        return flush_durable(sink);
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
    if (gateway_mqtt_reactor_notify(sink->reactor) != GATEWAY_OK) {
        return mqtt_failure(sink, GATEWAY_ERROR_SYSTEM, MOSQ_ERR_SUCCESS,
                            "reactor publish notification");
    }

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
        reactor_code = reactor_step(sink, slice, &code);
        if (reactor_code != GATEWAY_OK) {
            return mqtt_failure(sink, reactor_code, code,
                                "PUBACK external loop");
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
    if (sink->durable) {
        gateway_spool_snapshot spool_snapshot;

        if (sink->have_last_gateway_seq &&
            record->gateway_seq <= sink->last_gateway_seq) {
            return GATEWAY_ERROR_INVALID_VALUE;
        }
        code = gateway_spool_append(sink->spool, record);
        if (code != GATEWAY_OK) {
            return spool_failure(sink, code, "append record");
        }
        gateway_stats_increment(sink->stats,
                                GATEWAY_STAT_SPOOL_RECORDS_APPENDED);
        sink->have_last_gateway_seq = true;
        sink->last_gateway_seq = record->gateway_seq;
        if (!sink->batch_deadline_set) {
            if (!deadline_after_milliseconds(sink->batch_interval_ms,
                                             &sink->batch_deadline)) {
                return spool_failure(sink, GATEWAY_ERROR_SYSTEM,
                                     "batch deadline");
            }
            sink->batch_deadline_set = true;
        }
        /*
         * 持续 CAN 输入时 consumer 队列不会进入 idle 回调；因此重连期限
         * 和 external-loop 也必须在 consume 路径推进，不能只依赖队列
         * 空闲时的 consume_idle 回调。
         */
        if (sink->ever_connected || sink->reconnect_deadline_set) {
            code = gateway_mqtt_sink_poll(sink);
            if (code != GATEWAY_OK) {
                return code;
            }
        }
        gateway_spool_read(sink->spool, &spool_snapshot);
        sync_spool_snapshot(sink);
        if (spool_snapshot.pending_records >= sink->max_records) {
            return flush_durable(sink);
        }
        return GATEWAY_OK;
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
    gateway_error_code reactor_code;
    int code;

    if (sink == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    if (sink->durable) {
        gateway_spool_snapshot spool_snapshot;
        bool connected;

        reactor_code = durable_reconnect_step(sink);
        if (reactor_code != GATEWAY_OK) {
            return reactor_code;
        }
        (void)pthread_mutex_lock(&sink->snapshot_mutex);
        connected = sink->snapshot.connected;
        (void)pthread_mutex_unlock(&sink->snapshot_mutex);
        if (!connected) {
            return GATEWAY_OK;
        }
        reactor_code = reactor_step(sink, 0, &code);
        if (reactor_code != GATEWAY_OK || sink->transport_lost) {
            return durable_transport_offline(sink, code,
                                             "external-loop poll");
        }
        gateway_spool_read(sink->spool, &spool_snapshot);
        if (spool_snapshot.pending_records != 0 &&
            (sink->replay_pending ||
             spool_snapshot.pending_records >= sink->max_records ||
             (sink->batch_deadline_set &&
              timespec_reached(&sink->batch_deadline)))) {
            return flush_durable(sink);
        }
        return GATEWAY_OK;
    }
    reactor_code = reactor_step(sink, 0, &code);
    if (reactor_code != GATEWAY_OK) {
        return mqtt_failure(sink, reactor_code, code,
                            "external-loop poll");
    }
    if (sink->record_count > 0 && sink->batch_deadline_set &&
        timespec_reached(&sink->batch_deadline)) {
        return gateway_mqtt_sink_flush(sink);
    }
    return GATEWAY_OK;
}

gateway_error_code gateway_mqtt_sink_disconnect(gateway_mqtt_sink *sink)
{
    gateway_error_code reactor_code;
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
    if (gateway_mqtt_reactor_notify(sink->reactor) != GATEWAY_OK) {
        return mqtt_failure(sink, GATEWAY_ERROR_SYSTEM, MOSQ_ERR_SUCCESS,
                            "reactor disconnect notification");
    }
    reactor_code = reactor_step(sink, MQTT_LOOP_SLICE_MS, &code);
    if (reactor_code != GATEWAY_OK && code != MOSQ_ERR_NO_CONN) {
        return mqtt_failure(sink, reactor_code, code,
                            "disconnect external loop");
    }
    return GATEWAY_OK;
}

void gateway_mqtt_sink_read(gateway_mqtt_sink *sink,
                            gateway_mqtt_sink_snapshot *snapshot)
{
    if (sink == NULL || snapshot == NULL) {
        return;
    }
    sync_spool_snapshot(sink);
    sync_reactor_snapshot(sink);
    (void)pthread_mutex_lock(&sink->snapshot_mutex);
    *snapshot = sink->snapshot;
    (void)pthread_mutex_unlock(&sink->snapshot_mutex);
}

uint64_t gateway_mqtt_sink_next_gateway_seq(gateway_mqtt_sink *sink)
{
    uint64_t last_sequence;

    if (sink == NULL) {
        return 0;
    }
    last_sequence = sink->durable
                        ? gateway_spool_last_gateway_seq(sink->spool)
                        : sink->last_gateway_seq;
    return last_sequence == UINT64_MAX ? 0 : last_sequence + 1;
}

void gateway_mqtt_library_version(int *major, int *minor, int *revision)
{
    (void)mosquitto_lib_version(major, minor, revision);
}
