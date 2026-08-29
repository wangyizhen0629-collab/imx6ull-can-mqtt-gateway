#include "gateway/lifecycle.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t signal_write_fd = -1;

static void lifecycle_signal_handler(int signal_number)
{
    int saved_errno = errno;
    uint8_t value = (uint8_t)signal_number;
    int descriptor = (int)signal_write_fd;

    /* signal handler 只执行 async-signal-safe 的 write，锁和广播留给主循环。 */
    if (descriptor >= 0) {
        (void)write(descriptor, &value, sizeof(value));
    }
    errno = saved_errno;
}

static int make_nonblocking_cloexec(int descriptor)
{
    int flags = fcntl(descriptor, F_GETFL);
    int descriptor_flags;

    if (flags < 0 || fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -1;
    }
    descriptor_flags = fcntl(descriptor, F_GETFD);
    if (descriptor_flags < 0 ||
        fcntl(descriptor, F_SETFD, descriptor_flags | FD_CLOEXEC) < 0) {
        return -1;
    }
    return 0;
}

static void deadline_after(clockid_t clock_id, int timeout_ms,
                           struct timespec *deadline)
{
    (void)clock_gettime(clock_id, deadline);
    deadline->tv_sec += timeout_ms / 1000;
    deadline->tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (deadline->tv_nsec >= 1000000000L) {
        deadline->tv_sec++;
        deadline->tv_nsec -= 1000000000L;
    }
}

gateway_error_code gateway_lifecycle_init(gateway_lifecycle *lifecycle)
{
    pthread_condattr_t attributes;

    if (lifecycle == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    (void)memset(lifecycle, 0, sizeof(*lifecycle));
    lifecycle->signal_pipe[0] = -1;
    lifecycle->signal_pipe[1] = -1;
    lifecycle->condition_clock = CLOCK_REALTIME;

    if (pthread_mutex_init(&lifecycle->mutex, NULL) != 0) {
        return GATEWAY_ERROR_SYSTEM;
    }
    if (pthread_condattr_init(&attributes) != 0) {
        (void)pthread_mutex_destroy(&lifecycle->mutex);
        return GATEWAY_ERROR_SYSTEM;
    }
    if (pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC) == 0) {
        lifecycle->condition_clock = CLOCK_MONOTONIC;
    }
    if (pthread_cond_init(&lifecycle->condition, &attributes) != 0) {
        (void)pthread_condattr_destroy(&attributes);
        (void)pthread_mutex_destroy(&lifecycle->mutex);
        return GATEWAY_ERROR_SYSTEM;
    }
    (void)pthread_condattr_destroy(&attributes);
    if (pipe(lifecycle->signal_pipe) != 0 ||
        make_nonblocking_cloexec(lifecycle->signal_pipe[0]) != 0 ||
        make_nonblocking_cloexec(lifecycle->signal_pipe[1]) != 0) {
        if (lifecycle->signal_pipe[0] >= 0) {
            (void)close(lifecycle->signal_pipe[0]);
        }
        if (lifecycle->signal_pipe[1] >= 0) {
            (void)close(lifecycle->signal_pipe[1]);
        }
        (void)pthread_cond_destroy(&lifecycle->condition);
        (void)pthread_mutex_destroy(&lifecycle->mutex);
        return GATEWAY_ERROR_SYSTEM;
    }
    return GATEWAY_OK;
}

void gateway_lifecycle_destroy(gateway_lifecycle *lifecycle)
{
    if (lifecycle == NULL) {
        return;
    }
    gateway_lifecycle_restore_signal_handlers(lifecycle);
    if (lifecycle->signal_pipe[0] >= 0) {
        (void)close(lifecycle->signal_pipe[0]);
    }
    if (lifecycle->signal_pipe[1] >= 0) {
        (void)close(lifecycle->signal_pipe[1]);
    }
    (void)pthread_cond_destroy(&lifecycle->condition);
    (void)pthread_mutex_destroy(&lifecycle->mutex);
}

gateway_error_code gateway_lifecycle_install_signal_handlers(
    gateway_lifecycle *lifecycle)
{
    struct sigaction action;

    if (lifecycle == NULL || lifecycle->handlers_installed ||
        signal_write_fd >= 0) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    (void)memset(&action, 0, sizeof(action));
    action.sa_handler = lifecycle_signal_handler;
    (void)sigemptyset(&action.sa_mask);
    signal_write_fd = lifecycle->signal_pipe[1];
    if (sigaction(SIGINT, &action, &lifecycle->previous_sigint) != 0) {
        signal_write_fd = -1;
        return GATEWAY_ERROR_SYSTEM;
    }
    if (sigaction(SIGTERM, &action, &lifecycle->previous_sigterm) != 0) {
        (void)sigaction(SIGINT, &lifecycle->previous_sigint, NULL);
        signal_write_fd = -1;
        return GATEWAY_ERROR_SYSTEM;
    }
    lifecycle->handlers_installed = true;
    return GATEWAY_OK;
}

