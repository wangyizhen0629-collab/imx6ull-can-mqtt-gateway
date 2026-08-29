#include "gateway/can_receiver.h"

#include <linux/can.h>
#include <linux/can/raw.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,         \
                    __LINE__, #condition);                                     \
            return 1;                                                          \
        }                                                                      \
    } while (0)

static int create_timestamped_pair(int descriptors[2], bool timestamp_enabled)
{
    int enabled = 1;

    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, descriptors) != 0) {
        return -1;
    }
    if (timestamp_enabled &&
        setsockopt(descriptors[1], SOL_SOCKET, SO_TIMESTAMPNS, &enabled,
                   sizeof(enabled)) != 0) {
        (void)close(descriptors[0]);
        (void)close(descriptors[1]);
        return -1;
    }
    return 0;
}

static int send_frame(int descriptor, canid_t can_id, uint8_t dlc)
{
    struct can_frame frame = {
        .can_id = can_id,
        .can_dlc = dlc,
        .data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x7a, 0x7b},
    };

    return send(descriptor, &frame, sizeof(frame), 0) == (ssize_t)sizeof(frame)
               ? 0
               : -1;
}

static int test_kernel_socket_options(void)
{
    static const canid_t expected_ids[GATEWAY_CAN_TARGET_ID_COUNT] = {
        GATEWAY_CAN_ID_VEHICLE_DYNAMICS,
        GATEWAY_CAN_ID_POWER_STATUS,
        GATEWAY_CAN_ID_BODY_STATUS,
    };
    struct can_filter filters[GATEWAY_CAN_TARGET_ID_COUNT];
    socklen_t filters_size = sizeof(filters);
    socklen_t enabled_size;
    int descriptor;
    int enabled = 0;
    size_t index;

    descriptor = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    CHECK(descriptor >= 0);
    CHECK(gateway_can_receiver_configure_socket(descriptor) == GATEWAY_OK);
    CHECK(getsockopt(descriptor, SOL_CAN_RAW, CAN_RAW_FILTER, filters,
                     &filters_size) == 0);
    CHECK(filters_size == sizeof(filters));
    for (index = 0; index < GATEWAY_CAN_TARGET_ID_COUNT; index++) {
        CHECK(filters[index].can_id == expected_ids[index]);
        CHECK(filters[index].can_mask ==
              (CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG));
    }
    enabled_size = sizeof(enabled);
    CHECK(getsockopt(descriptor, SOL_SOCKET, SO_TIMESTAMPNS, &enabled,
                     &enabled_size) == 0);
    CHECK(enabled_size == sizeof(enabled));
    CHECK(enabled != 0);
    CHECK(close(descriptor) == 0);
    return 0;
}

static int test_valid_frame_and_timestamp(void)
{
    gateway_can_receiver receiver;
    gateway_can_reject_reason reason = GATEWAY_CAN_REJECT_ID;
    telemetry_record record;
    int descriptors[2];

    CHECK(create_timestamped_pair(descriptors, true) == 0);
    receiver.fd = descriptors[1];
    CHECK(send_frame(descriptors[0], GATEWAY_CAN_ID_VEHICLE_DYNAMICS, 8) == 0);
    CHECK(gateway_can_receiver_receive(&receiver, 1000, 42, &record, &reason) ==
          GATEWAY_OK);
    CHECK(reason == GATEWAY_CAN_REJECT_NONE);
    CHECK(record.gateway_seq == 42);
    CHECK(record.kernel_timestamp_ns > 0);
    CHECK(record.can_id == GATEWAY_CAN_ID_VEHICLE_DYNAMICS);
    CHECK(record.dlc == 8);
    CHECK(record.ecu_counter == 0x7a);
    CHECK(record.status_flags ==
          GATEWAY_RECORD_STATUS_KERNEL_TIMESTAMP_VALID);
    CHECK(memcmp(record.data,
                 (const uint8_t[]){0x01, 0x02, 0x03, 0x04,
                                   0x05, 0x06, 0x7a, 0x7b},
                 sizeof(record.data)) == 0);
    CHECK(close(descriptors[0]) == 0);
    CHECK(close(descriptors[1]) == 0);
    return 0;
}

static int check_rejected_frame(canid_t can_id,
                                uint8_t dlc,
                                gateway_can_reject_reason expected_reason)
{
    gateway_can_receiver receiver;
    gateway_can_reject_reason reason = GATEWAY_CAN_REJECT_NONE;
    telemetry_record record;
    int descriptors[2];

    CHECK(create_timestamped_pair(descriptors, true) == 0);
    receiver.fd = descriptors[1];
    CHECK(send_frame(descriptors[0], can_id, dlc) == 0);
    CHECK(gateway_can_receiver_receive(&receiver, 1000, 1, &record, &reason) ==
          GATEWAY_ERROR_INVALID_VALUE);
    CHECK(reason == expected_reason);
    CHECK(close(descriptors[0]) == 0);
    CHECK(close(descriptors[1]) == 0);
    return 0;
}

