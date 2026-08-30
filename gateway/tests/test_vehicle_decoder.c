#include "gateway/vehicle_decoder.h"
#include "gateway/vehicle_protocol.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GATEWAY_GOLDEN_VECTOR_PATH
#error "GATEWAY_GOLDEN_VECTOR_PATH must be defined"
#endif

enum {
    VECTOR_COLUMN_COUNT = 18,
    EXPECTED_VECTOR_COUNT = 20
};

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,         \
                    __LINE__, #condition);                                     \
            return 1;                                                          \
        }                                                                      \
    } while (0)

static int hex_nibble(char character)
{
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

static int parse_hex_payload(const char *text,
                             uint8_t payload[GATEWAY_CAN_DATA_SIZE])
{
    size_t index;

    if (strlen(text) != GATEWAY_CAN_DATA_SIZE * 2U) {
        return -1;
    }
    for (index = 0; index < GATEWAY_CAN_DATA_SIZE; index++) {
        int high = hex_nibble(text[index * 2U]);
        int low = hex_nibble(text[index * 2U + 1U]);

        if (high < 0 || low < 0) {
            return -1;
        }
        payload[index] = (uint8_t)((high << 4) | low);
    }
    return 0;
}

static int parse_u32(const char *text, uint32_t *value)
{
    unsigned long parsed;
    char *end = NULL;

    if (text == NULL || value == NULL || text[0] == '\0' || text[0] == '-') {
        return -1;
    }
    errno = 0;
    parsed = strtoul(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX) {
        return -1;
    }
    *value = (uint32_t)parsed;
    return 0;
}

static int parse_i32(const char *text, int32_t *value)
{
    long parsed;
    char *end = NULL;

    if (text == NULL || value == NULL || text[0] == '\0') {
        return -1;
    }
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < INT32_MIN ||
        parsed > INT32_MAX) {
        return -1;
    }
    *value = (int32_t)parsed;
    return 0;
}

static int expect_u32(const char *text, uint32_t actual)
{
    uint32_t expected;

    return parse_u32(text, &expected) == 0 && expected == actual ? 0 : -1;
}

static int expect_i32(const char *text, int32_t actual)
{
    int32_t expected;

    return parse_i32(text, &expected) == 0 && expected == actual ? 0 : -1;
}

static gateway_decode_result parse_result(const char *text)
{
    if (strcmp(text, "OK") == 0) {
        return GATEWAY_DECODE_OK;
    }
    if (strcmp(text, "CHECKSUM") == 0) {
        return GATEWAY_DECODE_CHECKSUM_MISMATCH;
    }
    if (strcmp(text, "ID") == 0) {
        return GATEWAY_DECODE_UNSUPPORTED_ID;
    }
    if (strcmp(text, "DLC") == 0) {
        return GATEWAY_DECODE_INVALID_DLC;
    }
    return GATEWAY_DECODE_INVALID_ARGUMENT;
}

static int verify_unrelated_fields(char *fields[VECTOR_COLUMN_COUNT],
                                   size_t first,
                                   size_t last)
{
    size_t index;

    for (index = first; index <= last; index++) {
        if (strcmp(fields[index], "NA") != 0) {
            return -1;
        }
    }
    return 0;
}

static int verify_success(char *fields[VECTOR_COLUMN_COUNT],
                          uint32_t can_id,
                          const gateway_decoded_payload *decoded)
{
    CHECK(decoded->schema_version == GATEWAY_DECODED_SCHEMA_VERSION);
    CHECK(expect_u32(fields[16], decoded->rolling_counter) == 0);
    CHECK(expect_u32(fields[17], decoded->checksum) == 0);

    switch (can_id) {
    case GATEWAY_CAN_ID_VEHICLE_DYNAMICS:
        CHECK(decoded->message_type ==
              GATEWAY_MESSAGE_TYPE_VEHICLE_DYNAMICS);
        CHECK(decoded->valid_signal_mask == GATEWAY_VEHICLE_SIGNAL_MASK);
        CHECK(expect_u32(fields[5], decoded->signals.vehicle_dynamics
                                        .vehicle_speed_centi_kph) == 0);
        CHECK(expect_u32(fields[6], decoded->signals.vehicle_dynamics
                                        .engine_speed_quarter_rpm) == 0);
        CHECK(expect_u32(fields[7], decoded->signals.vehicle_dynamics
                                        .throttle_tenth_percent) == 0);
        CHECK(expect_u32(fields[8],
                         decoded->signals.vehicle_dynamics.gear) == 0);
        CHECK(verify_unrelated_fields(fields, 9, 15) == 0);
        break;
    case GATEWAY_CAN_ID_POWER_STATUS:
        CHECK(decoded->message_type == GATEWAY_MESSAGE_TYPE_POWER_STATUS);
        CHECK(decoded->valid_signal_mask == GATEWAY_POWER_SIGNAL_MASK);
        CHECK(verify_unrelated_fields(fields, 5, 8) == 0);
        CHECK(expect_u32(fields[9], decoded->signals.power_status
                                        .battery_millivolt) == 0);
        CHECK(expect_i32(fields[10], decoded->signals.power_status
                                         .coolant_celsius) == 0);
        CHECK(expect_u32(fields[11], decoded->signals.power_status
                                         .soc_tenth_percent) == 0);
        CHECK(expect_u32(fields[12],
                         decoded->signals.power_status.fault_flags) == 0);
        CHECK(verify_unrelated_fields(fields, 13, 15) == 0);
        break;
    case GATEWAY_CAN_ID_BODY_STATUS:
        CHECK(decoded->message_type == GATEWAY_MESSAGE_TYPE_BODY_STATUS);
        CHECK(decoded->valid_signal_mask == GATEWAY_BODY_SIGNAL_MASK);
        CHECK(verify_unrelated_fields(fields, 5, 12) == 0);
        CHECK(expect_u32(fields[13], decoded->signals.body_status
                                         .odometer_tenth_km) == 0);
        CHECK(expect_u32(fields[14],
                         decoded->signals.body_status.door_flags) == 0);
        CHECK(expect_u32(fields[15],
                         decoded->signals.body_status.ignition_state) == 0);
        break;
    default:
        CHECK(0);
    }
    return 0;
}

