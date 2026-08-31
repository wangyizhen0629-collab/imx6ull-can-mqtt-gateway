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
    EXPECTED_VECTOR_COUNT = 42,
    SEMANTIC_DRIVE_CYCLE_TICKS = 6000
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

static char *next_csv_field(char *text, char **context)
{
    char *field;
    char *separator;

    if (context == NULL) {
        return NULL;
    }
    field = text != NULL ? text : *context;
    if (field == NULL) {
        return NULL;
    }
    separator = strchr(field, ',');
    if (separator == NULL) {
        *context = NULL;
    } else {
        *separator = '\0';
        *context = separator + 1;
    }
    return field;
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
        token = next_csv_field(line, &save);
        while (token != NULL && field_count < VECTOR_COLUMN_COUNT) {
            fields[field_count++] = token;
            token = next_csv_field(NULL, &save);
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

typedef struct {
    uint16_t speed_centi_kph;
    uint16_t engine_quarter_rpm;
    uint16_t throttle_tenth_percent;
    uint16_t battery_millivolt;
    uint32_t odometer_tenth_km;
    int16_t coolant_celsius;
    uint16_t soc_tenth_percent;
    uint16_t fault_flags;
    uint8_t gear;
    uint8_t door_flags;
    uint8_t ignition_state;
} semantic_vehicle_state;

static void pack_u16_le(uint8_t *payload, size_t offset, uint16_t value)
{
    payload[offset] = (uint8_t)(value & UINT16_C(0x00ff));
    payload[offset + 1U] = (uint8_t)(value >> 8);
}

static void pack_u24_le(uint8_t *payload, size_t offset, uint32_t value)
{
    payload[offset] = (uint8_t)(value & UINT32_C(0x000000ff));
    payload[offset + 1U] =
        (uint8_t)((value >> 8) & UINT32_C(0x000000ff));
    payload[offset + 2U] =
        (uint8_t)((value >> 16) & UINT32_C(0x000000ff));
}

static void finalize_payload(uint8_t payload[GATEWAY_CAN_DATA_SIZE],
                             uint8_t counter)
{
    uint8_t checksum = 0;
    size_t index;

    payload[6] = counter;
    for (index = 0; index < GATEWAY_CAN_DATA_SIZE - 1U; index++) {
        checksum ^= payload[index];
    }
    payload[7] = checksum;
}

static uint8_t select_semantic_gear(uint16_t speed_centi_kph)
{
    if (speed_centi_kph <= 1200U) {
        return 1U;
    }
    if (speed_centi_kph <= 2500U) {
        return 2U;
    }
    if (speed_centi_kph <= 4000U) {
        return 3U;
    }
    return 4U;
}

static uint16_t calculate_semantic_engine_speed(uint16_t speed_centi_kph,
                                                uint8_t gear)
{
    uint32_t rpm;

    if (speed_centi_kph == 0U) {
        rpm = 800U;
    } else if (gear == 1U) {
        rpm = 900U + ((uint32_t)speed_centi_kph * 7U) / 5U;
    } else if (gear == 2U) {
        rpm = 900U + ((uint32_t)speed_centi_kph * 7U) / 10U;
    } else if (gear == 3U) {
        rpm = 900U + ((uint32_t)speed_centi_kph * 9U) / 20U;
    } else {
        rpm = 900U + (uint32_t)speed_centi_kph / 4U;
    }
    return (uint16_t)(rpm * 4U);
}

static void update_semantic_fast_state(semantic_vehicle_state *state,
                                       unsigned int phase)
{
    uint32_t ramp_tick;

    if (phase < 300U || phase >= 5800U) {
        state->speed_centi_kph = 0U;
        state->engine_quarter_rpm = 0U;
        state->throttle_tenth_percent = 0U;
        state->gear = 0U;
        state->door_flags = 1U;
        state->ignition_state = 0U;
    } else if (phase < 500U || phase >= 5500U) {
        state->speed_centi_kph = 0U;
        state->engine_quarter_rpm = 800U * 4U;
        state->throttle_tenth_percent = 0U;
        state->gear = 0U;
        state->door_flags = 0U;
        state->ignition_state = 2U;
    } else {
        if (phase < 2000U) {
            ramp_tick = phase - 500U;
            state->speed_centi_kph =
                (uint16_t)(ramp_tick * 6000U / 1500U);
            state->throttle_tenth_percent = 320U;
        } else if (phase < 4000U) {
            state->speed_centi_kph = 6000U;
            state->throttle_tenth_percent = 160U;
        } else {
            ramp_tick = phase - 4000U;
            state->speed_centi_kph =
                (uint16_t)(6000U - ramp_tick * 6000U / 1500U);
            state->throttle_tenth_percent = 0U;
        }
        state->gear = select_semantic_gear(state->speed_centi_kph);
        state->engine_quarter_rpm = calculate_semantic_engine_speed(
            state->speed_centi_kph, state->gear);
        state->door_flags = 0U;
        state->ignition_state = 2U;
    }
    state->battery_millivolt =
        state->engine_quarter_rpm == 0U ? 12600U : 13800U;
}

static void advance_semantic_slow_state(semantic_vehicle_state *state,
                                        uint32_t *distance_accumulator,
                                        uint16_t *warmup_ticks,
                                        uint16_t *cooldown_ticks)
{
    *distance_accumulator += state->speed_centi_kph;
    if (*distance_accumulator >= UINT32_C(3600000)) {
        *distance_accumulator -= UINT32_C(3600000);
        if (state->odometer_tenth_km < UINT32_C(0x00ffffff)) {
            state->odometer_tenth_km++;
        }
    }

    if (state->engine_quarter_rpm != 0U) {
        *cooldown_ticks = 0U;
        (*warmup_ticks)++;
        if (*warmup_ticks >= 100U) {
            *warmup_ticks = 0U;
            if (state->coolant_celsius < 90) {
                state->coolant_celsius++;
            }
        }
    } else {
        *warmup_ticks = 0U;
        (*cooldown_ticks)++;
        if (*cooldown_ticks >= 500U) {
            *cooldown_ticks = 0U;
            if (state->coolant_celsius > 20) {
                state->coolant_celsius--;
            }
        }
    }
}

static void build_semantic_vehicle_payload(
    const semantic_vehicle_state *state,
    uint8_t counter,
    uint8_t payload[GATEWAY_CAN_DATA_SIZE])
{
    (void)memset(payload, 0, GATEWAY_CAN_DATA_SIZE);
    pack_u16_le(payload, 0U, state->speed_centi_kph);
    pack_u16_le(payload, 2U, state->engine_quarter_rpm);
    payload[4] = (uint8_t)(state->throttle_tenth_percent / 4U);
    payload[5] = state->gear;
    finalize_payload(payload, counter);
}

static void build_semantic_power_payload(
    const semantic_vehicle_state *state,
    uint8_t counter,
    uint8_t payload[GATEWAY_CAN_DATA_SIZE])
{
    (void)memset(payload, 0, GATEWAY_CAN_DATA_SIZE);
    pack_u16_le(payload, 0U, state->battery_millivolt);
    payload[2] = (uint8_t)(state->coolant_celsius + 40);
    payload[3] = (uint8_t)(state->soc_tenth_percent / 4U);
    pack_u16_le(payload, 4U, state->fault_flags);
    finalize_payload(payload, counter);
}

static void build_semantic_body_payload(
    const semantic_vehicle_state *state,
    uint8_t counter,
    uint8_t payload[GATEWAY_CAN_DATA_SIZE])
{
    (void)memset(payload, 0, GATEWAY_CAN_DATA_SIZE);
    pack_u24_le(payload, 0U, state->odometer_tenth_km);
    payload[3] = (uint8_t)(state->door_flags & UINT8_C(0x0f));
    payload[4] = (uint8_t)(state->ignition_state & UINT8_C(0x03));
    finalize_payload(payload, counter);
}

static int verify_semantic_payload(uint32_t can_id,
                                   const semantic_vehicle_state *state,
                                   const uint8_t payload[GATEWAY_CAN_DATA_SIZE])
{
    gateway_decoded_payload decoded;

    CHECK(gateway_vehicle_decode_frame(can_id, GATEWAY_CAN_DATA_SIZE,
                                       payload, &decoded) ==
          GATEWAY_DECODE_OK);
    CHECK(decoded.rolling_counter == payload[6]);
    CHECK(decoded.checksum == payload[7]);
    switch (can_id) {
    case GATEWAY_CAN_ID_VEHICLE_DYNAMICS:
        CHECK(decoded.signals.vehicle_dynamics.vehicle_speed_centi_kph ==
              state->speed_centi_kph);
        CHECK(decoded.signals.vehicle_dynamics.engine_speed_quarter_rpm ==
              state->engine_quarter_rpm);
        CHECK(decoded.signals.vehicle_dynamics.throttle_tenth_percent ==
              state->throttle_tenth_percent);
        CHECK(decoded.signals.vehicle_dynamics.gear == state->gear);
        break;
    case GATEWAY_CAN_ID_POWER_STATUS:
        CHECK(decoded.signals.power_status.battery_millivolt ==
              state->battery_millivolt);
        CHECK(decoded.signals.power_status.coolant_celsius ==
              state->coolant_celsius);
        CHECK(decoded.signals.power_status.soc_tenth_percent ==
              state->soc_tenth_percent);
        CHECK(decoded.signals.power_status.fault_flags == state->fault_flags);
        break;
    case GATEWAY_CAN_ID_BODY_STATUS:
        CHECK(decoded.signals.body_status.odometer_tenth_km ==
              state->odometer_tenth_km);
        CHECK(decoded.signals.body_status.door_flags == state->door_flags);
        CHECK(decoded.signals.body_status.ignition_state ==
              state->ignition_state);
        CHECK((payload[3] & UINT8_C(0xf0)) == 0U);
        CHECK((payload[4] & UINT8_C(0xfc)) == 0U);
        CHECK(payload[5] == 0U);
        break;
    default:
        CHECK(0);
    }
    return 0;
}

static int test_stm32_semantic_drive_cycle(void)
{
    semantic_vehicle_state state = {
        0U, 0U, 0U, 12600U, UINT32_C(1234567), 20, 800U, 0U, 0U, 1U, 0U
    };
    uint32_t distance_accumulator = 0;
    uint16_t warmup_ticks = 0;
    uint16_t cooldown_ticks = 0;
    unsigned int phase;
    unsigned int power_index = 0;
    unsigned int body_index = 0;

    for (phase = 0; phase < SEMANTIC_DRIVE_CYCLE_TICKS; phase++) {
        uint8_t payload[GATEWAY_CAN_DATA_SIZE];

        update_semantic_fast_state(&state, phase);
        build_semantic_vehicle_payload(&state, (uint8_t)phase, payload);
        CHECK(verify_semantic_payload(GATEWAY_CAN_ID_VEHICLE_DYNAMICS,
                                      &state, payload) == 0);

        if (phase % 10U == 9U) {
            build_semantic_power_payload(&state, (uint8_t)power_index,
                                         payload);
            CHECK(verify_semantic_payload(GATEWAY_CAN_ID_POWER_STATUS,
                                          &state, payload) == 0);
            power_index++;
        }
        if (phase % 100U == 99U) {
            build_semantic_body_payload(&state, (uint8_t)body_index,
                                        payload);
            CHECK(verify_semantic_payload(GATEWAY_CAN_ID_BODY_STATUS,
                                          &state, payload) == 0);
            body_index++;
        }
        advance_semantic_slow_state(&state, &distance_accumulator,
                                    &warmup_ticks, &cooldown_ticks);
    }
    CHECK(power_index == 600U);
    CHECK(body_index == 60U);
    CHECK(state.odometer_tenth_km == UINT32_C(1234572));
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
    CHECK(test_stm32_semantic_drive_cycle() == 0);
    return 0;
}
