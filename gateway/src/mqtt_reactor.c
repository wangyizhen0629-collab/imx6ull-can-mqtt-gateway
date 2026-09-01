#include "gateway/mqtt_reactor.h"

#include <errno.h>
#include <mosquitto.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

enum {
    REACTOR_EVENT_CAPACITY = 3,
    REACTOR_TAG_WAKE = 1,
    REACTOR_TAG_TIMER = 2,
    REACTOR_TAG_MQTT = 3
};

struct gateway_mqtt_reactor {
    int epoll_fd;
    int wake_fd;
    int timer_fd;
    int network_fd;
    uint32_t network_events;
    struct mosquitto *client;
    gateway_mqtt_reactor_snapshot snapshot;
};

static gateway_error_code add_fixed_fd(int epoll_fd,
                                       int fd,
                                       uint32_t tag)
{
    struct epoll_event event;

    (void)memset(&event, 0, sizeof(event));
    event.events = EPOLLIN;
    event.data.u32 = tag;
    return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == 0
               ? GATEWAY_OK
               : GATEWAY_ERROR_SYSTEM;
}

static void remove_network_fd(gateway_mqtt_reactor *reactor)
{
    if (reactor->network_fd >= 0) {
        (void)epoll_ctl(reactor->epoll_fd, EPOLL_CTL_DEL,
                        reactor->network_fd, NULL);
    }
    reactor->network_fd = -1;
    reactor->network_events = 0;
    reactor->snapshot.network_fd = -1;
}

static gateway_error_code update_network_fd(gateway_mqtt_reactor *reactor)
{
    struct epoll_event event;
    uint32_t desired_events;
    int desired_fd;
    int operation;

    desired_fd = reactor->client == NULL ? -1
                                         : mosquitto_socket(reactor->client);
    if (desired_fd < 0) {
        remove_network_fd(reactor);
        return GATEWAY_OK;
    }
    desired_events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
    if (mosquitto_want_write(reactor->client)) {
        desired_events |= EPOLLOUT;
    }
    if (desired_fd != reactor->network_fd) {
        remove_network_fd(reactor);
        operation = EPOLL_CTL_ADD;
    } else if (desired_events != reactor->network_events) {
        operation = EPOLL_CTL_MOD;
    } else {
        return GATEWAY_OK;
    }

    (void)memset(&event, 0, sizeof(event));
    event.events = desired_events;
    event.data.u32 = REACTOR_TAG_MQTT;
    if (epoll_ctl(reactor->epoll_fd, operation, desired_fd, &event) != 0) {
        remove_network_fd(reactor);
        return GATEWAY_ERROR_SYSTEM;
    }
    reactor->network_fd = desired_fd;
    reactor->network_events = desired_events;
    reactor->snapshot.network_fd = desired_fd;
    return GATEWAY_OK;
}

static gateway_error_code drain_wake_fd(gateway_mqtt_reactor *reactor)
{
    uint64_t value;

    for (;;) {
        ssize_t size = read(reactor->wake_fd, &value, sizeof(value));

        if (size == (ssize_t)sizeof(value)) {
            reactor->snapshot.wake_events += value;
            continue;
        }
        if (size < 0 && errno == EINTR) {
            continue;
        }
        if (size < 0 && errno == EAGAIN) {
            return GATEWAY_OK;
        }
        return GATEWAY_ERROR_SYSTEM;
    }
}

static gateway_error_code process_timer(gateway_mqtt_reactor *reactor,
                                        int *mosquitto_code)
{
    uint64_t expirations;
    ssize_t size;

    do {
        size = read(reactor->timer_fd, &expirations, sizeof(expirations));
    } while (size < 0 && errno == EINTR);
    if (size < 0 && errno == EAGAIN) {
        return GATEWAY_OK;
    }
    if (size != (ssize_t)sizeof(expirations)) {
        return GATEWAY_ERROR_SYSTEM;
    }
    reactor->snapshot.timer_expirations += expirations;
    if (reactor->client == NULL || reactor->network_fd < 0) {
        return GATEWAY_OK;
    }
    reactor->snapshot.loop_misc_calls++;
    *mosquitto_code = mosquitto_loop_misc(reactor->client);
    return *mosquitto_code == MOSQ_ERR_SUCCESS ? GATEWAY_OK
                                               : GATEWAY_ERROR_IO;
}