static int test_id_and_dlc_rejection(void)
{
    CHECK(check_rejected_frame(0x123, 8, GATEWAY_CAN_REJECT_ID) == 0);
    CHECK(check_rejected_frame(CAN_EFF_FLAG |
                                   GATEWAY_CAN_ID_VEHICLE_DYNAMICS,
                               8, GATEWAY_CAN_REJECT_ID) == 0);
    CHECK(check_rejected_frame(CAN_RTR_FLAG |
                                   GATEWAY_CAN_ID_VEHICLE_DYNAMICS,
                               8, GATEWAY_CAN_REJECT_ID) == 0);
    CHECK(check_rejected_frame(GATEWAY_CAN_ID_POWER_STATUS, 7,
                               GATEWAY_CAN_REJECT_DLC) == 0);
    return 0;
}

static int test_datagram_length_rejection(void)
{
    gateway_can_receiver receiver;
    gateway_can_reject_reason reason = GATEWAY_CAN_REJECT_NONE;
    telemetry_record record;
    struct can_frame frame = {0};
    unsigned char oversized[sizeof(frame) + 1] = {0};
    int descriptors[2];

    CHECK(create_timestamped_pair(descriptors, true) == 0);
    receiver.fd = descriptors[1];
    CHECK(send(descriptors[0], &frame, sizeof(frame) - 1, 0) ==
          (ssize_t)(sizeof(frame) - 1));
    CHECK(gateway_can_receiver_receive(&receiver, 1000, 1, &record, &reason) ==
          GATEWAY_ERROR_INVALID_VALUE);
    CHECK(reason == GATEWAY_CAN_REJECT_DATAGRAM_LENGTH);

    reason = GATEWAY_CAN_REJECT_NONE;
    CHECK(send(descriptors[0], oversized, sizeof(oversized), 0) ==
          (ssize_t)sizeof(oversized));
    CHECK(gateway_can_receiver_receive(&receiver, 1000, 1, &record, &reason) ==
          GATEWAY_ERROR_INVALID_VALUE);
    CHECK(reason == GATEWAY_CAN_REJECT_DATAGRAM_LENGTH);
    CHECK(close(descriptors[0]) == 0);
    CHECK(close(descriptors[1]) == 0);
    return 0;
}

static int test_missing_timestamp_and_timeout(void)
{
    gateway_can_receiver receiver;
    gateway_can_reject_reason reason = GATEWAY_CAN_REJECT_NONE;
    telemetry_record record;
    int descriptors[2];

    CHECK(create_timestamped_pair(descriptors, false) == 0);
    receiver.fd = descriptors[1];
    CHECK(send_frame(descriptors[0], GATEWAY_CAN_ID_BODY_STATUS, 8) == 0);
    CHECK(gateway_can_receiver_receive(&receiver, 1000, 1, &record, &reason) ==
          GATEWAY_ERROR_INVALID_VALUE);
    CHECK(reason == GATEWAY_CAN_REJECT_TIMESTAMP_MISSING);
    reason = GATEWAY_CAN_REJECT_ID;
    CHECK(gateway_can_receiver_receive(&receiver, 10, 1, &record, &reason) ==
          GATEWAY_ERROR_TIMEOUT);
    CHECK(reason == GATEWAY_CAN_REJECT_NONE);
    CHECK(close(descriptors[0]) == 0);
    CHECK(close(descriptors[1]) == 0);
    return 0;
}

int main(void)
{
    gateway_can_receiver receiver;

    gateway_can_receiver_init(&receiver);
    CHECK(receiver.fd == -1);
    CHECK(gateway_can_receiver_open(NULL, "can0") == GATEWAY_ERROR_ARGUMENT);
    CHECK(gateway_can_receiver_open(&receiver, NULL) == GATEWAY_ERROR_ARGUMENT);
    CHECK(gateway_can_receiver_open(&receiver, "") == GATEWAY_ERROR_ARGUMENT);
    CHECK(gateway_can_receiver_configure_socket(-1) ==
          GATEWAY_ERROR_ARGUMENT);
    CHECK(gateway_can_is_target_id(GATEWAY_CAN_ID_VEHICLE_DYNAMICS));
    CHECK(gateway_can_is_target_id(GATEWAY_CAN_ID_POWER_STATUS));
    CHECK(gateway_can_is_target_id(GATEWAY_CAN_ID_BODY_STATUS));
    CHECK(!gateway_can_is_target_id(0x123));
    CHECK(strcmp(gateway_can_reject_reason_name(GATEWAY_CAN_REJECT_DLC),
                 "dlc") == 0);
    CHECK(test_kernel_socket_options() == 0);
    CHECK(test_valid_frame_and_timestamp() == 0);
    CHECK(test_id_and_dlc_rejection() == 0);
    CHECK(test_datagram_length_rejection() == 0);
    CHECK(test_missing_timestamp_and_timeout() == 0);
    return 0;
}
