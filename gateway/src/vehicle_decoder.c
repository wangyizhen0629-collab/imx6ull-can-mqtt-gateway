#include "gateway/vehicle_decoder.h"

#include "gateway/vehicle_protocol.h"

#include <string.h>

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t read_u24_le(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16);
}

static uint8_t calculate_xor(const uint8_t data[GATEWAY_CAN_DATA_SIZE])
{
    uint8_t checksum = 0;
    size_t index;

    for (index = 0; index < GATEWAY_CAN_DATA_SIZE - 1; index++) {
        checksum ^= data[index];
    }
    return checksum;
}

static void initialize_decoded_payload(gateway_decoded_payload *decoded,
                                       gateway_message_type message_type,
                                       const uint8_t data[GATEWAY_CAN_DATA_SIZE])
{
    (void)memset(decoded, 0, sizeof(*decoded));
    decoded->schema_version = GATEWAY_DECODED_SCHEMA_VERSION;
    decoded->message_type = (uint8_t)message_type;
    decoded->rolling_counter = data[6];
    decoded->checksum = data[7];
}

gateway_decode_result gateway_vehicle_decode_frame(
    uint32_t can_id,
    uint8_t dlc,
    const uint8_t data[GATEWAY_CAN_DATA_SIZE],
    gateway_decoded_payload *decoded)
{
    gateway_message_type message_type;

    if (decoded == NULL) {
        return GATEWAY_DECODE_INVALID_ARGUMENT;
    }
    (void)memset(decoded, 0, sizeof(*decoded));
    if (data == NULL) {
        return GATEWAY_DECODE_INVALID_ARGUMENT;
    }
    switch (can_id) {
    case GATEWAY_CAN_ID_VEHICLE_DYNAMICS:
        message_type = GATEWAY_MESSAGE_TYPE_VEHICLE_DYNAMICS;
        break;
    case GATEWAY_CAN_ID_POWER_STATUS:
        message_type = GATEWAY_MESSAGE_TYPE_POWER_STATUS;
        break;
    case GATEWAY_CAN_ID_BODY_STATUS:
        message_type = GATEWAY_MESSAGE_TYPE_BODY_STATUS;
        break;
    default:
        return GATEWAY_DECODE_UNSUPPORTED_ID;
    }
    if (dlc != GATEWAY_CAN_DATA_SIZE) {
        return GATEWAY_DECODE_INVALID_DLC;
    }
    if (calculate_xor(data) != data[7]) {
        return GATEWAY_DECODE_CHECKSUM_MISMATCH;
    }

    initialize_decoded_payload(decoded, message_type, data);
    switch (message_type) {
    case GATEWAY_MESSAGE_TYPE_VEHICLE_DYNAMICS:
        decoded->valid_signal_mask = GATEWAY_VEHICLE_SIGNAL_MASK;
        decoded->signals.vehicle_dynamics.vehicle_speed_centi_kph =
            read_u16_le(&data[0]);
        decoded->signals.vehicle_dynamics.engine_speed_quarter_rpm =
            read_u16_le(&data[2]);
        decoded->signals.vehicle_dynamics.throttle_tenth_percent =
            (uint16_t)data[4] * 4U;
        decoded->signals.vehicle_dynamics.gear = data[5];
        break;
    case GATEWAY_MESSAGE_TYPE_POWER_STATUS:
        decoded->valid_signal_mask = GATEWAY_POWER_SIGNAL_MASK;
        decoded->signals.power_status.battery_millivolt =
            read_u16_le(&data[0]);
        decoded->signals.power_status.coolant_celsius =
            (int16_t)data[2] - 40;
        decoded->signals.power_status.soc_tenth_percent =
            (uint16_t)data[3] * 4U;
        decoded->signals.power_status.fault_flags = read_u16_le(&data[4]);
        break;
    case GATEWAY_MESSAGE_TYPE_BODY_STATUS:
        decoded->valid_signal_mask = GATEWAY_BODY_SIGNAL_MASK;
        decoded->signals.body_status.odometer_tenth_km =
            read_u24_le(&data[0]);
        decoded->signals.body_status.door_flags = data[3] & UINT8_C(0x0f);
        decoded->signals.body_status.ignition_state =
            data[4] & UINT8_C(0x03);
        break;
    case GATEWAY_MESSAGE_TYPE_NONE:
    default:
        return GATEWAY_DECODE_UNSUPPORTED_ID;
    }
    return GATEWAY_DECODE_OK;
}

gateway_decode_result gateway_vehicle_decode_record(telemetry_record *record)
{
    gateway_decoded_payload decoded;
    gateway_decode_result result;

    if (record == NULL) {
        return GATEWAY_DECODE_INVALID_ARGUMENT;
    }
    record->status_flags &=
        (uint16_t)~(GATEWAY_RECORD_STATUS_CHECKSUM_VALID |
                    GATEWAY_RECORD_STATUS_DECODED_VALID);
    (void)memset(record->decoded_payload, 0,
                 sizeof(record->decoded_payload));

    result = gateway_vehicle_decode_frame(record->can_id, record->dlc,
                                          record->data, &decoded);
    if (result != GATEWAY_DECODE_OK) {
        return result;
    }
    record->ecu_counter = decoded.rolling_counter;
    (void)memcpy(record->decoded_payload, &decoded, sizeof(decoded));
    record->status_flags |= GATEWAY_RECORD_STATUS_CHECKSUM_VALID |
                            GATEWAY_RECORD_STATUS_DECODED_VALID;
    return GATEWAY_DECODE_OK;
}

bool gateway_vehicle_read_decoded_payload(
    const telemetry_record *record,
    gateway_decoded_payload *decoded)
{
    if (record == NULL || decoded == NULL ||
        (record->status_flags & GATEWAY_RECORD_STATUS_DECODED_VALID) == 0) {
        return false;
    }
    (void)memcpy(decoded, record->decoded_payload, sizeof(*decoded));
    return true;
}

const char *gateway_decode_result_name(gateway_decode_result result)
{
    switch (result) {
    case GATEWAY_DECODE_OK:
        return "ok";
    case GATEWAY_DECODE_INVALID_ARGUMENT:
        return "invalid_argument";
    case GATEWAY_DECODE_UNSUPPORTED_ID:
        return "unsupported_id";
    case GATEWAY_DECODE_INVALID_DLC:
        return "invalid_dlc";
    case GATEWAY_DECODE_CHECKSUM_MISMATCH:
        return "checksum_mismatch";
    default:
        return "unknown";
    }
}
