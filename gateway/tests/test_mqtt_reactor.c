#include "gateway/mqtt_reactor.h"

#include <mosquitto.h>
#include <stdio.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,         \
                    __LINE__, #condition);                                     \
            return 1;                                                          \
        }                                                                      \
    } while (0)

static int test_wake_timer_and_rebind(void)
{
    gateway_mqtt_reactor_snapshot snapshot;
    gateway_mqtt_reactor *reactor = NULL;
    struct mosquitto *client;
    int mosquitto_code = MOSQ_ERR_SUCCESS;

    CHECK(mosquitto_lib_init() == MOSQ_ERR_SUCCESS);
    client = mosquitto_new("m8-reactor-unit", true, NULL);
    CHECK(client != NULL);
    CHECK(gateway_mqtt_reactor_create(&reactor, client, 10) == GATEWAY_OK);
    gateway_mqtt_reactor_read(reactor, &snapshot);
    CHECK(snapshot.enabled);
    CHECK(snapshot.network_fd == -1);

    CHECK(gateway_mqtt_reactor_notify(reactor) == GATEWAY_OK);
    CHECK(gateway_mqtt_reactor_step(reactor, 100, &mosquitto_code) ==
          GATEWAY_OK);
    CHECK(mosquitto_code == MOSQ_ERR_SUCCESS);
    gateway_mqtt_reactor_read(reactor, &snapshot);
    CHECK(snapshot.wake_notifications == 1);
    CHECK(snapshot.wake_events == 1);

    CHECK(gateway_mqtt_reactor_step(reactor, 100, &mosquitto_code) ==
          GATEWAY_OK);
    gateway_mqtt_reactor_read(reactor, &snapshot);
    CHECK(snapshot.timer_expirations >= 1);
    CHECK(snapshot.epoll_waits >= 2);
    CHECK(snapshot.loop_read_calls == 0);
    CHECK(snapshot.loop_write_calls == 0);
    CHECK(snapshot.loop_misc_calls == 0);

    CHECK(gateway_mqtt_reactor_bind(reactor, NULL) == GATEWAY_OK);
    gateway_mqtt_reactor_destroy(reactor);
    mosquitto_destroy(client);
    CHECK(mosquitto_lib_cleanup() == MOSQ_ERR_SUCCESS);
    return 0;
}

int main(void)
{
    CHECK(test_wake_timer_and_rebind() == 0);
    return 0;
}
