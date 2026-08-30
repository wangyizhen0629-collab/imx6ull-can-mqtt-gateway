#ifndef GATEWAY_TELEMETRY_RECORD_H
#define GATEWAY_TELEMETRY_RECORD_H

#include <stddef.h>
#include <stdint.h>

enum {
    GATEWAY_CAN_DATA_SIZE = 8,
    GATEWAY_DECODED_PAYLOAD_SIZE = 32,
    GATEWAY_TELEMETRY_RECORD_SIZE = 64,
    GATEWAY_RECORD_STATUS_KERNEL_TIMESTAMP_VALID = 1U << 0,
    GATEWAY_RECORD_STATUS_CHECKSUM_VALID = 1U << 1,
    GATEWAY_RECORD_STATUS_DECODED_VALID = 1U << 2
};

typedef struct {
    uint64_t gateway_seq;
    int64_t kernel_timestamp_ns;
    uint32_t can_id;
    uint8_t data[GATEWAY_CAN_DATA_SIZE];
    uint16_t status_flags;
    uint8_t dlc;
    uint8_t ecu_counter;
    /* M4 定义为 gateway_decoded_payload 的 memcpy 表示，禁止指针强制转换。 */
    uint8_t decoded_payload[GATEWAY_DECODED_PAYLOAD_SIZE];
} telemetry_record;

_Static_assert(sizeof(telemetry_record) == GATEWAY_TELEMETRY_RECORD_SIZE,
               "telemetry_record must remain 64 bytes");
_Static_assert(offsetof(telemetry_record, gateway_seq) == 0,
               "gateway_seq offset changed");
_Static_assert(offsetof(telemetry_record, kernel_timestamp_ns) == 8,
               "kernel timestamp offset changed");
_Static_assert(offsetof(telemetry_record, can_id) == 16,
               "CAN ID offset changed");
_Static_assert(offsetof(telemetry_record, data) == 20,
               "CAN data offset changed");
_Static_assert(offsetof(telemetry_record, decoded_payload) == 32,
               "decoded payload offset changed");

#endif
