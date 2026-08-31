#ifndef GATEWAY_CONFIG_H
#define GATEWAY_CONFIG_H

#include "gateway/error.h"
#include "gateway/log.h"

#include <stddef.h>
#include <stdint.h>

enum {
    GATEWAY_DEVICE_ID_SIZE = 64,
    GATEWAY_CAN_INTERFACE_SIZE = 16,
    GATEWAY_BROKER_HOST_SIZE = 256,
    GATEWAY_BROKER_USERNAME_SIZE = 128,
    GATEWAY_BROKER_PASSWORD_SIZE = 256,
    GATEWAY_MQTT_TOPIC_SIZE = 256,
    GATEWAY_SPOOL_PATH_SIZE = 512,
    GATEWAY_CONFIG_KEY_SIZE = 32,
    GATEWAY_CONFIG_MESSAGE_SIZE = 160,
    GATEWAY_QUEUE_CAPACITY_MAX = 65536,
    GATEWAY_QUEUE_PUSH_TIMEOUT_MS_MAX = 60000,
    GATEWAY_BATCH_INTERVAL_MS_MIN = 100,
    GATEWAY_BATCH_INTERVAL_MS_MAX = 60000,
    GATEWAY_MQTT_ACK_TIMEOUT_MS_MIN = 100,
    GATEWAY_MQTT_ACK_TIMEOUT_MS_MAX = 60000
};

typedef struct {
    char device_id[GATEWAY_DEVICE_ID_SIZE];
    char can_interface[GATEWAY_CAN_INTERFACE_SIZE];
    char broker_host[GATEWAY_BROKER_HOST_SIZE];
    uint16_t broker_port;
    char broker_username[GATEWAY_BROKER_USERNAME_SIZE];
    char broker_password[GATEWAY_BROKER_PASSWORD_SIZE];
    char mqtt_topic[GATEWAY_MQTT_TOPIC_SIZE];
    size_t queue_capacity;
    uint32_t queue_push_timeout_ms;
    uint32_t batch_interval_ms;
    uint32_t mqtt_ack_timeout_ms;
    char spool_path[GATEWAY_SPOOL_PATH_SIZE];
    gateway_log_level log_level;
} gateway_config;

typedef struct {
    gateway_error_code code;
    size_t line;
    char key[GATEWAY_CONFIG_KEY_SIZE];
    char message[GATEWAY_CONFIG_MESSAGE_SIZE];
} gateway_config_error;

void gateway_config_init_defaults(gateway_config *config);
gateway_error_code gateway_config_load_file(gateway_config *config,
                                            const char *path,
                                            gateway_config_error *error);
gateway_error_code gateway_config_apply_assignment(gateway_config *config,
                                                   const char *assignment,
                                                   gateway_config_error *error);
gateway_error_code gateway_config_validate(const gateway_config *config,
                                           gateway_config_error *error);
void gateway_config_log_redacted(const gateway_config *config,
                                 gateway_logger *logger);

#endif
