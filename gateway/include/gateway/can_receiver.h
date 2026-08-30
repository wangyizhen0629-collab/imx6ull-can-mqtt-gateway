#ifndef GATEWAY_CAN_RECEIVER_H
#define GATEWAY_CAN_RECEIVER_H

#include "gateway/error.h"
#include "gateway/telemetry_record.h"
#include "gateway/vehicle_protocol.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    GATEWAY_CAN_REJECT_NONE = 0,
    GATEWAY_CAN_REJECT_DATAGRAM_LENGTH,
    GATEWAY_CAN_REJECT_DLC,
    GATEWAY_CAN_REJECT_ID,
    GATEWAY_CAN_REJECT_CONTROL_TRUNCATED,
    GATEWAY_CAN_REJECT_TIMESTAMP_MISSING,
    GATEWAY_CAN_REJECT_TIMESTAMP_INVALID
} gateway_can_reject_reason;

typedef struct {
    int fd;
} gateway_can_receiver;

void gateway_can_receiver_init(gateway_can_receiver *receiver);
gateway_error_code gateway_can_receiver_open(gateway_can_receiver *receiver,
                                             const char *interface_name);
void gateway_can_receiver_close(gateway_can_receiver *receiver);

/*
 * 将精确标准帧过滤器和 SO_TIMESTAMPNS 应用到已创建的 CAN_RAW socket。
 * 独立暴露该步骤，便于在不修改任何接口状态时核验内核 socket 选项。
 */
gateway_error_code gateway_can_receiver_configure_socket(int descriptor);

gateway_error_code gateway_can_receiver_receive(
    gateway_can_receiver *receiver,
    int timeout_ms,
    uint64_t gateway_seq,
    telemetry_record *record,
    gateway_can_reject_reason *reject_reason);

bool gateway_can_is_target_id(uint32_t can_id);
const char *gateway_can_reject_reason_name(gateway_can_reject_reason reason);

#endif
