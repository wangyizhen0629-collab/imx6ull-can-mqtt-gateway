#include "gateway/config.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    CONFIG_LINE_SIZE = 2048,
    CONFIG_FIELD_DEVICE_ID = 0,
    CONFIG_FIELD_CAN_INTERFACE,
    CONFIG_FIELD_BROKER_HOST,
    CONFIG_FIELD_BROKER_PORT,
    CONFIG_FIELD_BROKER_USERNAME,
    CONFIG_FIELD_BROKER_PASSWORD,
    CONFIG_FIELD_MQTT_TOPIC,
    CONFIG_FIELD_QUEUE_CAPACITY,
    CONFIG_FIELD_QUEUE_PUSH_TIMEOUT_MS,
    CONFIG_FIELD_BATCH_INTERVAL_MS,
    CONFIG_FIELD_SPOOL_PATH,
    CONFIG_FIELD_LOG_LEVEL,
    CONFIG_FIELD_COUNT
};

static void clear_error(gateway_config_error *error)
{
    if (error != NULL) {
        (void)memset(error, 0, sizeof(*error));
        error->code = GATEWAY_OK;
    }
}

static gateway_error_code set_error(gateway_config_error *error,
                                    gateway_error_code code,
                                    size_t line,
                                    const char *key,
                                    const char *message)
{
    if (error != NULL) {
        error->code = code;
        error->line = line;
        (void)snprintf(error->key, sizeof(error->key), "%s",
                       key == NULL ? "" : key);
        (void)snprintf(error->message, sizeof(error->message), "%s",
                       message == NULL ? gateway_error_string(code) : message);
    }
    return code;
}

static char *trim(char *text)
{
    char *end;

    while (isspace((unsigned char)*text) != 0) {
        text++;
    }
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]) != 0) {
        end--;
    }
    *end = '\0';
    return text;
}

static bool valid_identifier(const char *value, size_t maximum, bool allow_colon)
{
    size_t index;
    size_t length = strlen(value);

    if (length == 0 || length > maximum) {
        return false;
    }
    for (index = 0; index < length; index++) {
        unsigned char character = (unsigned char)value[index];
        if (isalnum(character) == 0 && character != '-' && character != '_' &&
            character != '.' && (!allow_colon || character != ':')) {
            return false;
        }
    }
    return true;
}

static bool valid_visible_string(const char *value, size_t maximum, bool required)
{
    size_t index;
    size_t length = strlen(value);

    if ((required && length == 0) || length > maximum) {
        return false;
    }
    for (index = 0; index < length; index++) {
        unsigned char character = (unsigned char)value[index];
        if (iscntrl(character) != 0) {
            return false;
        }
    }
    return true;
}

