#include "gateway/spool.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

static void sleep_milliseconds(unsigned int milliseconds)
{
    struct timespec remaining;

    remaining.tv_sec = (time_t)(milliseconds / 1000U);
    remaining.tv_nsec = (long)(milliseconds % 1000U) * 1000000L;
    while (nanosleep(&remaining, &remaining) != 0 && errno == EINTR) {
    }
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

static int remove_v2_directory(const char *directory, uint64_t maximum_segment)
{
    char path[512];
    uint64_t number;

    for (number = 1; number <= maximum_segment; number++) {
        int written = snprintf(path, sizeof(path),
                               "%s/segment-%020" PRIu64 ".gsp2",
                               directory, number);

        if (written < 0 || (size_t)written >= sizeof(path)) {
            return -1;
        }
        if (unlink(path) != 0 && errno != ENOENT) {
            return -1;
        }
    }
    if (snprintf(path, sizeof(path), "%s/state.v2", directory) < 0) {
        return -1;
    }
    if (unlink(path) != 0 && errno != ENOENT) {
        return -1;
    }
    if (snprintf(path, sizeof(path), "%s/state.v2.tmp", directory) < 0) {
        return -1;
    }
    if (unlink(path) != 0 && errno != ENOENT) {
        return -1;
    }
    if (snprintf(path, sizeof(path), "%s/lock", directory) < 0 ||
        unlink(path) != 0) {
        return -1;
    }
    return rmdir(directory);
}

static int test_v2_roll_reclaim_and_sequence_state(const char *parent)
{
    char directory[256];
    gateway_spool_v2_options options;
    gateway_spool_snapshot snapshot;
    gateway_spool *spool = NULL;
    telemetry_record records[4];
    uint64_t batch_seq = 0;
    size_t count = 0;
    uint64_t sequence;

    CHECK(snprintf(directory, sizeof(directory), "%s/v2-roll", parent) > 0);
    gateway_spool_v2_options_init(&options);
    options.max_bytes = 6 * GATEWAY_SPOOL_ENTRY_SIZE;
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 2) ==
          GATEWAY_OK);
    for (sequence = 1; sequence <= 5; sequence++) {
        telemetry_record record = make_record(sequence);

        CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    }
    gateway_spool_read(spool, &snapshot);
    CHECK(snapshot.v2);
    CHECK(snapshot.segment_count == 3);
    CHECK(snapshot.physical_bytes == 5 * GATEWAY_SPOOL_ENTRY_SIZE);
    CHECK(snapshot.pending_bytes == snapshot.physical_bytes);
    CHECK(gateway_spool_prepare_batch(spool, records, 3, &count,
                                      &batch_seq) == GATEWAY_OK);
    CHECK(count == 3 && batch_seq == 1);
    CHECK(records[0].gateway_seq == 1 && records[2].gateway_seq == 3);
    CHECK(gateway_spool_ack_prepared(spool) == GATEWAY_OK);
    gateway_spool_read(spool, &snapshot);
    CHECK(snapshot.pending_records == 2);
    CHECK(snapshot.segment_count == 2);
    CHECK(snapshot.segments_reclaimed == 1);
    CHECK(snapshot.physical_bytes == 3 * GATEWAY_SPOOL_ENTRY_SIZE);
    CHECK(gateway_spool_prepare_batch(spool, records, 4, &count,
                                      &batch_seq) == GATEWAY_OK);
    CHECK(count == 2 && batch_seq == 2);
    CHECK(records[0].gateway_seq == 4 && records[1].gateway_seq == 5);
    CHECK(gateway_spool_ack_prepared(spool) == GATEWAY_OK);
    gateway_spool_read(spool, &snapshot);
    CHECK(snapshot.pending_records == 0);
    CHECK(snapshot.physical_bytes == 0);
    CHECK(snapshot.segment_count == 0);
    CHECK(snapshot.last_gateway_seq == 5);
    CHECK(snapshot.last_acked_gateway_seq == 5);
    CHECK(snapshot.next_batch_seq == 3);
    gateway_spool_close(spool);
    spool = NULL;

    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 2) ==
          GATEWAY_OK);
    CHECK(gateway_spool_last_gateway_seq(spool) == 5);
    gateway_spool_read(spool, &snapshot);
    CHECK(snapshot.next_batch_seq == 3);
    {
        telemetry_record record = make_record(6);

        CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    }
    gateway_spool_close(spool);
    CHECK(remove_v2_directory(directory, 4) == 0);
    return 0;
}