void gateway_lifecycle_restore_signal_handlers(gateway_lifecycle *lifecycle)
{
    if (lifecycle == NULL || !lifecycle->handlers_installed) {
        return;
    }
    (void)sigaction(SIGINT, &lifecycle->previous_sigint, NULL);
    (void)sigaction(SIGTERM, &lifecycle->previous_sigterm, NULL);
    signal_write_fd = -1;
    lifecycle->handlers_installed = false;
}

gateway_error_code gateway_lifecycle_request_stop(gateway_lifecycle *lifecycle,
                                                   int signal_number)
{
    if (lifecycle == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    if (pthread_mutex_lock(&lifecycle->mutex) != 0) {
        return GATEWAY_ERROR_SYSTEM;
    }
    if (!lifecycle->stop_requested) {
        lifecycle->stop_requested = true;
        lifecycle->stop_signal = signal_number;
    }
    (void)pthread_cond_broadcast(&lifecycle->condition);
    (void)pthread_mutex_unlock(&lifecycle->mutex);
    return GATEWAY_OK;
}

gateway_error_code gateway_lifecycle_wait(gateway_lifecycle *lifecycle,
                                          int timeout_ms,
                                          int *signal_number)
{
    gateway_error_code result = GATEWAY_OK;
    struct timespec deadline;

    if (lifecycle == NULL || timeout_ms < -1) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    if (pthread_mutex_lock(&lifecycle->mutex) != 0) {
        return GATEWAY_ERROR_SYSTEM;
    }
    if (timeout_ms >= 0) {
        deadline_after(lifecycle->condition_clock, timeout_ms, &deadline);
    }
    while (!lifecycle->stop_requested) {
        int status;
        if (timeout_ms < 0) {
            status = pthread_cond_wait(&lifecycle->condition, &lifecycle->mutex);
        } else {
            status = pthread_cond_timedwait(&lifecycle->condition,
                                            &lifecycle->mutex, &deadline);
        }
        if (status == ETIMEDOUT) {
            result = GATEWAY_ERROR_TIMEOUT;
            break;
        }
        if (status != 0) {
            result = GATEWAY_ERROR_SYSTEM;
            break;
        }
    }
    if (result == GATEWAY_OK && signal_number != NULL) {
        *signal_number = lifecycle->stop_signal;
    }
    (void)pthread_mutex_unlock(&lifecycle->mutex);
    return result;
}

gateway_error_code gateway_lifecycle_wait_signal(gateway_lifecycle *lifecycle,
                                                 int timeout_ms,
                                                 int *signal_number)
{
    struct pollfd descriptor;
    uint8_t value;
    ssize_t bytes_read;
    int status;

    if (lifecycle == NULL || timeout_ms < -1 || !lifecycle->handlers_installed) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    descriptor.fd = lifecycle->signal_pipe[0];
    descriptor.events = POLLIN;
    descriptor.revents = 0;
    do {
        status = poll(&descriptor, 1, timeout_ms);
    } while (status < 0 && errno == EINTR);
    if (status == 0) {
        return GATEWAY_ERROR_TIMEOUT;
    }
    if (status < 0 || (descriptor.revents & POLLIN) == 0) {
        return GATEWAY_ERROR_SYSTEM;
    }
    do {
        bytes_read = read(lifecycle->signal_pipe[0], &value, sizeof(value));
    } while (bytes_read < 0 && errno == EINTR);
    if (bytes_read != (ssize_t)sizeof(value)) {
        return GATEWAY_ERROR_SYSTEM;
    }
    if (gateway_lifecycle_request_stop(lifecycle, (int)value) != GATEWAY_OK) {
        return GATEWAY_ERROR_SYSTEM;
    }
    if (signal_number != NULL) {
        *signal_number = (int)value;
    }
    return GATEWAY_OK;
}

bool gateway_lifecycle_is_stop_requested(gateway_lifecycle *lifecycle,
                                         int *signal_number)
{
    bool requested;

    if (lifecycle == NULL || pthread_mutex_lock(&lifecycle->mutex) != 0) {
        return false;
    }
    requested = lifecycle->stop_requested;
    if (requested && signal_number != NULL) {
        *signal_number = lifecycle->stop_signal;
    }
    (void)pthread_mutex_unlock(&lifecycle->mutex);
    return requested;
}