static gateway_error_code parse_unsigned(const char *value,
                                         unsigned long long minimum,
                                         unsigned long long maximum,
                                         unsigned long long *parsed)
{
    unsigned long long result;
    char *end = NULL;

    if (value == NULL || value[0] == '\0' || value[0] == '-') {
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    errno = 0;
    result = strtoull(value, &end, 10);
    if (errno == ERANGE || result < minimum || result > maximum) {
        return GATEWAY_ERROR_RANGE;
    }
    if (end == value || *end != '\0') {
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    *parsed = result;
    return GATEWAY_OK;
}

static int field_index(const char *key)
{
    static const char *const keys[CONFIG_FIELD_COUNT] = {
        "device_id", "can_interface", "broker_host", "broker_port",
        "broker_username", "broker_password", "mqtt_topic",
        "queue_capacity", "queue_push_timeout_ms", "batch_interval_ms",
        "spool_path", "log_level"
    };
    int index;

    for (index = 0; index < CONFIG_FIELD_COUNT; index++) {
        if (strcmp(key, keys[index]) == 0) {
            return index;
        }
    }
    return -1;
}

static gateway_error_code set_field(gateway_config *config,
                                    const char *key,
                                    const char *value,
                                    size_t line,
                                    gateway_config_error *error)
{
    unsigned long long parsed;
    gateway_log_level level;
    gateway_error_code code;
    int field = field_index(key);

    if (field < 0) {
        return set_error(error, GATEWAY_ERROR_UNKNOWN_KEY, line, key, NULL);
    }

    switch (field) {
    case CONFIG_FIELD_DEVICE_ID:
        if (!valid_identifier(value, GATEWAY_DEVICE_ID_SIZE - 1, false)) {
            return set_error(error, GATEWAY_ERROR_INVALID_VALUE, line, key,
                             "device_id must use [A-Za-z0-9._-]");
        }
        (void)snprintf(config->device_id, sizeof(config->device_id), "%s", value);
        break;
    case CONFIG_FIELD_CAN_INTERFACE:
        if (!valid_identifier(value, GATEWAY_CAN_INTERFACE_SIZE - 1, true)) {
            return set_error(error, GATEWAY_ERROR_INVALID_VALUE, line, key,
                             "invalid CAN interface name");
        }
        (void)snprintf(config->can_interface, sizeof(config->can_interface),
                       "%s", value);
        break;
    case CONFIG_FIELD_BROKER_HOST:
        if (!valid_visible_string(value, GATEWAY_BROKER_HOST_SIZE - 1, true) ||
            strpbrk(value, " \t") != NULL) {
            return set_error(error, GATEWAY_ERROR_INVALID_VALUE, line, key,
                             "invalid Broker hostname/address");
        }
        (void)snprintf(config->broker_host, sizeof(config->broker_host), "%s",
                       value);
        break;
    case CONFIG_FIELD_BROKER_PORT:
        code = parse_unsigned(value, 1, UINT16_MAX, &parsed);
        if (code != GATEWAY_OK) {
            return set_error(error, code, line, key, NULL);
        }
        config->broker_port = (uint16_t)parsed;
        break;
    case CONFIG_FIELD_BROKER_USERNAME:
        if (!valid_visible_string(value, GATEWAY_BROKER_USERNAME_SIZE - 1,
                                  false)) {
            return set_error(error, GATEWAY_ERROR_INVALID_VALUE, line, key, NULL);
        }
        (void)snprintf(config->broker_username,
                       sizeof(config->broker_username), "%s", value);
        break;
    case CONFIG_FIELD_BROKER_PASSWORD:
        if (!valid_visible_string(value, GATEWAY_BROKER_PASSWORD_SIZE - 1,
                                  false)) {
            return set_error(error, GATEWAY_ERROR_INVALID_VALUE, line, key, NULL);
        }
        (void)snprintf(config->broker_password,
                       sizeof(config->broker_password), "%s", value);
        break;
    case CONFIG_FIELD_MQTT_TOPIC:
        if (!valid_visible_string(value, GATEWAY_MQTT_TOPIC_SIZE - 1, true) ||
            strpbrk(value, "+#") != NULL) {
            return set_error(error, GATEWAY_ERROR_INVALID_VALUE, line, key,
                             "publish topic must not contain wildcards");
        }
        (void)snprintf(config->mqtt_topic, sizeof(config->mqtt_topic), "%s",
                       value);
        break;
    case CONFIG_FIELD_QUEUE_CAPACITY:
        code = parse_unsigned(value, 1, GATEWAY_QUEUE_CAPACITY_MAX, &parsed);
        if (code != GATEWAY_OK) {
            return set_error(error, code, line, key, NULL);
        }
        config->queue_capacity = (size_t)parsed;
        break;
    case CONFIG_FIELD_QUEUE_PUSH_TIMEOUT_MS:
        code = parse_unsigned(value, 0, GATEWAY_QUEUE_PUSH_TIMEOUT_MS_MAX,
                              &parsed);
        if (code != GATEWAY_OK) {
            return set_error(error, code, line, key, NULL);
        }
        config->queue_push_timeout_ms = (uint32_t)parsed;
        break;
    case CONFIG_FIELD_BATCH_INTERVAL_MS:
        code = parse_unsigned(value, GATEWAY_BATCH_INTERVAL_MS_MIN,
                              GATEWAY_BATCH_INTERVAL_MS_MAX, &parsed);
        if (code != GATEWAY_OK) {
            return set_error(error, code, line, key, NULL);
        }
        config->batch_interval_ms = (uint32_t)parsed;
        break;
    case CONFIG_FIELD_SPOOL_PATH:
        if (!valid_visible_string(value, GATEWAY_SPOOL_PATH_SIZE - 1, true) ||
            value[0] != '/') {
            return set_error(error, GATEWAY_ERROR_INVALID_VALUE, line, key,
                             "spool_path must be absolute");
        }
        (void)snprintf(config->spool_path, sizeof(config->spool_path), "%s",
                       value);
        break;
    case CONFIG_FIELD_LOG_LEVEL:
        code = gateway_log_level_parse(value, &level);
        if (code != GATEWAY_OK) {
            return set_error(error, code, line, key,
                             "log_level must be debug, info, warn, or error");
        }
        config->log_level = level;
        break;
    default:
        return set_error(error, GATEWAY_ERROR_SYSTEM, line, key, NULL);
    }
    return GATEWAY_OK;
}

void gateway_config_init_defaults(gateway_config *config)
{
    if (config == NULL) {
        return;
    }
    (void)memset(config, 0, sizeof(*config));
    (void)snprintf(config->device_id, sizeof(config->device_id), "gateway-lab");
    (void)snprintf(config->can_interface, sizeof(config->can_interface), "can0");
    (void)snprintf(config->broker_host, sizeof(config->broker_host),
                   "broker.example.invalid");
    config->broker_port = 1883;
    (void)snprintf(config->mqtt_topic, sizeof(config->mqtt_topic),
                   "vehicle/gateway-lab/telemetry");
    config->queue_capacity = 1024;
    config->queue_push_timeout_ms = 50;
    config->batch_interval_ms = 1000;
    (void)snprintf(config->spool_path, sizeof(config->spool_path),
                   "/var/lib/gatewayd/spool.data");
    config->log_level = GATEWAY_LOG_INFO;
}

gateway_error_code gateway_config_load_file(gateway_config *config,
                                            const char *path,
                                            gateway_config_error *error)
{
    char buffer[CONFIG_LINE_SIZE];
    unsigned long long fields_seen = 0;
    size_t line = 0;
    FILE *stream;

    clear_error(error);
    if (config == NULL || path == NULL) {
        return set_error(error, GATEWAY_ERROR_ARGUMENT, 0, NULL, NULL);
    }
    stream = fopen(path, "r");
    if (stream == NULL) {
        return set_error(error, GATEWAY_ERROR_IO, 0, NULL, strerror(errno));
    }

    while (fgets(buffer, sizeof(buffer), stream) != NULL) {
        char *key;
        char *value;
        char *separator;
        int field;
        gateway_error_code code;

        line++;
        if (strchr(buffer, '\n') == NULL && !feof(stream)) {
            (void)fclose(stream);
            return set_error(error, GATEWAY_ERROR_PARSE, line, NULL,
                             "configuration line is too long");
        }
        key = trim(buffer);
        if (key[0] == '\0' || key[0] == '#') {
            continue;
        }
        separator = strchr(key, '=');
        if (separator == NULL) {
            (void)fclose(stream);
            return set_error(error, GATEWAY_ERROR_PARSE, line, NULL,
                             "expected key=value");
        }
        *separator = '\0';
        value = trim(separator + 1);
        key = trim(key);
        if (key[0] == '\0') {
            (void)fclose(stream);
            return set_error(error, GATEWAY_ERROR_PARSE, line, NULL,
                             "empty configuration key");
        }
        field = field_index(key);
        if (field >= 0 && (fields_seen & (1ULL << (unsigned int)field)) != 0) {
            (void)fclose(stream);
            return set_error(error, GATEWAY_ERROR_DUPLICATE_KEY, line, key, NULL);
        }
        code = set_field(config, key, value, line, error);
        if (code != GATEWAY_OK) {
            (void)fclose(stream);
            return code;
        }
        fields_seen |= 1ULL << (unsigned int)field;
    }
    if (ferror(stream) != 0) {
        int saved_errno = errno;
        (void)fclose(stream);
        return set_error(error, GATEWAY_ERROR_IO, line, NULL,
                         strerror(saved_errno));
    }
    if (fclose(stream) != 0) {
        return set_error(error, GATEWAY_ERROR_IO, line, NULL, strerror(errno));
    }
    return gateway_config_validate(config, error);
}

gateway_error_code gateway_config_apply_assignment(gateway_config *config,
                                                   const char *assignment,
                                                   gateway_config_error *error)
{
    char buffer[CONFIG_LINE_SIZE];
    char *separator;
    char *key;
    char *value;

    clear_error(error);
    if (config == NULL || assignment == NULL) {
        return set_error(error, GATEWAY_ERROR_ARGUMENT, 0, NULL, NULL);
    }
    if (strlen(assignment) >= sizeof(buffer)) {
        return set_error(error, GATEWAY_ERROR_PARSE, 0, NULL,
                         "assignment is too long");
    }
    (void)snprintf(buffer, sizeof(buffer), "%s", assignment);
    separator = strchr(buffer, '=');
    if (separator == NULL) {
        return set_error(error, GATEWAY_ERROR_PARSE, 0, NULL,
                         "expected key=value");
    }
    *separator = '\0';
    key = trim(buffer);
    value = trim(separator + 1);
    if (key[0] == '\0') {
        return set_error(error, GATEWAY_ERROR_PARSE, 0, NULL,
                         "empty configuration key");
    }
    return set_field(config, key, value, 0, error);
}

gateway_error_code gateway_config_validate(const gateway_config *config,
                                           gateway_config_error *error)
{
    clear_error(error);
    if (config == NULL) {
        return set_error(error, GATEWAY_ERROR_ARGUMENT, 0, NULL, NULL);
    }
    if (!valid_identifier(config->device_id, GATEWAY_DEVICE_ID_SIZE - 1,
                          false)) {
        return set_error(error, GATEWAY_ERROR_INVALID_VALUE, 0, "device_id",
                         NULL);
    }
    if (!valid_identifier(config->can_interface,
                          GATEWAY_CAN_INTERFACE_SIZE - 1, true)) {
        return set_error(error, GATEWAY_ERROR_INVALID_VALUE, 0,
                         "can_interface", NULL);
    }
    if (!valid_visible_string(config->broker_host,
                              GATEWAY_BROKER_HOST_SIZE - 1, true) ||
        strpbrk(config->broker_host, " \t") != NULL) {
        return set_error(error, GATEWAY_ERROR_INVALID_VALUE, 0, "broker_host",
                         NULL);
    }
    if (config->broker_port == 0) {
        return set_error(error, GATEWAY_ERROR_RANGE, 0, "broker_port", NULL);
    }
    if (!valid_visible_string(config->broker_username,
                              GATEWAY_BROKER_USERNAME_SIZE - 1, false) ||
        !valid_visible_string(config->broker_password,
                              GATEWAY_BROKER_PASSWORD_SIZE - 1, false)) {
        return set_error(error, GATEWAY_ERROR_INVALID_VALUE, 0,
                         "broker_credentials", NULL);
    }
    if (!valid_visible_string(config->mqtt_topic,
                              GATEWAY_MQTT_TOPIC_SIZE - 1, true) ||
        strpbrk(config->mqtt_topic, "+#") != NULL) {
        return set_error(error, GATEWAY_ERROR_INVALID_VALUE, 0, "mqtt_topic",
                         NULL);
    }
    if (config->queue_capacity == 0 ||
        config->queue_capacity > GATEWAY_QUEUE_CAPACITY_MAX) {
        return set_error(error, GATEWAY_ERROR_RANGE, 0, "queue_capacity", NULL);
    }
    if (config->queue_push_timeout_ms > GATEWAY_QUEUE_PUSH_TIMEOUT_MS_MAX) {
        return set_error(error, GATEWAY_ERROR_RANGE, 0,
                         "queue_push_timeout_ms", NULL);
    }
    if (config->batch_interval_ms < GATEWAY_BATCH_INTERVAL_MS_MIN ||
        config->batch_interval_ms > GATEWAY_BATCH_INTERVAL_MS_MAX) {
        return set_error(error, GATEWAY_ERROR_RANGE, 0, "batch_interval_ms",
                         NULL);
    }
    if (!valid_visible_string(config->spool_path,
                              GATEWAY_SPOOL_PATH_SIZE - 1, true) ||
        config->spool_path[0] != '/') {
        return set_error(error, GATEWAY_ERROR_INVALID_VALUE, 0, "spool_path",
                         NULL);
    }
    if (config->log_level < GATEWAY_LOG_DEBUG ||
        config->log_level > GATEWAY_LOG_ERROR) {
        return set_error(error, GATEWAY_ERROR_INVALID_VALUE, 0, "log_level",
                         NULL);
    }
    return GATEWAY_OK;
}

void gateway_config_log_redacted(const gateway_config *config,
                                 gateway_logger *logger)
{
    if (config == NULL || logger == NULL) {
        return;
    }
    gateway_log(logger, GATEWAY_LOG_INFO, "config",
                "device_id=%s can_interface=%s broker_host=%s broker_port=%u "
                "broker_username=%s broker_password=<redacted> mqtt_topic=%s "
                "queue_capacity=%zu queue_push_timeout_ms=%u "
                "batch_interval_ms=%u spool_path=%s log_level=%s",
                config->device_id, config->can_interface, config->broker_host,
                (unsigned int)config->broker_port,
                config->broker_username[0] == '\0' ? "<unset>" : "<redacted>",
                config->mqtt_topic, config->queue_capacity,
                config->queue_push_timeout_ms, config->batch_interval_ms,
                config->spool_path, gateway_log_level_name(config->log_level));
}
