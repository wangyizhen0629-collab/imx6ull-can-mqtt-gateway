#include "gateway/spool.h"

#include "gateway/config.h"
#include "spool_v2_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>

enum {
    SPOOL_HEADER_SIZE = 16,
    SPOOL_PAYLOAD_SIZE = 64,
    STATE_SIZE = 40,
    STATE_PATH_SIZE = GATEWAY_SPOOL_PATH_SIZE + 16
};

static const uint8_t ENTRY_MAGIC[4] = {'G', 'S', 'P', '1'};
static const uint8_t STATE_MAGIC[4] = {'G', 'S', 'T', '1'};

struct gateway_spool {
    gateway_spool_v2 *v2_spool;
    bool v2;
    int data_fd;
    char data_path[GATEWAY_SPOOL_PATH_SIZE];
    char state_path[STATE_PATH_SIZE];
    char state_tmp_path[STATE_PATH_SIZE];
    off_t data_end;
    off_t ack_offset;
    off_t prepared_end;
    uint64_t last_gateway_seq;
    uint64_t last_acked_gateway_seq;
    uint64_t next_batch_seq;
    uint64_t records_appended;
    uint64_t records_replayed;
    uint64_t tail_recoveries;
    uint64_t state_recoveries;
    uint64_t corruptions;
    bool batch_prepared;
};

static void put_u16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
    destination[2] = (uint8_t)(value >> 16);
    destination[3] = (uint8_t)(value >> 24);
}

static void put_u64(uint8_t *destination, uint64_t value)
{
    size_t index;

    for (index = 0; index < 8; index++) {
        destination[index] = (uint8_t)(value >> (index * 8U));
    }
}

static uint16_t get_u16(const uint8_t *source)
{
    return (uint16_t)source[0] | (uint16_t)((uint16_t)source[1] << 8);
}

static uint32_t get_u32(const uint8_t *source)
{
    return (uint32_t)source[0] | ((uint32_t)source[1] << 8) |
           ((uint32_t)source[2] << 16) | ((uint32_t)source[3] << 24);
}

static uint64_t get_u64(const uint8_t *source)
{
    uint64_t value = 0;
    size_t index;

    for (index = 0; index < 8; index++) {
        value |= (uint64_t)source[index] << (index * 8U);
    }
    return value;
}

static uint32_t crc32_bytes(const uint8_t *bytes, size_t size)
{
    uint32_t crc = UINT32_C(0xffffffff);
    size_t index;

    for (index = 0; index < size; index++) {
        unsigned int bit;

        crc ^= bytes[index];
        for (bit = 0; bit < 8; bit++) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);

            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & mask);
        }
    }
    return ~crc;
}

static gateway_error_code read_exact(int fd,
                                     void *buffer,
                                     size_t size,
                                     off_t offset)
{
    uint8_t *cursor = buffer;
    size_t completed = 0;

    while (completed < size) {
        ssize_t count = pread(fd, cursor + completed, size - completed,
                              offset + (off_t)completed);

        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return GATEWAY_ERROR_IO;
        }
        completed += (size_t)count;
    }
    return GATEWAY_OK;
}

static gateway_error_code write_exact(int fd,
                                      const void *buffer,
                                      size_t size,
                                      off_t offset)
{
    const uint8_t *cursor = buffer;
    size_t completed = 0;

    while (completed < size) {
        ssize_t count = pwrite(fd, cursor + completed, size - completed,
                               offset + (off_t)completed);

        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return GATEWAY_ERROR_IO;
        }
        completed += (size_t)count;
    }
    return GATEWAY_OK;
}

