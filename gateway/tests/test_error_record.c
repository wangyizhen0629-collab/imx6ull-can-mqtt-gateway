#include "gateway/error.h"
#include "gateway/telemetry_record.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,         \
                    __LINE__, #condition);                                     \
            return 1;                                                          \
        }                                                                      \
    } while (0)

int main(void)
{
    telemetry_record record = {0};

    CHECK(sizeof(record) == GATEWAY_TELEMETRY_RECORD_SIZE);
    CHECK(sizeof(record.data) == GATEWAY_CAN_DATA_SIZE);
    CHECK(sizeof(record.decoded_payload) == GATEWAY_DECODED_PAYLOAD_SIZE);
    record.gateway_seq = UINT64_C(0x0102030405060708);
    record.can_id = UINT32_C(0x100);
    record.dlc = 8;
    CHECK(record.gateway_seq == UINT64_C(0x0102030405060708));
    CHECK(record.can_id == UINT32_C(0x100));
    CHECK(strcmp(gateway_error_string(GATEWAY_ERROR_TIMEOUT), "timeout") == 0);
    CHECK(strcmp(gateway_error_string((gateway_error_code)999),
                 "unknown error") == 0);
    return 0;
}