static int test_v2_capacity_preserves_unacked(const char *parent)
{
    char directory[256];
    gateway_spool_v2_options options;
    gateway_spool_snapshot snapshot;
    gateway_spool *spool = NULL;
    telemetry_record records[2];
    telemetry_record record;
    uint64_t batch_seq;
    size_t count;

    CHECK(snprintf(directory, sizeof(directory), "%s/v2-capacity", parent) >
          0);
    gateway_spool_v2_options_init(&options);
    options.max_bytes = 2 * GATEWAY_SPOOL_ENTRY_SIZE;
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 2) ==
          GATEWAY_OK);
    record = make_record(1);
    CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    record = make_record(2);
    CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    record = make_record(3);
    CHECK(gateway_spool_append(spool, &record) == GATEWAY_ERROR_CAPACITY);
    gateway_spool_read(spool, &snapshot);
    CHECK(snapshot.pending_records == 2);
    CHECK(snapshot.physical_bytes == 2 * GATEWAY_SPOOL_ENTRY_SIZE);
    CHECK(gateway_spool_prepare_batch(spool, records, 2, &count,
                                      &batch_seq) == GATEWAY_OK);
    CHECK(count == 2);
    CHECK(gateway_spool_ack_prepared(spool) == GATEWAY_OK);
    CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    gateway_spool_close(spool);
    CHECK(remove_v2_directory(directory, 3) == 0);
    return 0;
}

static int test_v2_tail_corruption_and_missing(const char *parent)
{
    char directory[256];
    char path[512];
    gateway_spool_v2_options options;
    gateway_spool_snapshot snapshot;
    gateway_spool *spool = NULL;
    uint8_t partial[7] = {1, 2, 3, 4, 5, 6, 7};
    uint64_t sequence;

    gateway_spool_v2_options_init(&options);
    options.max_bytes = 6 * GATEWAY_SPOOL_ENTRY_SIZE;
    CHECK(snprintf(directory, sizeof(directory), "%s/v2-tail", parent) > 0);
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 3) ==
          GATEWAY_OK);
    for (sequence = 1; sequence <= 2; sequence++) {
        telemetry_record record = make_record(sequence);

        CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    }
    gateway_spool_close(spool);
    spool = NULL;
    CHECK(snprintf(path, sizeof(path),
                   "%s/segment-00000000000000000001.gsp2", directory) > 0);
    CHECK(append_bytes(path, partial, sizeof(partial)) == 0);
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 3) ==
          GATEWAY_OK);
    gateway_spool_read(spool, &snapshot);
    CHECK(snapshot.tail_recoveries == 1);
    CHECK(snapshot.pending_records == 2);
    gateway_spool_close(spool);
    CHECK(remove_v2_directory(directory, 2) == 0);

    CHECK(snprintf(directory, sizeof(directory), "%s/v2-corrupt", parent) >
          0);
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 3) ==
          GATEWAY_OK);
    for (sequence = 1; sequence <= 3; sequence++) {
        telemetry_record record = make_record(sequence);

        CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    }
    gateway_spool_close(spool);
    spool = NULL;
    CHECK(snprintf(path, sizeof(path),
                   "%s/segment-00000000000000000001.gsp2", directory) > 0);
    CHECK(write_byte(path, GATEWAY_SPOOL_ENTRY_SIZE + 20, 0xaaU) == 0);
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 3) ==
          GATEWAY_ERROR_INVALID_VALUE);
    CHECK(spool == NULL);
    CHECK(remove_v2_directory(directory, 2) == 0);

    CHECK(snprintf(directory, sizeof(directory), "%s/v2-missing", parent) >
          0);
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 2) ==
          GATEWAY_OK);
    for (sequence = 1; sequence <= 4; sequence++) {
        telemetry_record record = make_record(sequence);

        CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    }
    gateway_spool_close(spool);
    spool = NULL;
    CHECK(snprintf(path, sizeof(path),
                   "%s/segment-00000000000000000001.gsp2", directory) > 0);
    CHECK(unlink(path) == 0);
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 2) ==
          GATEWAY_ERROR_INVALID_VALUE);
    CHECK(spool == NULL);
    CHECK(remove_v2_directory(directory, 3) == 0);
    return 0;
}