static void encode_record(uint8_t entry[GATEWAY_SPOOL_ENTRY_SIZE],
                          const telemetry_record *record)
{
    uint8_t *payload;

    (void)memset(entry, 0, GATEWAY_SPOOL_ENTRY_SIZE);
    (void)memcpy(entry, ENTRY_MAGIC, sizeof(ENTRY_MAGIC));
    put_u16(entry + 4, 1);
    put_u16(entry + 6, GATEWAY_SPOOL_ENTRY_SIZE);
    payload = entry + SPOOL_HEADER_SIZE;
    put_u64(payload, record->gateway_seq);
    put_u64(payload + 8, (uint64_t)record->kernel_timestamp_ns);
    put_u32(payload + 16, record->can_id);
    (void)memcpy(payload + 20, record->data, GATEWAY_CAN_DATA_SIZE);
    put_u16(payload + 28, record->status_flags);
    payload[30] = record->dlc;
    payload[31] = record->ecu_counter;
    (void)memcpy(payload + 32, record->decoded_payload,
                 GATEWAY_DECODED_PAYLOAD_SIZE);
    put_u32(entry + 8, crc32_bytes(payload, SPOOL_PAYLOAD_SIZE));
}

static gateway_error_code decode_record(
    const uint8_t entry[GATEWAY_SPOOL_ENTRY_SIZE],
    telemetry_record *record)
{
    const uint8_t *payload = entry + SPOOL_HEADER_SIZE;

    if (memcmp(entry, ENTRY_MAGIC, sizeof(ENTRY_MAGIC)) != 0 ||
        get_u16(entry + 4) != 1 ||
        get_u16(entry + 6) != GATEWAY_SPOOL_ENTRY_SIZE ||
        get_u32(entry + 8) != crc32_bytes(payload, SPOOL_PAYLOAD_SIZE)) {
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    (void)memset(record, 0, sizeof(*record));
    record->gateway_seq = get_u64(payload);
    record->kernel_timestamp_ns = (int64_t)get_u64(payload + 8);
    record->can_id = get_u32(payload + 16);
    (void)memcpy(record->data, payload + 20, GATEWAY_CAN_DATA_SIZE);
    record->status_flags = get_u16(payload + 28);
    record->dlc = payload[30];
    record->ecu_counter = payload[31];
    (void)memcpy(record->decoded_payload, payload + 32,
                 GATEWAY_DECODED_PAYLOAD_SIZE);
    return record->gateway_seq == 0 ? GATEWAY_ERROR_INVALID_VALUE : GATEWAY_OK;
}

static gateway_error_code derive_paths(gateway_spool *spool,
                                       const char *data_path)
{
    size_t length = strlen(data_path);
    size_t prefix_length = length;
    int written;

    if (length == 0 || length >= sizeof(spool->data_path) ||
        data_path[0] != '/') {
        return GATEWAY_ERROR_ARGUMENT;
    }
    (void)snprintf(spool->data_path, sizeof(spool->data_path), "%s",
                   data_path);
    if (length >= 5 && strcmp(data_path + length - 5, ".data") == 0) {
        prefix_length = length - 5;
        written = snprintf(spool->state_path, sizeof(spool->state_path),
                           "%.*s.state", (int)prefix_length, data_path);
    } else {
        written = snprintf(spool->state_path, sizeof(spool->state_path),
                           "%s.state", data_path);
    }
    if (written < 0 || (size_t)written >= sizeof(spool->state_path)) {
        return GATEWAY_ERROR_RANGE;
    }
    written = snprintf(spool->state_tmp_path, sizeof(spool->state_tmp_path),
                       "%s.tmp", spool->state_path);
    if (written < 0 || (size_t)written >= sizeof(spool->state_tmp_path)) {
        return GATEWAY_ERROR_RANGE;
    }
    return GATEWAY_OK;
}

static gateway_error_code truncate_tail(gateway_spool *spool, off_t valid_end)
{
    if (ftruncate(spool->data_fd, valid_end) != 0 ||
        fdatasync(spool->data_fd) != 0) {
        return GATEWAY_ERROR_IO;
    }
    spool->data_end = valid_end;
    spool->tail_recoveries++;
    return GATEWAY_OK;
}

static gateway_error_code scan_data(gateway_spool *spool)
{
    struct stat status;
    uint64_t previous_seq = 0;
    off_t offset;

    if (fstat(spool->data_fd, &status) != 0 || !S_ISREG(status.st_mode)) {
        return GATEWAY_ERROR_IO;
    }
    spool->data_end = status.st_size;
    for (offset = 0;
         offset + (off_t)GATEWAY_SPOOL_ENTRY_SIZE <= status.st_size;
         offset += GATEWAY_SPOOL_ENTRY_SIZE) {
        uint8_t entry[GATEWAY_SPOOL_ENTRY_SIZE];
        telemetry_record record;
        gateway_error_code code = read_exact(spool->data_fd, entry,
                                             sizeof(entry), offset);

        if (code != GATEWAY_OK) {
            return code;
        }
        code = decode_record(entry, &record);
        if (code != GATEWAY_OK || record.gateway_seq <= previous_seq) {
            if (offset + (off_t)GATEWAY_SPOOL_ENTRY_SIZE == status.st_size) {
                spool->last_gateway_seq = previous_seq;
                return truncate_tail(spool, offset);
            }
            spool->corruptions++;
            return GATEWAY_ERROR_INVALID_VALUE;
        }
        previous_seq = record.gateway_seq;
    }
    offset = status.st_size - status.st_size % GATEWAY_SPOOL_ENTRY_SIZE;
    if (offset != status.st_size) {
        gateway_error_code code = truncate_tail(spool, offset);

        if (code != GATEWAY_OK) {
            return code;
        }
    }
    spool->last_gateway_seq = previous_seq;
    return GATEWAY_OK;
}

static gateway_error_code record_seq_at(gateway_spool *spool,
                                        off_t offset,
                                        uint64_t *sequence)
{
    uint8_t entry[GATEWAY_SPOOL_ENTRY_SIZE];
    telemetry_record record;
    gateway_error_code code;

    code = read_exact(spool->data_fd, entry, sizeof(entry), offset);
    if (code != GATEWAY_OK) {
        return code;
    }
    code = decode_record(entry, &record);
    if (code == GATEWAY_OK) {
        *sequence = record.gateway_seq;
    }
    return code;
}

static gateway_error_code load_state(gateway_spool *spool)
{
    uint8_t state[STATE_SIZE];
    struct stat status;
    uint64_t ack_offset;
    uint64_t ack_seq;
    uint64_t next_batch;
    bool valid = true;
    int fd = open(spool->state_path, O_RDONLY);

    spool->next_batch_seq = 1;
    if (fd < 0) {
        if (errno == ENOENT) {
            if (spool->data_end != 0) {
                spool->state_recoveries++;
            }
            return GATEWAY_OK;
        }
        return GATEWAY_ERROR_IO;
    }
    if (fstat(fd, &status) != 0 || status.st_size != STATE_SIZE ||
        read_exact(fd, state, sizeof(state), 0) != GATEWAY_OK) {
        valid = false;
    }
    if (close(fd) != 0) {
        return GATEWAY_ERROR_IO;
    }
    if (valid &&
        (memcmp(state, STATE_MAGIC, sizeof(STATE_MAGIC)) != 0 ||
         get_u16(state + 4) != 1 || get_u16(state + 6) != STATE_SIZE ||
         get_u32(state + 8) != crc32_bytes(state + 12, STATE_SIZE - 12))) {
        valid = false;
    }
    ack_offset = valid ? get_u64(state + 16) : 0;
    ack_seq = valid ? get_u64(state + 24) : 0;
    next_batch = valid ? get_u64(state + 32) : 1;
    if (valid &&
        (ack_offset > (uint64_t)spool->data_end ||
         ack_offset % GATEWAY_SPOOL_ENTRY_SIZE != 0 || next_batch == 0 ||
         ((ack_offset == 0) != (ack_seq == 0)))) {
        valid = false;
    }
    if (valid && ack_offset != 0) {
        uint64_t actual_seq = 0;

        if (record_seq_at(spool,
                          (off_t)ack_offset - GATEWAY_SPOOL_ENTRY_SIZE,
                          &actual_seq) != GATEWAY_OK || actual_seq != ack_seq) {
            valid = false;
        }
    }
    if (!valid) {
        spool->state_recoveries++;
        return GATEWAY_OK;
    }
    spool->ack_offset = (off_t)ack_offset;
    spool->last_acked_gateway_seq = ack_seq;
    spool->next_batch_seq = next_batch;
    return GATEWAY_OK;
}

static gateway_error_code fsync_parent_directory(const char *path)
{
    char parent[STATE_PATH_SIZE];
    char *slash;
    int fd;
    int result;

    if (strlen(path) >= sizeof(parent)) {
        return GATEWAY_ERROR_RANGE;
    }
    (void)snprintf(parent, sizeof(parent), "%s", path);
    slash = strrchr(parent, '/');
    if (slash == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    if (slash == parent) {
        slash[1] = '\0';
    } else {
        *slash = '\0';
    }
    fd = open(parent, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        return GATEWAY_ERROR_IO;
    }
    result = fsync(fd);
    if (close(fd) != 0) {
        result = -1;
    }
    return result == 0 ? GATEWAY_OK : GATEWAY_ERROR_IO;
}

static gateway_error_code persist_state(gateway_spool *spool,
                                        off_t ack_offset,
                                        uint64_t ack_seq,
                                        uint64_t next_batch)
{
    uint8_t state[STATE_SIZE];
    gateway_error_code code;
    int fd;
    int saved_errno;

    (void)memset(state, 0, sizeof(state));
    (void)memcpy(state, STATE_MAGIC, sizeof(STATE_MAGIC));
    put_u16(state + 4, 1);
    put_u16(state + 6, STATE_SIZE);
    put_u64(state + 16, (uint64_t)ack_offset);
    put_u64(state + 24, ack_seq);
    put_u64(state + 32, next_batch);
    put_u32(state + 8, crc32_bytes(state + 12, STATE_SIZE - 12));

    fd = open(spool->state_tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0640);
    if (fd < 0) {
        return GATEWAY_ERROR_IO;
    }
    code = write_exact(fd, state, sizeof(state), 0);
    if (code == GATEWAY_OK && fdatasync(fd) != 0) {
        code = GATEWAY_ERROR_IO;
    }
    saved_errno = errno;
    if (close(fd) != 0 && code == GATEWAY_OK) {
        code = GATEWAY_ERROR_IO;
        saved_errno = errno;
    }
    errno = saved_errno;
    if (code != GATEWAY_OK) {
        (void)unlink(spool->state_tmp_path);
        return code;
    }
    if (rename(spool->state_tmp_path, spool->state_path) != 0) {
        (void)unlink(spool->state_tmp_path);
        return GATEWAY_ERROR_IO;
    }
    return fsync_parent_directory(spool->state_path);
}

gateway_error_code gateway_spool_open(gateway_spool **spool,
                                      const char *data_path)
{
    gateway_spool *created;
    gateway_error_code code;

    if (spool == NULL || data_path == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    *spool = NULL;
    created = calloc(1, sizeof(*created));
    if (created == NULL) {
        return GATEWAY_ERROR_SYSTEM;
    }
    created->data_fd = -1;
    code = derive_paths(created, data_path);
    if (code != GATEWAY_OK) {
        gateway_spool_close(created);
        return code;
    }
    created->data_fd = open(created->data_path, O_RDWR | O_CREAT, 0640);
    if (created->data_fd < 0) {
        gateway_spool_close(created);
        return GATEWAY_ERROR_IO;
    }
    /* 单写者锁随进程异常退出由内核释放，避免两个 gatewayd 竞争游标。 */
    if (flock(created->data_fd, LOCK_EX | LOCK_NB) != 0) {
        gateway_spool_close(created);
        return GATEWAY_ERROR_CLOSED;
    }
    code = scan_data(created);
    if (code == GATEWAY_OK) {
        code = load_state(created);
    }
    if (code != GATEWAY_OK) {
        gateway_spool_close(created);
        return code;
    }
    *spool = created;
    return GATEWAY_OK;
}

void gateway_spool_v2_options_init(gateway_spool_v2_options *options)
{
    if (options == NULL) {
        return;
    }
    options->max_bytes = GATEWAY_SPOOL_MAX_BYTES_DEFAULT;
    options->sync_records = GATEWAY_SPOOL_SYNC_RECORDS_DEFAULT;
    options->sync_interval_ms = GATEWAY_SPOOL_SYNC_INTERVAL_MS_DEFAULT;
}

static gateway_error_code open_v2_dispatch(
    gateway_spool **spool,
    const char *directory,
    const gateway_spool_v2_options *options,
    uint64_t segment_records)
{
    gateway_spool *created;
    gateway_error_code code;

    if (spool == NULL || directory == NULL || options == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    *spool = NULL;
    created = calloc(1, sizeof(*created));
    if (created == NULL) {
        return GATEWAY_ERROR_SYSTEM;
    }
    created->data_fd = -1;
    created->v2 = true;
    code = gateway_spool_v2_open(&created->v2_spool, directory, options,
                                 segment_records);
    if (code != GATEWAY_OK) {
        gateway_spool_close(created);
        return code;
    }
    *spool = created;
    return GATEWAY_OK;
}

gateway_error_code gateway_spool_open_v2(
    gateway_spool **spool,
    const char *directory,
    const gateway_spool_v2_options *options)
{
    return open_v2_dispatch(spool, directory, options,
                            GATEWAY_SPOOL_V2_SEGMENT_RECORDS);
}

gateway_error_code gateway_spool_test_open_v2(
    gateway_spool **spool,
    const char *directory,
    const gateway_spool_v2_options *options,
    uint64_t segment_records)
{
    return open_v2_dispatch(spool, directory, options, segment_records);
}

void gateway_spool_close(gateway_spool *spool)
{
    if (spool == NULL) {
        return;
    }
    if (spool->v2) {
        gateway_spool_v2_close(spool->v2_spool);
    } else if (spool->data_fd >= 0) {
        (void)close(spool->data_fd);
    }
    free(spool);
}

gateway_error_code gateway_spool_append(gateway_spool *spool,
                                        const telemetry_record *record)
{
    uint8_t entry[GATEWAY_SPOOL_ENTRY_SIZE];
    off_t original_end;
    gateway_error_code code;

    if (spool != NULL && spool->v2) {
        return gateway_spool_v2_append(spool->v2_spool, record);
    }
    if (spool == NULL || record == NULL || record->gateway_seq == 0 ||
        (spool->last_gateway_seq != 0 &&
         record->gateway_seq <= spool->last_gateway_seq)) {
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    encode_record(entry, record);
    original_end = spool->data_end;
    code = write_exact(spool->data_fd, entry, sizeof(entry), original_end);
    if (code != GATEWAY_OK || fdatasync(spool->data_fd) != 0) {
        (void)ftruncate(spool->data_fd, original_end);
        return GATEWAY_ERROR_IO;
    }
    spool->data_end += GATEWAY_SPOOL_ENTRY_SIZE;
    spool->last_gateway_seq = record->gateway_seq;
    spool->records_appended++;
    return GATEWAY_OK;
}

gateway_error_code gateway_spool_prepare_batch(gateway_spool *spool,
                                               telemetry_record *records,
                                               size_t capacity,
                                               size_t *record_count,
                                               uint64_t *batch_seq)
{
    off_t offset;
    size_t count = 0;

    if (spool != NULL && spool->v2) {
        return gateway_spool_v2_prepare_batch(
            spool->v2_spool, records, capacity, record_count, batch_seq);
    }
    if (spool == NULL || records == NULL || capacity == 0 ||
        record_count == NULL || batch_seq == NULL || spool->batch_prepared) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    offset = spool->ack_offset;
    while (offset < spool->data_end && count < capacity) {
        uint8_t entry[GATEWAY_SPOOL_ENTRY_SIZE];
        gateway_error_code code = read_exact(spool->data_fd, entry,
                                             sizeof(entry), offset);

        if (code != GATEWAY_OK) {
            return code;
        }
        code = decode_record(entry, &records[count]);
        if (code != GATEWAY_OK ||
            (count != 0 &&
             records[count].gateway_seq <= records[count - 1].gateway_seq)) {
            spool->corruptions++;
            return GATEWAY_ERROR_INVALID_VALUE;
        }
        count++;
        offset += GATEWAY_SPOOL_ENTRY_SIZE;
    }
    *record_count = count;
    *batch_seq = spool->next_batch_seq;
    if (count != 0) {
        spool->prepared_end = offset;
        spool->batch_prepared = true;
        spool->records_replayed += (uint64_t)count;
    }
    return GATEWAY_OK;
}

gateway_error_code gateway_spool_ack_prepared(gateway_spool *spool)
{
    uint64_t last_seq;
    gateway_error_code code;

    if (spool != NULL && spool->v2) {
        return gateway_spool_v2_ack_prepared(spool->v2_spool);
    }
    if (spool == NULL || !spool->batch_prepared) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    code = record_seq_at(spool,
                         spool->prepared_end - GATEWAY_SPOOL_ENTRY_SIZE,
                         &last_seq);
    if (code != GATEWAY_OK) {
        spool->corruptions++;
        return code;
    }
    code = persist_state(spool, spool->prepared_end, last_seq,
                         spool->next_batch_seq + 1);
    if (code != GATEWAY_OK) {
        return code;
    }
    spool->ack_offset = spool->prepared_end;
    spool->last_acked_gateway_seq = last_seq;
    spool->next_batch_seq++;
    spool->batch_prepared = false;
    spool->prepared_end = 0;
    return GATEWAY_OK;
}

void gateway_spool_cancel_prepared(gateway_spool *spool)
{
    if (spool != NULL && spool->v2) {
        gateway_spool_v2_cancel_prepared(spool->v2_spool);
    } else if (spool != NULL) {
        spool->batch_prepared = false;
        spool->prepared_end = 0;
    }
}

uint64_t gateway_spool_last_gateway_seq(const gateway_spool *spool)
{
    if (spool == NULL) {
        return 0;
    }
    return spool->v2 ? gateway_spool_v2_last_gateway_seq(spool->v2_spool)
                     : spool->last_gateway_seq;
}

gateway_error_code gateway_spool_flush(gateway_spool *spool)
{
    if (spool == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    return spool->v2 ? gateway_spool_v2_flush(spool->v2_spool) : GATEWAY_OK;
}

gateway_error_code gateway_spool_poll(gateway_spool *spool)
{
    if (spool == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    return spool->v2 ? gateway_spool_v2_poll(spool->v2_spool) : GATEWAY_OK;
}

gateway_error_code gateway_spool_test_fail_next(
    gateway_spool *spool,
    gateway_spool_fault_point point)
{
    if (spool == NULL || !spool->v2) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    return gateway_spool_v2_fail_next(spool->v2_spool, point);
}

void gateway_spool_read(const gateway_spool *spool,
                        gateway_spool_snapshot *snapshot)
{
    if (spool == NULL || snapshot == NULL) {
        return;
    }
    if (spool->v2) {
        gateway_spool_v2_read(spool->v2_spool, snapshot);
        return;
    }
    (void)memset(snapshot, 0, sizeof(*snapshot));
    snapshot->total_records = (uint64_t)(spool->data_end /
                                         GATEWAY_SPOOL_ENTRY_SIZE);
    snapshot->pending_records = (uint64_t)((spool->data_end -
                                            spool->ack_offset) /
                                           GATEWAY_SPOOL_ENTRY_SIZE);
    snapshot->last_gateway_seq = spool->last_gateway_seq;
    snapshot->last_acked_gateway_seq = spool->last_acked_gateway_seq;
    snapshot->next_batch_seq = spool->next_batch_seq;
    snapshot->records_appended = spool->records_appended;
    snapshot->records_replayed = spool->records_replayed;
    snapshot->tail_recoveries = spool->tail_recoveries;
    snapshot->state_recoveries = spool->state_recoveries;
    snapshot->corruptions = spool->corruptions;
    snapshot->physical_bytes = (uint64_t)spool->data_end;
    snapshot->pending_bytes = (uint64_t)(spool->data_end - spool->ack_offset);
    snapshot->batch_prepared = spool->batch_prepared;
}