static int test_golden_vectors(void)
{
    static const char header_prefix[] =
        "name,result,can_id,dlc,data_hex,";
    char line[1024];
    FILE *stream = fopen(GATEWAY_GOLDEN_VECTOR_PATH, "r");
    size_t vector_count = 0;

    CHECK(stream != NULL);
    CHECK(fgets(line, sizeof(line), stream) != NULL);
    CHECK(strncmp(line, header_prefix, strlen(header_prefix)) == 0);

    while (fgets(line, sizeof(line), stream) != NULL) {
        gateway_decoded_payload decoded;
        gateway_decoded_payload from_record;
        gateway_decoded_payload zero = {0};
        telemetry_record record;
        gateway_decode_result expected_result;
        gateway_decode_result result;
        char *fields[VECTOR_COLUMN_COUNT];
        char *save = NULL;
        char *token;
        uint8_t payload[GATEWAY_CAN_DATA_SIZE];
        uint32_t can_id;
        uint32_t dlc;
        size_t field_count = 0;
        size_t index;

        line[strcspn(line, "\r\n")] = '\0';
        token = strtok_r(line, ",", &save);
        while (token != NULL && field_count < VECTOR_COLUMN_COUNT) {
            fields[field_count++] = token;
            token = strtok_r(NULL, ",", &save);
        }
        CHECK(field_count == VECTOR_COLUMN_COUNT);
        CHECK(token == NULL);
        CHECK(parse_u32(fields[2], &can_id) == 0);
        CHECK(parse_u32(fields[3], &dlc) == 0 && dlc <= UINT8_MAX);
        CHECK(parse_hex_payload(fields[4], payload) == 0);
        expected_result = parse_result(fields[1]);
        CHECK(expected_result != GATEWAY_DECODE_INVALID_ARGUMENT);

        (void)memset(&decoded, 0xa5, sizeof(decoded));
        result = gateway_vehicle_decode_frame(can_id, (uint8_t)dlc, payload,
                                              &decoded);
        CHECK(result == expected_result);
        CHECK(strcmp(gateway_decode_result_name(result),
                     gateway_decode_result_name(expected_result)) == 0);

        (void)memset(&record, 0, sizeof(record));
        record.can_id = can_id;
        record.dlc = (uint8_t)dlc;
        record.status_flags = GATEWAY_RECORD_STATUS_KERNEL_TIMESTAMP_VALID |
                              GATEWAY_RECORD_STATUS_CHECKSUM_VALID |
                              GATEWAY_RECORD_STATUS_DECODED_VALID;
        (void)memset(record.decoded_payload, 0xa5,
                     sizeof(record.decoded_payload));
        (void)memcpy(record.data, payload, sizeof(record.data));
        CHECK(gateway_vehicle_decode_record(&record) == expected_result);
        CHECK((record.status_flags &
               GATEWAY_RECORD_STATUS_KERNEL_TIMESTAMP_VALID) != 0);

        if (result == GATEWAY_DECODE_OK) {
            CHECK(verify_success(fields, can_id, &decoded) == 0);
            CHECK((record.status_flags &
                   GATEWAY_RECORD_STATUS_CHECKSUM_VALID) != 0);
            CHECK((record.status_flags &
                   GATEWAY_RECORD_STATUS_DECODED_VALID) != 0);
            CHECK(record.ecu_counter == decoded.rolling_counter);
            CHECK(gateway_vehicle_read_decoded_payload(&record,
                                                       &from_record));
            CHECK(memcmp(&decoded, &from_record, sizeof(decoded)) == 0);
        } else {
            CHECK(memcmp(&decoded, &zero, sizeof(decoded)) == 0);
            CHECK((record.status_flags &
                   (GATEWAY_RECORD_STATUS_CHECKSUM_VALID |
                    GATEWAY_RECORD_STATUS_DECODED_VALID)) == 0);
            for (index = 0; index < sizeof(record.decoded_payload); index++) {
                CHECK(record.decoded_payload[index] == 0);
            }
            CHECK(!gateway_vehicle_read_decoded_payload(&record,
                                                        &from_record));
        }
        vector_count++;
    }
    CHECK(ferror(stream) == 0);
    CHECK(fclose(stream) == 0);
    CHECK(vector_count == EXPECTED_VECTOR_COUNT);
    return 0;
}