static int test_v2_fault_injection(const char *parent)
{
    static const gateway_spool_fault_point append_faults[] = {
        GATEWAY_SPOOL_FAULT_APPEND_WRITE,
        GATEWAY_SPOOL_FAULT_SEGMENT_SYNC,
        GATEWAY_SPOOL_FAULT_STATE_WRITE,
        GATEWAY_SPOOL_FAULT_STATE_SYNC,
        GATEWAY_SPOOL_FAULT_DIRECTORY_SYNC
    };
    gateway_spool_v2_options options;
    size_t index;

    gateway_spool_v2_options_init(&options);
    options.max_bytes = 4 * GATEWAY_SPOOL_ENTRY_SIZE;
    for (index = 0; index < sizeof(append_faults) / sizeof(append_faults[0]);
         index++) {
        char directory[256];
        gateway_spool *spool = NULL;
        telemetry_record record = make_record(1);

        CHECK(snprintf(directory, sizeof(directory), "%s/v2-fault-%zu",
                       parent, index) > 0);
        CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 2) ==
              GATEWAY_OK);
        CHECK(gateway_spool_test_fail_next(spool, append_faults[index]) ==
              GATEWAY_OK);
        CHECK(gateway_spool_append(spool, &record) == GATEWAY_ERROR_IO);
        gateway_spool_close(spool);
        spool = NULL;
        CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 2) ==
              GATEWAY_OK);
        record = make_record(gateway_spool_last_gateway_seq(spool) + 1);
        CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
        gateway_spool_close(spool);
        CHECK(remove_v2_directory(directory, 3) == 0);
    }
    {
        char directory[256];
        gateway_spool *spool = NULL;
        telemetry_record record = make_record(1);
        telemetry_record prepared;
        uint64_t batch_seq;
        size_t count;

        CHECK(snprintf(directory, sizeof(directory), "%s/v2-fault-ack",
                       parent) > 0);
        CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 2) ==
              GATEWAY_OK);
        CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
        CHECK(gateway_spool_prepare_batch(spool, &prepared, 1, &count,
                                          &batch_seq) == GATEWAY_OK);
        CHECK(gateway_spool_test_fail_next(
                  spool, GATEWAY_SPOOL_FAULT_STATE_RENAME) == GATEWAY_OK);
        CHECK(gateway_spool_ack_prepared(spool) == GATEWAY_ERROR_IO);
        gateway_spool_close(spool);
        spool = NULL;
        CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 2) ==
              GATEWAY_OK);
        CHECK(gateway_spool_prepare_batch(spool, &prepared, 1, &count,
                                          &batch_seq) == GATEWAY_OK);
        CHECK(count == 1 && prepared.gateway_seq == 1);
        CHECK(gateway_spool_ack_prepared(spool) == GATEWAY_OK);
        gateway_spool_close(spool);
        CHECK(remove_v2_directory(directory, 3) == 0);
    }
    {
        char directory[256];
        gateway_spool *spool = NULL;
        gateway_spool_snapshot snapshot;
        telemetry_record record = make_record(1);
        telemetry_record prepared;
        uint64_t batch_seq;
        size_t count;

        CHECK(snprintf(directory, sizeof(directory), "%s/v2-fault-delete",
                       parent) > 0);
        CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 2) ==
              GATEWAY_OK);
        CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
        CHECK(gateway_spool_prepare_batch(spool, &prepared, 1, &count,
                                          &batch_seq) == GATEWAY_OK);
        CHECK(gateway_spool_test_fail_next(
                  spool, GATEWAY_SPOOL_FAULT_SEGMENT_DELETE) == GATEWAY_OK);
        CHECK(gateway_spool_ack_prepared(spool) == GATEWAY_ERROR_IO);
        gateway_spool_close(spool);
        spool = NULL;
        CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 2) ==
              GATEWAY_OK);
        gateway_spool_read(spool, &snapshot);
        CHECK(snapshot.pending_records == 0 && snapshot.physical_bytes == 0);
        CHECK(gateway_spool_last_gateway_seq(spool) == 1);
        gateway_spool_close(spool);
        CHECK(remove_v2_directory(directory, 3) == 0);
    }
    return 0;
}

