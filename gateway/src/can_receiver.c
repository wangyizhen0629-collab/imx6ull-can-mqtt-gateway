#include "gateway/can_receiver.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static void set_reject_reason(gateway_can_reject_reason *destination,
                              gateway_can_reject_reason reason)
{
    if (destination != NULL) {
        *destination = reason;
    }
}

static int set_close_on_exec(int descriptor)
{
    int flags = fcntl(descriptor, F_GETFD);

    if (flags < 0 || fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC) < 0) {
        return -1;
    }
    return 0;
}

static gateway_error_code timestamp_to_nanoseconds(
    const struct timespec *timestamp,
    int64_t *nanoseconds)
{
    int64_t seconds;

    if (timestamp == NULL || nanoseconds == NULL || timestamp->tv_sec < 0 ||
        timestamp->tv_nsec < 0 || timestamp->tv_nsec >= 1000000000L) {
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    seconds = (int64_t)timestamp->tv_sec;
    if (seconds > (INT64_MAX - (int64_t)timestamp->tv_nsec) /
                      INT64_C(1000000000)) {
        return GATEWAY_ERROR_RANGE;
    }
    *nanoseconds = seconds * INT64_C(1000000000) +
                   (int64_t)timestamp->tv_nsec;
    return GATEWAY_OK;
}

bool gateway_can_is_target_id(uint32_t can_id)
{
    return can_id == GATEWAY_CAN_ID_VEHICLE_DYNAMICS ||
           can_id == GATEWAY_CAN_ID_POWER_STATUS ||
           can_id == GATEWAY_CAN_ID_BODY_STATUS;
}

const char *gateway_can_reject_reason_name(gateway_can_reject_reason reason)
{
    switch (reason) {
    case GATEWAY_CAN_REJECT_NONE:
        return "none";
    case GATEWAY_CAN_REJECT_DATAGRAM_LENGTH:
        return "datagram_length";
    case GATEWAY_CAN_REJECT_DLC:
        return "dlc";
    case GATEWAY_CAN_REJECT_ID:
        return "can_id";
    case GATEWAY_CAN_REJECT_CONTROL_TRUNCATED:
        return "control_truncated";
    case GATEWAY_CAN_REJECT_TIMESTAMP_MISSING:
        return "timestamp_missing";
    case GATEWAY_CAN_REJECT_TIMESTAMP_INVALID:
        return "timestamp_invalid";
    default:
        return "unknown";
    }
}

void gateway_can_receiver_init(gateway_can_receiver *receiver)
{
    if (receiver != NULL) {
        receiver->fd = -1;
    }
}

gateway_error_code gateway_can_receiver_configure_socket(int descriptor)
{
    static const struct can_filter filters[GATEWAY_CAN_TARGET_ID_COUNT] = {
        {
            .can_id = GATEWAY_CAN_ID_VEHICLE_DYNAMICS,
            .can_mask = CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG,
        },
        {
            .can_id = GATEWAY_CAN_ID_POWER_STATUS,
            .can_mask = CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG,
        },
        {
            .can_id = GATEWAY_CAN_ID_BODY_STATUS,
            .can_mask = CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG,
        },
    };
    int enabled = 1;

    if (descriptor < 0) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    if (setsockopt(descriptor, SOL_CAN_RAW, CAN_RAW_FILTER, filters,
                   sizeof(filters)) != 0 ||
        setsockopt(descriptor, SOL_SOCKET, SO_TIMESTAMPNS, &enabled,
                   sizeof(enabled)) != 0) {
        return GATEWAY_ERROR_SYSTEM;
    }
    return GATEWAY_OK;
}

gateway_error_code gateway_can_receiver_open(gateway_can_receiver *receiver,
                                             const char *interface_name)
{
    struct sockaddr_can address;
    unsigned int interface_index;
    int descriptor;

    if (receiver == NULL || interface_name == NULL || interface_name[0] == '\0' ||
        receiver->fd >= 0) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    descriptor = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (descriptor < 0) {
        return GATEWAY_ERROR_SYSTEM;
    }
    if (set_close_on_exec(descriptor) != 0 ||
        gateway_can_receiver_configure_socket(descriptor) != GATEWAY_OK) {
        int saved_errno = errno;
        (void)close(descriptor);
        errno = saved_errno;
        return GATEWAY_ERROR_SYSTEM;
    }

    errno = 0;
    interface_index = if_nametoindex(interface_name);
    if (interface_index == 0) {
        int saved_errno = errno == 0 ? ENODEV : errno;
        (void)close(descriptor);
        errno = saved_errno;
        return GATEWAY_ERROR_SYSTEM;
    }
    (void)memset(&address, 0, sizeof(address));
    address.can_family = AF_CAN;
    address.can_ifindex = (int)interface_index;
    if (bind(descriptor, (const struct sockaddr *)&address, sizeof(address)) !=
        0) {
        int saved_errno = errno;
        (void)close(descriptor);
        errno = saved_errno;
        return GATEWAY_ERROR_SYSTEM;
    }
    receiver->fd = descriptor;
    return GATEWAY_OK;
}

void gateway_can_receiver_close(gateway_can_receiver *receiver)
{
    if (receiver != NULL && receiver->fd >= 0) {
        (void)close(receiver->fd);
        receiver->fd = -1;
    }
}

gateway_error_code gateway_can_receiver_receive(
    gateway_can_receiver *receiver,
    int timeout_ms,
    uint64_t gateway_seq,
    telemetry_record *record,
    gateway_can_reject_reason *reject_reason)
{
    struct can_frame frame;
    struct iovec vector;
    struct msghdr header;
    struct pollfd poll_descriptor;
    struct cmsghdr *control_message;
    struct timespec timestamp;
    int64_t timestamp_ns;
    bool timestamp_found = false;
    ssize_t received;
    int poll_status;
    union {
        struct cmsghdr alignment;
        unsigned char bytes[CMSG_SPACE(sizeof(struct timespec))];
    } control;

    set_reject_reason(reject_reason, GATEWAY_CAN_REJECT_NONE);
    if (receiver == NULL || receiver->fd < 0 || timeout_ms < -1 ||
        record == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }

    poll_descriptor.fd = receiver->fd;
    poll_descriptor.events = POLLIN;
    poll_descriptor.revents = 0;
    do {
        poll_status = poll(&poll_descriptor, 1, timeout_ms);
    } while (poll_status < 0 && errno == EINTR);
    if (poll_status == 0) {
        return GATEWAY_ERROR_TIMEOUT;
    }
    if (poll_status < 0 || (poll_descriptor.revents & POLLIN) == 0) {
        return GATEWAY_ERROR_SYSTEM;
    }

    (void)memset(&frame, 0, sizeof(frame));
    (void)memset(&header, 0, sizeof(header));
    (void)memset(&control, 0, sizeof(control));
    vector.iov_base = &frame;
    vector.iov_len = sizeof(frame);
    header.msg_iov = &vector;
    header.msg_iovlen = 1;
    header.msg_control = control.bytes;
    header.msg_controllen = sizeof(control.bytes);
    do {
        received = recvmsg(receiver->fd, &header, 0);
    } while (received < 0 && errno == EINTR);
    if (received < 0) {
        return GATEWAY_ERROR_SYSTEM;
    }
    if (received != (ssize_t)sizeof(frame) ||
        (header.msg_flags & MSG_TRUNC) != 0) {
        set_reject_reason(reject_reason,
                          GATEWAY_CAN_REJECT_DATAGRAM_LENGTH);
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    if (!gateway_can_is_target_id(frame.can_id)) {
        set_reject_reason(reject_reason, GATEWAY_CAN_REJECT_ID);
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    if (frame.can_dlc != GATEWAY_CAN_DATA_SIZE) {
        set_reject_reason(reject_reason, GATEWAY_CAN_REJECT_DLC);
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    if ((header.msg_flags & MSG_CTRUNC) != 0) {
        set_reject_reason(reject_reason,
                          GATEWAY_CAN_REJECT_CONTROL_TRUNCATED);
        return GATEWAY_ERROR_INVALID_VALUE;
    }

    for (control_message = CMSG_FIRSTHDR(&header); control_message != NULL;
         control_message = CMSG_NXTHDR(&header, control_message)) {
        if (control_message->cmsg_level == SOL_SOCKET &&
            /* 旧版严格 POSIX 头文件未必公开 SCM_ 别名，两者常量值相同。 */
            control_message->cmsg_type == SO_TIMESTAMPNS &&
            control_message->cmsg_len >= CMSG_LEN(sizeof(timestamp))) {
            (void)memcpy(&timestamp, CMSG_DATA(control_message),
                         sizeof(timestamp));
            timestamp_found = true;
            break;
        }
    }
    if (!timestamp_found) {
        set_reject_reason(reject_reason,
                          GATEWAY_CAN_REJECT_TIMESTAMP_MISSING);
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    if (timestamp_to_nanoseconds(&timestamp, &timestamp_ns) != GATEWAY_OK) {
        set_reject_reason(reject_reason,
                          GATEWAY_CAN_REJECT_TIMESTAMP_INVALID);
        return GATEWAY_ERROR_INVALID_VALUE;
    }

    (void)memset(record, 0, sizeof(*record));
    record->gateway_seq = gateway_seq;
    record->kernel_timestamp_ns = timestamp_ns;
    record->can_id = frame.can_id;
    (void)memcpy(record->data, frame.data, sizeof(record->data));
    record->status_flags = GATEWAY_RECORD_STATUS_KERNEL_TIMESTAMP_VALID;
    record->dlc = frame.can_dlc;
    record->ecu_counter = frame.data[6];
    return GATEWAY_OK;
}
