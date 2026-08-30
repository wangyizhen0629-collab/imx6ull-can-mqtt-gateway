#ifndef GATEWAY_VEHICLE_DECODER_H
#define GATEWAY_VEHICLE_DECODER_H

#include "gateway/telemetry_record.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    GATEWAY_DECODED_SCHEMA_VERSION = 1,
    GATEWAY_VEHICLE_SIGNAL_MASK = 0x0f,
    GATEWAY_POWER_SIGNAL_MASK = 0x0f,
    GATEWAY_BODY_SIGNAL_MASK = 0x07
};

typedef enum {
    GATEWAY_MESSAGE_TYPE_NONE = 0,
    GATEWAY_MESSAGE_TYPE_VEHICLE_DYNAMICS = 1,
    GATEWAY_MESSAGE_TYPE_POWER_STATUS = 2,
    GATEWAY_MESSAGE_TYPE_BODY_STATUS = 3
} gateway_message_type;

typedef enum {
    GATEWAY_DECODE_OK = 0,
    GATEWAY_DECODE_INVALID_ARGUMENT,
    GATEWAY_DECODE_UNSUPPORTED_ID,
    GATEWAY_DECODE_INVALID_DLC,
    GATEWAY_DECODE_CHECKSUM_MISMATCH
} gateway_decode_result;

typedef struct {
    uint32_t vehicle_speed_centi_kph;
    uint32_t engine_speed_quarter_rpm;
    uint16_t throttle_tenth_percent;
    uint8_t gear;
    uint8_t reserved[13];
} gateway_vehicle_dynamics_signals;

typedef struct {
    uint32_t battery_millivolt;
    int16_t coolant_celsius;
    uint16_t soc_tenth_percent;
    uint16_t fault_flags;
    uint8_t reserved[14];
} gateway_power_status_signals;

typedef struct {
    uint32_t odometer_tenth_km;
    uint8_t door_flags;
    uint8_t ignition_state;
    uint8_t reserved[18];
} gateway_body_status_signals;

typedef union {
    gateway_vehicle_dynamics_signals vehicle_dynamics;
    gateway_power_status_signals power_status;
    gateway_body_status_signals body_status;
    uint8_t bytes[24];
} gateway_decoded_signals;

/*
 * 物理量使用显式定点单位，避免在 CAN 接收热路径引入浮点和平台相关布局。
 * 该结构通过 memcpy 写入 telemetry_record 的 32 字节解码区，禁止直接强制转换。
 */
typedef struct {
    uint8_t schema_version;
    uint8_t message_type;
    uint8_t rolling_counter;
    uint8_t checksum;
    uint32_t valid_signal_mask;
    gateway_decoded_signals signals;
} gateway_decoded_payload;

_Static_assert(sizeof(gateway_vehicle_dynamics_signals) == 24,
               "vehicle signal payload must remain 24 bytes");
_Static_assert(sizeof(gateway_power_status_signals) == 24,
               "power signal payload must remain 24 bytes");
_Static_assert(sizeof(gateway_body_status_signals) == 24,
               "body signal payload must remain 24 bytes");
_Static_assert(sizeof(gateway_decoded_payload) == GATEWAY_DECODED_PAYLOAD_SIZE,
               "decoded payload must fit telemetry_record");

gateway_decode_result gateway_vehicle_decode_frame(
    uint32_t can_id,
    uint8_t dlc,
    const uint8_t data[GATEWAY_CAN_DATA_SIZE],
    gateway_decoded_payload *decoded);

gateway_decode_result gateway_vehicle_decode_record(telemetry_record *record);

bool gateway_vehicle_read_decoded_payload(
    const telemetry_record *record,
    gateway_decoded_payload *decoded);

const char *gateway_decode_result_name(gateway_decode_result result);

#endif