static int test_v2_fail_closed_layout(const char *parent)
{
    char directory[256];
    char path[512];
    gateway_spool_v2_options options;
    gateway_spool *spool = NULL;
    telemetry_record record;
    int descriptor;

    gateway_spool_v2_options_init(&options);
    options.max_bytes = 6 * GATEWAY_SPOOL_ENTRY_SIZE;
    CHECK(snprintf(path, sizeof(path), "%s/legacy-file", parent) > 0);
    descriptor = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    CHECK(descriptor >= 0);
    CHECK(close(descriptor) == 0);
    CHECK(gateway_spool_test_open_v2(&spool, path, &options, 2) ==
          GATEWAY_ERROR_INVALID_VALUE);
    CHECK(spool == NULL);
    CHECK(unlink(path) == 0);

    CHECK(snprintf(directory, sizeof(directory), "%s/v2-no-state", parent) >
          0);
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 2) ==
          GATEWAY_OK);
    record = make_record(1);
    CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    gateway_spool_close(spool);
    spool = NULL;
    CHECK(snprintf(path, sizeof(path), "%s/state.v2", directory) > 0);
    CHECK(unlink(path) == 0);
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 2) ==
          GATEWAY_ERROR_INVALID_VALUE);
    CHECK(spool == NULL);
    CHECK(remove_v2_directory(directory, 3) == 0);

    CHECK(snprintf(directory, sizeof(directory), "%s/v2-gap", parent) > 0);
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 2) ==
          GATEWAY_OK);
    record = make_record(1);
    CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    record = make_record(2);
    CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    record = make_record(3);
    CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    gateway_spool_close(spool);
    spool = NULL;
    {
        char destination[512];

        CHECK(snprintf(path, sizeof(path),
                       "%s/segment-00000000000000000002.gsp2",
                       directory) > 0);
        CHECK(snprintf(destination, sizeof(destination),
                       "%s/segment-00000000000000000003.gsp2",
                       directory) > 0);
        CHECK(rename(path, destination) == 0);
    }
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 2) ==
          GATEWAY_ERROR_INVALID_VALUE);
    CHECK(spool == NULL);
    CHECK(remove_v2_directory(directory, 4) == 0);
    return 0;
}

