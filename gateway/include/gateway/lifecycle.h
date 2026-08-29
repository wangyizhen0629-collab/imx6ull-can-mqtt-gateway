#ifndef GATEWAY_LIFECYCLE_H
#define GATEWAY_LIFECYCLE_H

#include "gateway/error.h"

#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    clockid_t condition_clock;
    int signal_pipe[2];
    bool stop_requested;
    int stop_signal;
    bool handlers_installed;
    struct sigaction previous_sigint;
    struct sigaction previous_sigterm;
} gateway_lifecycle;

gateway_error_code gateway_lifecycle_init(gateway_lifecycle *lifecycle);
void gateway_lifecycle_destroy(gateway_lifecycle *lifecycle);
gateway_error_code gateway_lifecycle_install_signal_handlers(
    gateway_lifecycle *lifecycle);
void gateway_lifecycle_restore_signal_handlers(gateway_lifecycle *lifecycle);
gateway_error_code gateway_lifecycle_request_stop(gateway_lifecycle *lifecycle,
                                                   int signal_number);
gateway_error_code gateway_lifecycle_wait(gateway_lifecycle *lifecycle,
                                          int timeout_ms,
                                          int *signal_number);
gateway_error_code gateway_lifecycle_wait_signal(gateway_lifecycle *lifecycle,
                                                 int timeout_ms,
                                                 int *signal_number);
bool gateway_lifecycle_is_stop_requested(gateway_lifecycle *lifecycle,
                                         int *signal_number);

#endif