static gateway_error_code process_socket(gateway_mqtt_reactor *reactor,
                                         uint32_t events,
                                         int *mosquitto_code)
{
    if (reactor->client == NULL || reactor->network_fd < 0) {
        return GATEWAY_OK;
    }
    reactor->snapshot.socket_events++;
    if ((events & EPOLLOUT) != 0U &&
        mosquitto_want_write(reactor->client)) {
        reactor->snapshot.loop_write_calls++;
        *mosquitto_code = mosquitto_loop_write(reactor->client, 1);
        if (*mosquitto_code != MOSQ_ERR_SUCCESS) {
            return GATEWAY_ERROR_IO;
        }
    }
    if ((events & (EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0U) {
        reactor->snapshot.loop_read_calls++;
        *mosquitto_code = mosquitto_loop_read(reactor->client, 1);
        if (*mosquitto_code != MOSQ_ERR_SUCCESS) {
            return GATEWAY_ERROR_IO;
        }
    }
    return GATEWAY_OK;
}

gateway_error_code gateway_mqtt_reactor_create(
    gateway_mqtt_reactor **reactor,
    struct mosquitto *client,
    uint32_t misc_interval_ms)
{
    gateway_mqtt_reactor *created;
    struct itimerspec timer;

    if (reactor == NULL || misc_interval_ms == 0) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    *reactor = NULL;
    created = calloc(1, sizeof(*created));
    if (created == NULL) {
        return GATEWAY_ERROR_SYSTEM;
    }
    created->epoll_fd = -1;
    created->wake_fd = -1;
    created->timer_fd = -1;
    created->network_fd = -1;
    created->snapshot.network_fd = -1;
    created->client = client;

    created->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    created->wake_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    created->timer_fd = timerfd_create(CLOCK_MONOTONIC,
                                       TFD_CLOEXEC | TFD_NONBLOCK);
    if (created->epoll_fd < 0 || created->wake_fd < 0 ||
        created->timer_fd < 0 ||
        add_fixed_fd(created->epoll_fd, created->wake_fd,
                     REACTOR_TAG_WAKE) != GATEWAY_OK ||
        add_fixed_fd(created->epoll_fd, created->timer_fd,
                     REACTOR_TAG_TIMER) != GATEWAY_OK) {
        gateway_mqtt_reactor_destroy(created);
        return GATEWAY_ERROR_SYSTEM;
    }

    (void)memset(&timer, 0, sizeof(timer));
    timer.it_interval.tv_sec = (time_t)(misc_interval_ms / 1000U);
    timer.it_interval.tv_nsec =
        (long)(misc_interval_ms % 1000U) * 1000000L;
    timer.it_value = timer.it_interval;
    if (timerfd_settime(created->timer_fd, 0, &timer, NULL) != 0 ||
        update_network_fd(created) != GATEWAY_OK) {
        gateway_mqtt_reactor_destroy(created);
        return GATEWAY_ERROR_SYSTEM;
    }
    created->snapshot.enabled = true;
    *reactor = created;
    return GATEWAY_OK;
}

void gateway_mqtt_reactor_destroy(gateway_mqtt_reactor *reactor)
{
    if (reactor == NULL) {
        return;
    }
    remove_network_fd(reactor);
    if (reactor->timer_fd >= 0) {
        (void)close(reactor->timer_fd);
    }
    if (reactor->wake_fd >= 0) {
        (void)close(reactor->wake_fd);
    }
    if (reactor->epoll_fd >= 0) {
        (void)close(reactor->epoll_fd);
    }
    free(reactor);
}

gateway_error_code gateway_mqtt_reactor_bind(
    gateway_mqtt_reactor *reactor,
    struct mosquitto *client)
{
    if (reactor == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    remove_network_fd(reactor);
    reactor->client = client;
    return update_network_fd(reactor);
}

gateway_error_code gateway_mqtt_reactor_notify(
    gateway_mqtt_reactor *reactor)
{
    uint64_t value = 1;
    ssize_t size;

    if (reactor == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    do {
        size = write(reactor->wake_fd, &value, sizeof(value));
    } while (size < 0 && errno == EINTR);
    if (size < 0 && errno == EAGAIN) {
        return GATEWAY_OK;
    }
    if (size != (ssize_t)sizeof(value)) {
        return GATEWAY_ERROR_SYSTEM;
    }
    reactor->snapshot.wake_notifications++;
    return GATEWAY_OK;
}

gateway_error_code gateway_mqtt_reactor_step(
    gateway_mqtt_reactor *reactor,
    int timeout_ms,
    int *mosquitto_code)
{
    struct epoll_event events[REACTOR_EVENT_CAPACITY];
    int event_count;
    int index;

    if (reactor == NULL || timeout_ms < -1 || mosquitto_code == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    *mosquitto_code = MOSQ_ERR_SUCCESS;
    if (update_network_fd(reactor) != GATEWAY_OK) {
        return GATEWAY_ERROR_SYSTEM;
    }
    reactor->snapshot.epoll_waits++;
    event_count = epoll_wait(reactor->epoll_fd, events,
                             REACTOR_EVENT_CAPACITY, timeout_ms);
    if (event_count < 0 && errno == EINTR) {
        return GATEWAY_OK;
    }
    if (event_count < 0) {
        return GATEWAY_ERROR_SYSTEM;
    }
    for (index = 0; index < event_count; index++) {
        gateway_error_code code = GATEWAY_OK;

        if (events[index].data.u32 == REACTOR_TAG_WAKE) {
            code = drain_wake_fd(reactor);
        } else if (events[index].data.u32 == REACTOR_TAG_TIMER) {
            code = process_timer(reactor, mosquitto_code);
        } else if (events[index].data.u32 == REACTOR_TAG_MQTT) {
            code = process_socket(reactor, events[index].events,
                                  mosquitto_code);
        }
        if (code != GATEWAY_OK) {
            (void)update_network_fd(reactor);
            return code;
        }
    }
    return update_network_fd(reactor);
}

void gateway_mqtt_reactor_read(
    gateway_mqtt_reactor *reactor,
    gateway_mqtt_reactor_snapshot *snapshot)
{
    if (reactor == NULL || snapshot == NULL) {
        return;
    }
    *snapshot = reactor->snapshot;
}