static int test_v2_group_commit(const char *parent)
{
    char directory[256];
    gateway_spool_v2_options options;
    gateway_spool_snapshot first;
    gateway_spool_snapshot second;
    gateway_spool_snapshot snapshot;
    gateway_spool *spool = NULL;
    telemetry_record records[6];
    uint64_t batch_seq;
    size_t count;
    uint64_t sequence;

    gateway_spool_v2_options_init(&options);
    options.max_bytes = 10 * GATEWAY_SPOOL_ENTRY_SIZE;
    options.sync_records = 3;
    options.sync_interval_ms = 60000;
    CHECK(snprintf(directory, sizeof(directory), "%s/v2-group-count", parent) >
          0);
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 5) ==
          GATEWAY_OK);
    {
        telemetry_record record = make_record(1);

        CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    }
    gateway_spool_read(spool, &first);
    CHECK(first.unsynced_records == 1);
    {
        telemetry_record record = make_record(2);

        CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    }
    gateway_spool_read(spool, &second);
    CHECK(second.unsynced_records == 2);
    CHECK(second.sync_count == first.sync_count);
    {
        telemetry_record record = make_record(3);

        CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    }
    gateway_spool_read(spool, &snapshot);
    CHECK(snapshot.unsynced_records == 0);
    CHECK(snapshot.sync_count > second.sync_count);
    CHECK(gateway_spool_prepare_batch(spool, records, 6, &count,
                                      &batch_seq) == GATEWAY_OK);
    CHECK(count == 3 && records[0].gateway_seq == 1 &&
          records[2].gateway_seq == 3);
    CHECK(gateway_spool_ack_prepared(spool) == GATEWAY_OK);
    gateway_spool_close(spool);
    CHECK(remove_v2_directory(directory, 3) == 0);

    options.sync_interval_ms = 20;
    CHECK(snprintf(directory, sizeof(directory), "%s/v2-group-time", parent) >
          0);
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 5) ==
          GATEWAY_OK);
    {
        telemetry_record record = make_record(1);

        CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    }
    gateway_spool_read(spool, &first);
    CHECK(first.unsynced_records == 1);
    sleep_milliseconds(30);
    CHECK(gateway_spool_poll(spool) == GATEWAY_OK);
    gateway_spool_read(spool, &second);
    CHECK(second.unsynced_records == 0 && second.sync_count > first.sync_count);
    gateway_spool_close(spool);
    CHECK(remove_v2_directory(directory, 3) == 0);

    options.sync_records = 4;
    options.sync_interval_ms = 60000;
    CHECK(snprintf(directory, sizeof(directory), "%s/v2-group-prepare",
                   parent) > 0);
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 5) ==
          GATEWAY_OK);
    for (sequence = 1; sequence <= 2; sequence++) {
        telemetry_record record = make_record(sequence);

        CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    }
    gateway_spool_read(spool, &first);
    CHECK(first.unsynced_records == 2);
    CHECK(gateway_spool_prepare_batch(spool, records, 6, &count,
                                      &batch_seq) == GATEWAY_OK);
    gateway_spool_read(spool, &second);
    CHECK(count == 2 && second.unsynced_records == 0);
    CHECK(second.sync_count > first.sync_count);
    gateway_spool_cancel_prepared(spool);
    gateway_spool_close(spool);
    spool = NULL;
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 5) ==
          GATEWAY_OK);
    gateway_spool_read(spool, &snapshot);
    CHECK(snapshot.pending_records == 2 && snapshot.unsynced_records == 0);
    CHECK(gateway_spool_last_gateway_seq(spool) == 2);
    gateway_spool_close(spool);
    CHECK(remove_v2_directory(directory, 3) == 0);

    CHECK(snprintf(directory, sizeof(directory), "%s/v2-group-close", parent) >
          0);
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 5) ==
          GATEWAY_OK);
    {
        telemetry_record record = make_record(1);

        CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    }
    gateway_spool_read(spool, &snapshot);
    CHECK(snapshot.unsynced_records == 1);
    gateway_spool_close(spool);
    spool = NULL;
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 5) ==
          GATEWAY_OK);
    gateway_spool_read(spool, &snapshot);
    CHECK(snapshot.pending_records == 1 && snapshot.unsynced_records == 0);
    CHECK(gateway_spool_last_gateway_seq(spool) == 1);
    gateway_spool_close(spool);
    CHECK(remove_v2_directory(directory, 3) == 0);

    options.sync_records = 3;
    CHECK(snprintf(directory, sizeof(directory), "%s/v2-group-failure",
                   parent) > 0);
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 5) ==
          GATEWAY_OK);
    {
        telemetry_record record = make_record(1);

        CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    }
    CHECK(gateway_spool_test_fail_next(
              spool, GATEWAY_SPOOL_FAULT_SEGMENT_SYNC) == GATEWAY_OK);
    CHECK(gateway_spool_flush(spool) == GATEWAY_ERROR_IO);
    gateway_spool_read(spool, &snapshot);
    CHECK(snapshot.sync_failures == 1 && snapshot.unsynced_records == 1);
    gateway_spool_close(spool);
    spool = NULL;
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 5) ==
          GATEWAY_OK);
    CHECK(gateway_spool_last_gateway_seq(spool) == 3);
    {
        telemetry_record record = make_record(4);

        CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    }
    gateway_spool_close(spool);
    CHECK(remove_v2_directory(directory, 3) == 0);

    options.sync_records = 2;
    CHECK(snprintf(directory, sizeof(directory), "%s/v2-group-cross", parent) >
          0);
    CHECK(gateway_spool_test_open_v2(&spool, directory, &options, 2) ==
          GATEWAY_OK);
    for (sequence = 1; sequence <= 6; sequence++) {
        telemetry_record record = make_record(sequence);

        CHECK(gateway_spool_append(spool, &record) == GATEWAY_OK);
    }
    CHECK(gateway_spool_prepare_batch(spool, records, 4, &count,
                                      &batch_seq) == GATEWAY_OK);
    CHECK(count == 4 && records[0].gateway_seq == 1 &&
          records[3].gateway_seq == 4);
    CHECK(gateway_spool_ack_prepared(spool) == GATEWAY_OK);
    CHECK(gateway_spool_prepare_batch(spool, records, 4, &count,
                                      &batch_seq) == GATEWAY_OK);
    CHECK(count == 2 && records[0].gateway_seq == 5 &&
          records[1].gateway_seq == 6);
    CHECK(gateway_spool_ack_prepared(spool) == GATEWAY_OK);
    gateway_spool_read(spool, &snapshot);
    CHECK(snapshot.pending_records == 0 && snapshot.physical_bytes == 0);
    gateway_spool_close(spool);
    CHECK(remove_v2_directory(directory, 5) == 0);
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
    CHECK(test_v2_roll_reclaim_and_sequence_state(directory) == 0);
    CHECK(test_v2_capacity_preserves_unacked(directory) == 0);
    CHECK(test_v2_tail_corruption_and_missing(directory) == 0);
    CHECK(test_v2_fault_injection(directory) == 0);
    CHECK(test_v2_fail_closed_layout(directory) == 0);
    CHECK(test_v2_group_commit(directory) == 0);
    CHECK(unlink(data_path) == 0);
    CHECK(unlink(state_path) == 0);
    CHECK(unlink(corrupt_path) == 0);
    CHECK(unlink(lock_path) == 0);
    CHECK(rmdir(directory) == 0);
    return 0;
}