static int test_stm32_pattern_all_counters(void)
{
    static const struct {
        uint32_t can_id;
        uint8_t base;
    } messages[] = {
        {GATEWAY_CAN_ID_VEHICLE_DYNAMICS, 0x10},
        {GATEWAY_CAN_ID_POWER_STATUS, 0x20},
        {GATEWAY_CAN_ID_BODY_STATUS, 0x30},
    };
    size_t message_index;
    unsigned int counter;

    for (message_index = 0;
         message_index < sizeof(messages) / sizeof(messages[0]);
         message_index++) {
        for (counter = 0; counter <= UINT8_MAX; counter++) {
            gateway_decoded_payload decoded;
            uint8_t payload[GATEWAY_CAN_DATA_SIZE];
            uint8_t checksum = 0;
            size_t byte_index;

            for (byte_index = 0; byte_index < 6; byte_index++) {
                payload[byte_index] =
                    (uint8_t)(messages[message_index].base + counter +
                              byte_index);
                checksum ^= payload[byte_index];
            }
            payload[6] = (uint8_t)counter;
            checksum ^= payload[6];
            payload[7] = checksum;
            CHECK(gateway_vehicle_decode_frame(
                      messages[message_index].can_id, GATEWAY_CAN_DATA_SIZE,
                      payload, &decoded) == GATEWAY_DECODE_OK);
            CHECK(decoded.rolling_counter == (uint8_t)counter);
            CHECK(decoded.checksum == checksum);
            switch (messages[message_index].can_id) {
            case GATEWAY_CAN_ID_VEHICLE_DYNAMICS:
                CHECK(decoded.signals.vehicle_dynamics
                          .vehicle_speed_centi_kph ==
                      ((uint32_t)payload[0] |
                       ((uint32_t)payload[1] << 8)));
                CHECK(decoded.signals.vehicle_dynamics
                          .engine_speed_quarter_rpm ==
                      ((uint32_t)payload[2] |
                       ((uint32_t)payload[3] << 8)));
                CHECK(decoded.signals.vehicle_dynamics
                          .throttle_tenth_percent ==
                      (uint16_t)payload[4] * 4U);
                CHECK(decoded.signals.vehicle_dynamics.gear == payload[5]);
                break;
            case GATEWAY_CAN_ID_POWER_STATUS:
                CHECK(decoded.signals.power_status.battery_millivolt ==
                      ((uint32_t)payload[0] |
                       ((uint32_t)payload[1] << 8)));
                CHECK(decoded.signals.power_status.coolant_celsius ==
                      (int16_t)payload[2] - 40);
                CHECK(decoded.signals.power_status.soc_tenth_percent ==
                      (uint16_t)payload[3] * 4U);
                CHECK(decoded.signals.power_status.fault_flags ==
                      ((uint16_t)payload[4] |
                       ((uint16_t)payload[5] << 8)));
                break;
            case GATEWAY_CAN_ID_BODY_STATUS:
                CHECK(decoded.signals.body_status.odometer_tenth_km ==
                      ((uint32_t)payload[0] |
                       ((uint32_t)payload[1] << 8) |
                       ((uint32_t)payload[2] << 16)));
                CHECK(decoded.signals.body_status.door_flags ==
                      (payload[3] & UINT8_C(0x0f)));
                CHECK(decoded.signals.body_status.ignition_state ==
                      (payload[4] & UINT8_C(0x03)));
                break;
            default:
                CHECK(0);
            }
        }
    }
    return 0;
}

static int test_arguments(void)
{
    gateway_decoded_payload decoded;
    gateway_decoded_payload zero = {0};
    telemetry_record record = {0};
    uint8_t payload[GATEWAY_CAN_DATA_SIZE] = {0};

    (void)memset(&decoded, 0xa5, sizeof(decoded));
    CHECK(gateway_vehicle_decode_frame(0x100, 8, NULL, &decoded) ==
          GATEWAY_DECODE_INVALID_ARGUMENT);
    CHECK(memcmp(&decoded, &zero, sizeof(decoded)) == 0);
    CHECK(gateway_vehicle_decode_frame(0x100, 8, payload, NULL) ==
          GATEWAY_DECODE_INVALID_ARGUMENT);
    CHECK(gateway_vehicle_decode_record(NULL) ==
          GATEWAY_DECODE_INVALID_ARGUMENT);
    CHECK(!gateway_vehicle_read_decoded_payload(NULL, &decoded));
    CHECK(!gateway_vehicle_read_decoded_payload(&record, NULL));
    CHECK(strcmp(gateway_decode_result_name((gateway_decode_result)999),
                 "unknown") == 0);
    return 0;
}

int main(void)
{
    CHECK(test_arguments() == 0);
    CHECK(test_golden_vectors() == 0);
    CHECK(test_stm32_pattern_all_counters() == 0);
    return 0;
}
