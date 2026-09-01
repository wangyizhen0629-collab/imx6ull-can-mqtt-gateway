#include "gateway/spool.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,         \
                    __LINE__, #condition);                                     \
            return 1;                                                          \
        }                                                                      \
    } while (0)

static telemetry_record make_record(uint64_t sequence)
{
    telemetry_record record;

    (void)memset(&record, 0, sizeof(record));
    record.gateway_seq = sequence;
    record.kernel_timestamp_ns = (int64_t)(sequence * 1000U);
    record.can_id = 0x100U + (uint32_t)(sequence % 3U);
    record.data[0] = (uint8_t)sequence;
    record.status_flags = GATEWAY_RECORD_STATUS_KERNEL_TIMESTAMP_VALID |
                          GATEWAY_RECORD_STATUS_CHECKSUM_VALID |
                          GATEWAY_RECORD_STATUS_DECODED_VALID;
    record.dlc = GATEWAY_CAN_DATA_SIZE;
    record.ecu_counter = (uint8_t)sequence;
    record.decoded_payload[0] = (uint8_t)(sequence + 1U);
    return record;
}

static int write_byte(const char *path, off_t offset, uint8_t value)
{
    int fd = open(path, O_WRONLY);
    ssize_t count;

    if (fd < 0) {
        return -1;
    }
    count = pwrite(fd, &value, 1, offset);
    if (count == 1 && fdatasync(fd) != 0) {
        count = -1;
    }
    if (close(fd) != 0) {
        count = -1;
    }
    return count == 1 ? 0 : -1;
}

static int append_bytes(const char *path, const uint8_t *bytes, size_t size)
{
    int fd = open(path, O_WRONLY | O_APPEND);
    ssize_t count;

    if (fd < 0) {
        return -1;
    }
    count = write(fd, bytes, size);
    if (count == (ssize_t)size && fdatasync(fd) != 0) {
        count = -1;
    }
    if (close(fd) != 0) {
        count = -1;
    }
    return count == (ssize_t)size ? 0 : -1;
}

static int test_append_ack_reopen_and_tail_recovery(const char *data_path,
                                                    const char *state_path)
{
    gateway_spool *spool = NULL;
    gateway_spool_snapshot snapshot;
    telemetry_record records[4];
    uint8_t partial[7] = {1, 2, 3, 4, 5, 6, 7};
    uint64_t batch_seq = 0;
    size_t count = 0;
    uint64_t sequence;

    CHECK(gateway_spool_open(&spool, data_path) == GATEWAY_OK);
    for (sequence = 1; sequence <= 5; sequence++) {
        telemetry_record record = make_record(sequence);

        CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    }
    CHECK(gateway_spool_last_gateway_seq(spool) == 5);
    CHECK(gateway_spool_prepare_batch(spool, records, 2, &count,
                                      &batch_seq) == GATEWAY_OK);
    CHECK(count == 2 && batch_seq == 1);
    CHECK(records[0].gateway_seq == 1 && records[1].gateway_seq == 2);
    CHECK(gateway_spool_ack_prepared(spool) == GATEWAY_OK);
    gateway_spool_read(spool, &snapshot);
    CHECK(snapshot.pending_records == 3);
    CHECK(snapshot.last_acked_gateway_seq == 2);
    CHECK(snapshot.next_batch_seq == 2);
    gateway_spool_close(spool);
    spool = NULL;

    CHECK(append_bytes(data_path, partial, sizeof(partial)) == 0);
    CHECK(gateway_spool_open(&spool, data_path) == GATEWAY_OK);
    gateway_spool_read(spool, &snapshot);
    CHECK(snapshot.total_records == 5);
    CHECK(snapshot.pending_records == 3);
    CHECK(snapshot.tail_recoveries == 1);
    CHECK(snapshot.state_recoveries == 0);
    CHECK(gateway_spool_prepare_batch(spool, records, 4, &count,
                                      &batch_seq) == GATEWAY_OK);
    CHECK(count == 3 && batch_seq == 2);
    CHECK(records[0].gateway_seq == 3 && records[2].gateway_seq == 5);
    gateway_spool_cancel_prepared(spool);
    gateway_spool_close(spool);
    spool = NULL;

    CHECK(write_byte(state_path, 8, 0xffU) == 0);
    CHECK(gateway_spool_open(&spool, data_path) == GATEWAY_OK);
    gateway_spool_read(spool, &snapshot);
    CHECK(snapshot.state_recoveries == 1);
    CHECK(snapshot.pending_records == 5);
    CHECK(snapshot.last_acked_gateway_seq == 0);
    CHECK(snapshot.next_batch_seq == 1);
    CHECK(gateway_spool_prepare_batch(spool, records, 2, &count,
                                      &batch_seq) == GATEWAY_OK);
    CHECK(records[0].gateway_seq == 1 && batch_seq == 1);
    gateway_spool_close(spool);
    return 0;
}

static int test_interior_corruption_is_fatal(const char *data_path)
{
    gateway_spool *spool = NULL;
    telemetry_record record;

    CHECK(gateway_spool_open(&spool, data_path) == GATEWAY_OK);
    record = make_record(1);
    CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    record = make_record(2);
    CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    gateway_spool_close(spool);
    spool = NULL;
    CHECK(write_byte(data_path, 20, 0xaaU) == 0);
    CHECK(gateway_spool_open(&spool, data_path) ==
          GATEWAY_ERROR_INVALID_VALUE);
    CHECK(spool == NULL);
    return 0;
}

static int test_single_writer_lock(const char *data_path)
{
    gateway_spool *first = NULL;
    gateway_spool *second = NULL;

    CHECK(gateway_spool_open(&first, data_path) == GATEWAY_OK);
    CHECK(gateway_spool_open(&second, data_path) == GATEWAY_ERROR_CLOSED);
    CHECK(second == NULL);
    gateway_spool_close(first);
    CHECK(gateway_spool_open(&second, data_path) == GATEWAY_OK);
    gateway_spool_close(second);
    return 0;
}

int main(void)
{
    char directory[] = "/tmp/gateway-spool-test-XXXXXX";
    char data_path[256];
    char state_path[256];
    char corrupt_path[256];
    char lock_path[256];

    CHECK(mkdtemp(directory) != NULL);
    CHECK(snprintf(data_path, sizeof(data_path), "%s/spool.data", directory) > 0);
    CHECK(snprintf(state_path, sizeof(state_path), "%s/spool.state", directory) > 0);
    CHECK(snprintf(corrupt_path, sizeof(corrupt_path), "%s/corrupt.data",
                   directory) > 0);
    CHECK(snprintf(lock_path, sizeof(lock_path), "%s/lock.data", directory) > 0);
    CHECK(test_append_ack_reopen_and_tail_recovery(data_path, state_path) == 0);
    CHECK(test_interior_corruption_is_fatal(corrupt_path) == 0);
    CHECK(test_single_writer_lock(lock_path) == 0);
    CHECK(unlink(data_path) == 0);
    CHECK(unlink(state_path) == 0);
    CHECK(unlink(corrupt_path) == 0);
    CHECK(unlink(lock_path) == 0);
    CHECK(rmdir(directory) == 0);
    return 0;
}
