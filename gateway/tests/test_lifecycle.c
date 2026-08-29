#include "gateway/lifecycle.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,         \
                    __LINE__, #condition);                                     \
            return 1;                                                          \
        }                                                                      \
    } while (0)

typedef struct {
    gateway_lifecycle *lifecycle;
    gateway_error_code result;
    int signal_number;
} wait_context;

static void *wait_for_stop(void *argument)
{
    wait_context *context = argument;
    context->result = gateway_lifecycle_wait(context->lifecycle, 2000,
                                             &context->signal_number);
    return NULL;
}

static int test_condition_wakeup(void)
{
    gateway_lifecycle lifecycle;
    wait_context context = {0};
    pthread_t thread;

    CHECK(gateway_lifecycle_init(&lifecycle) == GATEWAY_OK);
    context.lifecycle = &lifecycle;
    CHECK(pthread_create(&thread, NULL, wait_for_stop, &context) == 0);
    CHECK(gateway_lifecycle_request_stop(&lifecycle, 0) == GATEWAY_OK);
    CHECK(pthread_join(thread, NULL) == 0);
    CHECK(context.result == GATEWAY_OK);
    CHECK(context.signal_number == 0);
    CHECK(gateway_lifecycle_is_stop_requested(&lifecycle, NULL));
    gateway_lifecycle_destroy(&lifecycle);
    return 0;
}

static int test_signal_and_timeout(void)
{
    gateway_lifecycle lifecycle;
    int signal_number = 0;

    CHECK(gateway_lifecycle_init(&lifecycle) == GATEWAY_OK);
    CHECK(gateway_lifecycle_install_signal_handlers(&lifecycle) == GATEWAY_OK);
    CHECK(gateway_lifecycle_wait_signal(&lifecycle, 20, NULL) ==
          GATEWAY_ERROR_TIMEOUT);
    CHECK(raise(SIGTERM) == 0);
    CHECK(gateway_lifecycle_wait_signal(&lifecycle, 1000, &signal_number) ==
          GATEWAY_OK);
    CHECK(signal_number == SIGTERM);
    CHECK(gateway_lifecycle_is_stop_requested(&lifecycle, &signal_number));
    CHECK(signal_number == SIGTERM);
    gateway_lifecycle_destroy(&lifecycle);
    return 0;
}

int main(void)
{
    CHECK(test_condition_wakeup() == 0);
    CHECK(test_signal_and_timeout() == 0);
    return 0;
}
