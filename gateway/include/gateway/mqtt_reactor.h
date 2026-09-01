#ifndef GATEWAY_MQTT_REACTOR_H
#define GATEWAY_MQTT_REACTOR_H

#include "gateway/error.h"

#include <stdbool.h>
#include <stdint.h>

struct mosquitto;

typedef struct {
    bool enabled;
    int network_fd;
    uint64_t epoll_waits;
    uint64_t wake_notifications;
    uint64_t wake_events;
    uint64_t timer_expirations;
    uint64_t socket_events;
    uint64_t loop_read_calls;
    uint64_t loop_write_calls;
    uint64_t loop_misc_calls;
} gateway_mqtt_reactor_snapshot;

typedef struct gateway_mqtt_reactor gateway_mqtt_reactor;

gateway_error_code gateway_mqtt_reactor_create(
    gateway_mqtt_reactor **reactor,
    struct mosquitto *client,
    uint32_t misc_interval_ms);
void gateway_mqtt_reactor_destroy(gateway_mqtt_reactor *reactor);

gateway_error_code gateway_mqtt_reactor_bind(
    gateway_mqtt_reactor *reactor,
    struct mosquitto *client);
gateway_error_code gateway_mqtt_reactor_notify(
    gateway_mqtt_reactor *reactor);

/*
 * 执行一次有界 reactor 等待。mosquitto_code 始终返回本次 external-loop API
 * 的结果；系统调用失败时保持为 MOSQ_ERR_SUCCESS，由 gateway error 区分。
 */
gateway_error_code gateway_mqtt_reactor_step(
    gateway_mqtt_reactor *reactor,
    int timeout_ms,
    int *mosquitto_code);

void gateway_mqtt_reactor_read(
    gateway_mqtt_reactor *reactor,
    gateway_mqtt_reactor_snapshot *snapshot);

#endif
