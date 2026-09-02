#include "spool_v2_internal.h"

#include "gateway/config.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

enum {
    V2_HEADER_SIZE = 16,
    V2_PAYLOAD_SIZE = 64,
    V2_STATE_SIZE = 112,
    V2_SEGMENT_NAME_SIZE = 34
};

static const uint8_t V2_ENTRY_MAGIC[4] = {'G', 'S', 'P', '2'};
static const uint8_t V2_STATE_MAGIC[4] = {'G', 'S', 'T', '2'};

typedef struct {
    uint64_t number;
    uint64_t size;
    uint64_t first_seq;
    uint64_t last_seq;
} v2_segment;

struct gateway_spool_v2 {
    int directory_fd;
    int lock_fd;
    int segment_fd;
    uint64_t open_segment;
    char directory[GATEWAY_SPOOL_PATH_SIZE];
    gateway_spool_v2_options options;
    uint64_t segment_records;
    uint64_t segment_bytes;
    uint64_t generation;
    uint64_t last_allocated_seq;
    uint64_t sequence_fence;
    uint64_t last_acked_seq;
    uint64_t next_batch_seq;
    uint64_t ack_segment;
    uint64_t ack_offset;
    uint64_t write_segment;
    uint64_t write_offset;
    uint64_t prepared_segment;
    uint64_t prepared_offset;
    uint64_t prepared_last_seq;
    v2_segment *segments;
    size_t segment_count;
    size_t segment_capacity;
    uint64_t physical_bytes;
    uint64_t records_appended;
    uint64_t records_replayed;
    uint64_t tail_recoveries;
    uint64_t state_recoveries;
    uint64_t corruptions;
    uint64_t segments_reclaimed;
    uint64_t sync_count;
    uint64_t sync_failures;
    gateway_spool_fault_point fault_once;
    bool batch_prepared;
    bool failed;
};

static gateway_error_code read_record_at(gateway_spool_v2 *spool,
                                         uint64_t segment_number,
                                         uint64_t offset,
                                         telemetry_record *record);

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

static bool inject_fault(gateway_spool_v2 *spool,
                         gateway_spool_fault_point point)
{
    if (spool->fault_once != point) {
        return false;
    }
    spool->fault_once = GATEWAY_SPOOL_FAULT_NONE;
    errno = EIO;
    return true;
}

static gateway_error_code sync_descriptor(gateway_spool_v2 *spool,
                                          int descriptor,
                                          bool directory,
                                          gateway_spool_fault_point point)
{
    int result;

    if (inject_fault(spool, point)) {
        spool->sync_failures++;
        return GATEWAY_ERROR_IO;
    }
    result = directory ? fsync(descriptor) : fdatasync(descriptor);
    if (result != 0) {
        spool->sync_failures++;
        return GATEWAY_ERROR_IO;
    }
    spool->sync_count++;
    return GATEWAY_OK;
}

static gateway_error_code sync_directory(gateway_spool_v2 *spool)
{
    return sync_descriptor(spool, spool->directory_fd, true,
                           GATEWAY_SPOOL_FAULT_DIRECTORY_SYNC);
}

static void encode_record(uint8_t entry[GATEWAY_SPOOL_ENTRY_SIZE],
                          const telemetry_record *record)
{
    uint8_t *payload;

    (void)memset(entry, 0, GATEWAY_SPOOL_ENTRY_SIZE);
    (void)memcpy(entry, V2_ENTRY_MAGIC, sizeof(V2_ENTRY_MAGIC));
    put_u16(entry + 4, 2);
    put_u16(entry + 6, GATEWAY_SPOOL_ENTRY_SIZE);
    payload = entry + V2_HEADER_SIZE;
    put_u64(payload, record->gateway_seq);
    put_u64(payload + 8, (uint64_t)record->kernel_timestamp_ns);
    put_u32(payload + 16, record->can_id);
    (void)memcpy(payload + 20, record->data, GATEWAY_CAN_DATA_SIZE);
    put_u16(payload + 28, record->status_flags);
    payload[30] = record->dlc;
    payload[31] = record->ecu_counter;
    (void)memcpy(payload + 32, record->decoded_payload,
                 GATEWAY_DECODED_PAYLOAD_SIZE);
    put_u32(entry + 8, crc32_bytes(payload, V2_PAYLOAD_SIZE));
}

static gateway_error_code decode_record(
    const uint8_t entry[GATEWAY_SPOOL_ENTRY_SIZE],
    telemetry_record *record)
{
    const uint8_t *payload = entry + V2_HEADER_SIZE;

    if (memcmp(entry, V2_ENTRY_MAGIC, sizeof(V2_ENTRY_MAGIC)) != 0 ||
        get_u16(entry + 4) != 2 ||
        get_u16(entry + 6) != GATEWAY_SPOOL_ENTRY_SIZE ||
        get_u32(entry + 8) != crc32_bytes(payload, V2_PAYLOAD_SIZE) ||
        get_u32(entry + 12) != 0) {
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

static bool segment_name(uint64_t number, char name[V2_SEGMENT_NAME_SIZE])
{
    int written = snprintf(name, V2_SEGMENT_NAME_SIZE,
                           "segment-%020" PRIu64 ".gsp2", number);

    return written == V2_SEGMENT_NAME_SIZE - 1;
}

static bool parse_segment_name(const char *name, uint64_t *number)
{
    uint64_t value = 0;
    size_t index;

    if (strlen(name) != V2_SEGMENT_NAME_SIZE - 1 ||
        memcmp(name, "segment-", 8) != 0 ||
        memcmp(name + 28, ".gsp2", 5) != 0) {
        return false;
    }
    for (index = 8; index < 28; index++) {
        unsigned int digit;

        if (name[index] < '0' || name[index] > '9') {
            return false;
        }
        digit = (unsigned int)(name[index] - '0');
        if (value > (UINT64_MAX - digit) / 10U) {
            return false;
        }
        value = value * 10U + digit;
    }
    if (value == 0) {
        return false;
    }
    *number = value;
    return true;
}

static int compare_segments(const void *left, const void *right)
{
    const v2_segment *a = left;
    const v2_segment *b = right;

    return a->number < b->number ? -1 : a->number > b->number ? 1 : 0;
}

static gateway_error_code add_segment(gateway_spool_v2 *spool,
                                      uint64_t number)
{
    v2_segment *resized;

    if (spool->segment_count == spool->segment_capacity) {
        size_t new_capacity = spool->segment_capacity == 0
                                  ? 8
                                  : spool->segment_capacity * 2;

        if (new_capacity < spool->segment_capacity ||
            new_capacity > SIZE_MAX / sizeof(*resized)) {
            return GATEWAY_ERROR_RANGE;
        }
        resized = realloc(spool->segments, new_capacity * sizeof(*resized));
        if (resized == NULL) {
            return GATEWAY_ERROR_SYSTEM;
        }
        spool->segments = resized;
        spool->segment_capacity = new_capacity;
    }
    (void)memset(&spool->segments[spool->segment_count], 0,
                 sizeof(spool->segments[spool->segment_count]));
    spool->segments[spool->segment_count].number = number;
    spool->segment_count++;
    return GATEWAY_OK;
}

static v2_segment *find_segment(gateway_spool_v2 *spool, uint64_t number)
{
    size_t index;

    for (index = 0; index < spool->segment_count; index++) {
        if (spool->segments[index].number == number) {
            return &spool->segments[index];
        }
    }
    return NULL;
}

static gateway_error_code fsync_parent(const char *path)
{
    char parent[GATEWAY_SPOOL_PATH_SIZE];
    char *slash;
    int descriptor;
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
    descriptor = open(parent, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (descriptor < 0) {
        return GATEWAY_ERROR_IO;
    }
    result = fsync(descriptor);
    if (close(descriptor) != 0) {
        result = -1;
    }
    return result == 0 ? GATEWAY_OK : GATEWAY_ERROR_IO;
}

static gateway_error_code ensure_directory(gateway_spool_v2 *spool,
                                           const char *path)
{
    struct stat status;
    bool created = false;

    if (path[0] != '/' || path[1] == '\0' ||
        strlen(path) >= sizeof(spool->directory)) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    if (lstat(path, &status) != 0) {
        if (errno != ENOENT || mkdir(path, 0750) != 0) {
            return GATEWAY_ERROR_IO;
        }
        created = true;
    } else if (!S_ISDIR(status.st_mode)) {
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    if (created && fsync_parent(path) != GATEWAY_OK) {
        return GATEWAY_ERROR_IO;
    }
    (void)snprintf(spool->directory, sizeof(spool->directory), "%s", path);
    spool->directory_fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    return spool->directory_fd < 0 ? GATEWAY_ERROR_IO : GATEWAY_OK;
}

static gateway_error_code inspect_directory(gateway_spool_v2 *spool,
                                            bool *state_exists,
                                            bool *tmp_exists)
{
    DIR *directory;
    struct dirent *entry;
    int duplicate_fd = dup(spool->directory_fd);

    if (duplicate_fd < 0) {
        return GATEWAY_ERROR_IO;
    }
    directory = fdopendir(duplicate_fd);
    if (directory == NULL) {
        (void)close(duplicate_fd);
        return GATEWAY_ERROR_IO;
    }
    errno = 0;
    while ((entry = readdir(directory)) != NULL) {
        uint64_t number;
        gateway_error_code code;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0 ||
            strcmp(entry->d_name, "lock") == 0) {
            continue;
        }
        if (strcmp(entry->d_name, "state.v2") == 0) {
            *state_exists = true;
            continue;
        }
        if (strcmp(entry->d_name, "state.v2.tmp") == 0) {
            *tmp_exists = true;
            continue;
        }
        if (!parse_segment_name(entry->d_name, &number)) {
            (void)closedir(directory);
            return GATEWAY_ERROR_INVALID_VALUE;
        }
        code = add_segment(spool, number);
        if (code != GATEWAY_OK) {
            (void)closedir(directory);
            return code;
        }
    }
    if (errno != 0) {
        (void)closedir(directory);
        return GATEWAY_ERROR_IO;
    }
    if (closedir(directory) != 0) {
        return GATEWAY_ERROR_IO;
    }
    if (spool->segment_count > 1) {
        size_t index;

        qsort(spool->segments, spool->segment_count,
              sizeof(*spool->segments), compare_segments);
        for (index = 1; index < spool->segment_count; index++) {
            if (spool->segments[index - 1].number ==
                spool->segments[index].number) {
                return GATEWAY_ERROR_INVALID_VALUE;
            }
        }
    }
    return GATEWAY_OK;
}

static void encode_state(const gateway_spool_v2 *spool,
                         uint8_t state[V2_STATE_SIZE],
                         uint64_t generation)
{
    (void)memset(state, 0, V2_STATE_SIZE);
    (void)memcpy(state, V2_STATE_MAGIC, sizeof(V2_STATE_MAGIC));
    put_u16(state + 4, 2);
    put_u16(state + 6, V2_STATE_SIZE);
    put_u64(state + 16, generation);
    put_u64(state + 24, spool->last_allocated_seq);
    put_u64(state + 32, spool->sequence_fence);
    put_u64(state + 40, spool->last_acked_seq);
    put_u64(state + 48, spool->next_batch_seq);
    put_u64(state + 56, spool->ack_segment);
    put_u64(state + 64, spool->ack_offset);
    put_u64(state + 72, spool->write_segment);
    put_u64(state + 80, spool->write_offset);
    put_u64(state + 88, spool->segment_records);
    put_u32(state + 8, crc32_bytes(state + 12, V2_STATE_SIZE - 12));
}

static gateway_error_code persist_state(gateway_spool_v2 *spool)
{
    uint8_t state[V2_STATE_SIZE];
    gateway_error_code code;
    uint64_t generation;
    int descriptor;
    int saved_errno = 0;

    if (spool->generation == UINT64_MAX) {
        return GATEWAY_ERROR_RANGE;
    }
    generation = spool->generation + 1;
    encode_state(spool, state, generation);
    descriptor = openat(spool->directory_fd, "state.v2.tmp",
                        O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0640);
    if (descriptor < 0) {
        return GATEWAY_ERROR_IO;
    }
    if (inject_fault(spool, GATEWAY_SPOOL_FAULT_STATE_WRITE)) {
        code = GATEWAY_ERROR_IO;
    } else {
        code = write_exact(descriptor, state, sizeof(state), 0);
    }
    if (code == GATEWAY_OK) {
        code = sync_descriptor(spool, descriptor, false,
                               GATEWAY_SPOOL_FAULT_STATE_SYNC);
    }
    if (code != GATEWAY_OK) {
        saved_errno = errno;
    }
    if (close(descriptor) != 0 && code == GATEWAY_OK) {
        code = GATEWAY_ERROR_IO;
        saved_errno = errno;
    }
    if (code != GATEWAY_OK) {
        (void)unlinkat(spool->directory_fd, "state.v2.tmp", 0);
        errno = saved_errno;
        return code;
    }
    if (inject_fault(spool, GATEWAY_SPOOL_FAULT_STATE_RENAME) ||
        renameat(spool->directory_fd, "state.v2.tmp", spool->directory_fd,
                 "state.v2") != 0) {
        (void)unlinkat(spool->directory_fd, "state.v2.tmp", 0);
        return GATEWAY_ERROR_IO;
    }
    code = sync_directory(spool);
    if (code == GATEWAY_OK) {
        spool->generation = generation;
    }
    return code;
}

static gateway_error_code load_state(gateway_spool_v2 *spool)
{
    uint8_t state[V2_STATE_SIZE];
    struct stat status;
    int descriptor = openat(spool->directory_fd, "state.v2",
                            O_RDONLY | O_CLOEXEC);
    gateway_error_code code = GATEWAY_OK;

    if (descriptor < 0) {
        return errno == ENOENT ? GATEWAY_ERROR_CLOSED : GATEWAY_ERROR_IO;
    }
    if (fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode) ||
        status.st_size != V2_STATE_SIZE ||
        read_exact(descriptor, state, sizeof(state), 0) != GATEWAY_OK) {
        code = GATEWAY_ERROR_INVALID_VALUE;
    }
    if (close(descriptor) != 0 && code == GATEWAY_OK) {
        code = GATEWAY_ERROR_IO;
    }
    if (code != GATEWAY_OK) {
        return code;
    }
    if (memcmp(state, V2_STATE_MAGIC, sizeof(V2_STATE_MAGIC)) != 0 ||
        get_u16(state + 4) != 2 || get_u16(state + 6) != V2_STATE_SIZE ||
        get_u32(state + 8) != crc32_bytes(state + 12, V2_STATE_SIZE - 12) ||
        get_u32(state + 12) != 0 || get_u64(state + 96) != 0 ||
        get_u64(state + 104) != 0) {
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    spool->generation = get_u64(state + 16);
    spool->last_allocated_seq = get_u64(state + 24);
    spool->sequence_fence = get_u64(state + 32);
    spool->last_acked_seq = get_u64(state + 40);
    spool->next_batch_seq = get_u64(state + 48);
    spool->ack_segment = get_u64(state + 56);
    spool->ack_offset = get_u64(state + 64);
    spool->write_segment = get_u64(state + 72);
    spool->write_offset = get_u64(state + 80);
    if (spool->generation == 0 || spool->next_batch_seq == 0 ||
        spool->ack_segment == 0 || spool->write_segment == 0 ||
        spool->ack_segment > spool->write_segment ||
        spool->ack_offset % GATEWAY_SPOOL_ENTRY_SIZE != 0 ||
        spool->write_offset % GATEWAY_SPOOL_ENTRY_SIZE != 0 ||
        spool->ack_offset > spool->segment_bytes ||
        spool->write_offset > spool->segment_bytes ||
        get_u64(state + 88) != spool->segment_records ||
        spool->last_acked_seq > spool->last_allocated_seq ||
        spool->last_allocated_seq > spool->sequence_fence) {
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    return GATEWAY_OK;
}

static gateway_error_code open_segment_fd(gateway_spool_v2 *spool,
                                          uint64_t number,
                                          int flags,
                                          int *descriptor)
{
    char name[V2_SEGMENT_NAME_SIZE];

    if (!segment_name(number, name)) {
        return GATEWAY_ERROR_RANGE;
    }
    *descriptor = openat(spool->directory_fd, name, flags | O_CLOEXEC, 0640);
    return *descriptor < 0 ? GATEWAY_ERROR_IO : GATEWAY_OK;
}

static gateway_error_code scan_one_segment(gateway_spool_v2 *spool,
                                           v2_segment *segment,
                                           bool final_segment,
                                           uint64_t *previous_seq)
{
    struct stat status;
    uint64_t valid_size;
    uint64_t offset;
    int descriptor;
    gateway_error_code code;

    code = open_segment_fd(spool, segment->number, O_RDWR, &descriptor);
    if (code != GATEWAY_OK) {
        return code;
    }
    if (fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode) ||
        status.st_size < 0 || (uint64_t)status.st_size > spool->segment_bytes) {
        (void)close(descriptor);
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    valid_size = (uint64_t)status.st_size -
                 (uint64_t)status.st_size % GATEWAY_SPOOL_ENTRY_SIZE;
    if (valid_size != (uint64_t)status.st_size && !final_segment) {
        (void)close(descriptor);
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    for (offset = 0; offset < valid_size; offset += GATEWAY_SPOOL_ENTRY_SIZE) {
        uint8_t entry[GATEWAY_SPOOL_ENTRY_SIZE];
        telemetry_record record;

        code = read_exact(descriptor, entry, sizeof(entry), (off_t)offset);
        if (code == GATEWAY_OK) {
            code = decode_record(entry, &record);
        }
        if (code != GATEWAY_OK ||
            (*previous_seq != 0 && record.gateway_seq <= *previous_seq)) {
            if (!final_segment || offset + GATEWAY_SPOOL_ENTRY_SIZE !=
                                      valid_size) {
                spool->corruptions++;
                (void)close(descriptor);
                return GATEWAY_ERROR_INVALID_VALUE;
            }
            valid_size = offset;
            break;
        }
        if (segment->first_seq == 0) {
            segment->first_seq = record.gateway_seq;
        }
        segment->last_seq = record.gateway_seq;
        *previous_seq = record.gateway_seq;
    }
    if (valid_size != (uint64_t)status.st_size) {
        if (ftruncate(descriptor, (off_t)valid_size) != 0) {
            (void)close(descriptor);
            return GATEWAY_ERROR_IO;
        }
        code = sync_descriptor(spool, descriptor, false,
                               GATEWAY_SPOOL_FAULT_SEGMENT_SYNC);
        if (code != GATEWAY_OK) {
            (void)close(descriptor);
            return code;
        }
        spool->tail_recoveries++;
    }
    if (close(descriptor) != 0) {
        return GATEWAY_ERROR_IO;
    }
    segment->size = valid_size;
    spool->physical_bytes += valid_size;
    return GATEWAY_OK;
}

static gateway_error_code validate_and_scan(gateway_spool_v2 *spool)
{
    uint64_t previous_seq = 0;
    uint64_t actual_last_seq = 0;
    uint64_t actual_pending_last_seq = 0;
    size_t index;
    bool reconcile = false;

    for (index = 0; index < spool->segment_count; index++) {
        v2_segment *segment = &spool->segments[index];
        bool final_segment = index + 1 == spool->segment_count;
        gateway_error_code code = scan_one_segment(
            spool, segment, final_segment &&
                                segment->number == spool->write_segment,
            &previous_seq);

        if (code != GATEWAY_OK) {
            return code;
        }
        if (segment->number > spool->write_segment) {
            return GATEWAY_ERROR_INVALID_VALUE;
        }
        if (segment->number >= spool->ack_segment &&
            segment->number < spool->write_segment &&
            segment->size != spool->segment_bytes) {
            return GATEWAY_ERROR_INVALID_VALUE;
        }
        if (index > 0 && segment->number >= spool->ack_segment &&
            spool->segments[index - 1].number >= spool->ack_segment &&
            segment->number != spool->segments[index - 1].number + 1) {
            return GATEWAY_ERROR_INVALID_VALUE;
        }
        if (segment->last_seq != 0) {
            actual_last_seq = segment->last_seq;
            if (segment->number >= spool->ack_segment) {
                actual_pending_last_seq = segment->last_seq;
            }
        }
    }
    {
        v2_segment *write = find_segment(spool, spool->write_segment);
        v2_segment *ack = find_segment(spool, spool->ack_segment);

        if (write == NULL) {
            if (spool->write_offset != 0) {
                return GATEWAY_ERROR_INVALID_VALUE;
            }
        } else if (write->size < spool->write_offset) {
            return GATEWAY_ERROR_INVALID_VALUE;
        } else if (write->size > spool->write_offset) {
            if (write->last_seq > spool->sequence_fence) {
                return GATEWAY_ERROR_INVALID_VALUE;
            }
            spool->write_offset = write->size;
            spool->last_allocated_seq = write->last_seq;
            spool->sequence_fence = write->last_seq;
            if (spool->write_offset == spool->segment_bytes) {
                if (spool->write_segment == UINT64_MAX) {
                    return GATEWAY_ERROR_RANGE;
                }
                spool->write_segment++;
                spool->write_offset = 0;
            }
            reconcile = true;
        }
        if (spool->ack_segment < spool->write_segment && ack == NULL) {
            return GATEWAY_ERROR_INVALID_VALUE;
        }
        if (ack != NULL && spool->ack_offset > ack->size) {
            return GATEWAY_ERROR_INVALID_VALUE;
        }
        if (ack != NULL && spool->ack_offset != 0) {
            telemetry_record record;
            gateway_error_code code = read_record_at(
                spool, spool->ack_segment,
                spool->ack_offset - GATEWAY_SPOOL_ENTRY_SIZE, &record);

            if (code != GATEWAY_OK ||
                record.gateway_seq != spool->last_acked_seq) {
                return GATEWAY_ERROR_INVALID_VALUE;
            }
        }
    }
    if (actual_last_seq > spool->sequence_fence ||
        (actual_pending_last_seq != 0 &&
         actual_pending_last_seq < spool->last_acked_seq)) {
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    if (reconcile) {
        spool->state_recoveries++;
        return persist_state(spool);
    }
    return GATEWAY_OK;
}

static gateway_error_code remove_reclaimed_segments(gateway_spool_v2 *spool)
{
    size_t source;
    size_t destination = 0;
    bool removed = false;

    for (source = 0; source < spool->segment_count; source++) {
        v2_segment segment = spool->segments[source];

        if (segment.number < spool->ack_segment) {
            char name[V2_SEGMENT_NAME_SIZE];

            if (!segment_name(segment.number, name)) {
                return GATEWAY_ERROR_RANGE;
            }
            if (inject_fault(spool, GATEWAY_SPOOL_FAULT_SEGMENT_DELETE) ||
                unlinkat(spool->directory_fd, name, 0) != 0) {
                return GATEWAY_ERROR_IO;
            }
            spool->physical_bytes -= segment.size;
            spool->segments_reclaimed++;
            removed = true;
        } else {
            spool->segments[destination++] = segment;
        }
    }
    spool->segment_count = destination;
    return removed ? sync_directory(spool) : GATEWAY_OK;
}

static uint64_t pending_records(const gateway_spool_v2 *spool)
{
    uint64_t count = 0;
    size_t index;

    for (index = 0; index < spool->segment_count; index++) {
        const v2_segment *segment = &spool->segments[index];

        if (segment->number < spool->ack_segment) {
            continue;
        }
        if (segment->number == spool->ack_segment) {
            if (segment->size > spool->ack_offset) {
                count += (segment->size - spool->ack_offset) /
                         GATEWAY_SPOOL_ENTRY_SIZE;
            }
        } else {
            count += segment->size / GATEWAY_SPOOL_ENTRY_SIZE;
        }
    }
    return count;
}

static gateway_error_code ensure_write_segment(gateway_spool_v2 *spool)
{
    v2_segment *segment;
    gateway_error_code code;

    if (spool->segment_fd >= 0 &&
        spool->open_segment == spool->write_segment) {
        return GATEWAY_OK;
    }
    if (spool->segment_fd >= 0) {
        if (close(spool->segment_fd) != 0) {
            spool->segment_fd = -1;
            return GATEWAY_ERROR_IO;
        }
        spool->segment_fd = -1;
    }
    segment = find_segment(spool, spool->write_segment);
    if (segment == NULL) {
        code = open_segment_fd(spool, spool->write_segment,
                               O_RDWR | O_CREAT | O_EXCL,
                               &spool->segment_fd);
        if (code != GATEWAY_OK) {
            return code;
        }
        code = add_segment(spool, spool->write_segment);
        if (code != GATEWAY_OK) {
            (void)close(spool->segment_fd);
            spool->segment_fd = -1;
            return code;
        }
        qsort(spool->segments, spool->segment_count,
              sizeof(*spool->segments), compare_segments);
        code = sync_directory(spool);
        if (code != GATEWAY_OK) {
            return code;
        }
    } else {
        code = open_segment_fd(spool, spool->write_segment, O_RDWR,
                               &spool->segment_fd);
        if (code != GATEWAY_OK) {
            return code;
        }
    }
    spool->open_segment = spool->write_segment;
    return GATEWAY_OK;
}

gateway_error_code gateway_spool_v2_open(
    gateway_spool_v2 **spool,
    const char *directory,
    const gateway_spool_v2_options *options,
    uint64_t segment_records)
{
    gateway_spool_v2 *created;
    gateway_error_code code;
    bool state_exists = false;
    bool tmp_exists = false;

    if (spool == NULL || directory == NULL || options == NULL ||
        segment_records == 0 ||
        segment_records > GATEWAY_SPOOL_V2_SEGMENT_RECORDS ||
        options->max_bytes < segment_records * GATEWAY_SPOOL_ENTRY_SIZE ||
        options->max_bytes > GATEWAY_SPOOL_MAX_BYTES_MAX ||
        options->sync_records != 1 || options->sync_interval_ms == 0 ||
        options->sync_interval_ms > 60000U) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    *spool = NULL;
    created = calloc(1, sizeof(*created));
    if (created == NULL) {
        return GATEWAY_ERROR_SYSTEM;
    }
    created->directory_fd = -1;
    created->lock_fd = -1;
    created->segment_fd = -1;
    created->options = *options;
    created->segment_records = segment_records;
    created->segment_bytes = segment_records * GATEWAY_SPOOL_ENTRY_SIZE;
    code = ensure_directory(created, directory);
    if (code == GATEWAY_OK) {
        code = inspect_directory(created, &state_exists, &tmp_exists);
    }
    if (code == GATEWAY_OK) {
        created->lock_fd = openat(created->directory_fd, "lock",
                                  O_RDWR | O_CREAT | O_CLOEXEC, 0640);
        if (created->lock_fd < 0) {
            code = GATEWAY_ERROR_IO;
        } else if (flock(created->lock_fd, LOCK_EX | LOCK_NB) != 0) {
            code = GATEWAY_ERROR_CLOSED;
        }
    }
    if (code == GATEWAY_OK) {
        code = sync_directory(created);
    }
    if (code == GATEWAY_OK && tmp_exists) {
        if (unlinkat(created->directory_fd, "state.v2.tmp", 0) != 0) {
            code = GATEWAY_ERROR_IO;
        } else {
            code = sync_directory(created);
        }
    }
    if (code == GATEWAY_OK && !state_exists) {
        if (created->segment_count != 0) {
            code = GATEWAY_ERROR_INVALID_VALUE;
        } else {
            created->next_batch_seq = 1;
            created->ack_segment = 1;
            created->write_segment = 1;
            code = persist_state(created);
        }
    } else if (code == GATEWAY_OK) {
        code = load_state(created);
    }
    if (code == GATEWAY_OK) {
        code = validate_and_scan(created);
    }
    if (code == GATEWAY_OK) {
        code = remove_reclaimed_segments(created);
    }
    if (code != GATEWAY_OK) {
        gateway_spool_v2_close(created);
        return code;
    }
    *spool = created;
    return GATEWAY_OK;
}

void gateway_spool_v2_close(gateway_spool_v2 *spool)
{
    if (spool == NULL) {
        return;
    }
    if (!spool->failed) {
        (void)gateway_spool_v2_flush(spool);
    }
    if (spool->segment_fd >= 0) {
        (void)close(spool->segment_fd);
    }
    if (spool->lock_fd >= 0) {
        (void)close(spool->lock_fd);
    }
    if (spool->directory_fd >= 0) {
        (void)close(spool->directory_fd);
    }
    free(spool->segments);
    free(spool);
}

gateway_error_code gateway_spool_v2_append(gateway_spool_v2 *spool,
                                            const telemetry_record *record)
{
    uint8_t entry[GATEWAY_SPOOL_ENTRY_SIZE];
    v2_segment *segment;
    gateway_error_code code;
    uint64_t original_offset;

    if (spool == NULL || record == NULL || record->gateway_seq == 0) {
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    if (spool->failed) {
        return GATEWAY_ERROR_CLOSED;
    }
    if (record->gateway_seq <= spool->last_allocated_seq ||
        record->gateway_seq <= spool->sequence_fence) {
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    if (spool->physical_bytes > spool->options.max_bytes -
                                    GATEWAY_SPOOL_ENTRY_SIZE) {
        return GATEWAY_ERROR_CAPACITY;
    }
    spool->sequence_fence = record->gateway_seq;
    code = persist_state(spool);
    if (code != GATEWAY_OK) {
        spool->failed = true;
        return code;
    }
    code = ensure_write_segment(spool);
    if (code != GATEWAY_OK) {
        spool->failed = true;
        return code;
    }
    original_offset = spool->write_offset;
    encode_record(entry, record);
    if (inject_fault(spool, GATEWAY_SPOOL_FAULT_APPEND_WRITE)) {
        code = GATEWAY_ERROR_IO;
    } else {
        code = write_exact(spool->segment_fd, entry, sizeof(entry),
                           (off_t)spool->write_offset);
    }
    if (code == GATEWAY_OK) {
        code = sync_descriptor(spool, spool->segment_fd, false,
                               GATEWAY_SPOOL_FAULT_SEGMENT_SYNC);
    }
    if (code != GATEWAY_OK) {
        (void)ftruncate(spool->segment_fd, (off_t)original_offset);
        spool->failed = true;
        return code;
    }
    segment = find_segment(spool, spool->write_segment);
    if (segment == NULL) {
        spool->failed = true;
        return GATEWAY_ERROR_SYSTEM;
    }
    if (segment->first_seq == 0) {
        segment->first_seq = record->gateway_seq;
    }
    segment->last_seq = record->gateway_seq;
    segment->size += GATEWAY_SPOOL_ENTRY_SIZE;
    spool->physical_bytes += GATEWAY_SPOOL_ENTRY_SIZE;
    spool->write_offset += GATEWAY_SPOOL_ENTRY_SIZE;
    spool->last_allocated_seq = record->gateway_seq;
    spool->records_appended++;
    if (spool->write_offset == spool->segment_bytes) {
        if (spool->write_segment == UINT64_MAX) {
            spool->failed = true;
            return GATEWAY_ERROR_RANGE;
        }
        if (close(spool->segment_fd) != 0) {
            spool->segment_fd = -1;
            spool->failed = true;
            return GATEWAY_ERROR_IO;
        }
        spool->segment_fd = -1;
        spool->open_segment = 0;
        spool->write_segment++;
        spool->write_offset = 0;
    }
    code = persist_state(spool);
    if (code != GATEWAY_OK) {
        spool->failed = true;
        return code;
    }
    return GATEWAY_OK;
}

static gateway_error_code read_record_at(gateway_spool_v2 *spool,
                                         uint64_t segment_number,
                                         uint64_t offset,
                                         telemetry_record *record)
{
    uint8_t entry[GATEWAY_SPOOL_ENTRY_SIZE];
    int descriptor;
    gateway_error_code code;

    code = open_segment_fd(spool, segment_number, O_RDONLY, &descriptor);
    if (code != GATEWAY_OK) {
        return code;
    }
    code = read_exact(descriptor, entry, sizeof(entry), (off_t)offset);
    if (close(descriptor) != 0 && code == GATEWAY_OK) {
        code = GATEWAY_ERROR_IO;
    }
    if (code == GATEWAY_OK) {
        code = decode_record(entry, record);
    }
    return code;
}

gateway_error_code gateway_spool_v2_prepare_batch(gateway_spool_v2 *spool,
                                                  telemetry_record *records,
                                                  size_t capacity,
                                                  size_t *record_count,
                                                  uint64_t *batch_seq)
{
    uint64_t segment_number;
    uint64_t offset;
    size_t count = 0;
    gateway_error_code code;

    if (spool == NULL || records == NULL || capacity == 0 ||
        record_count == NULL || batch_seq == NULL || spool->batch_prepared) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    if (spool->failed) {
        return GATEWAY_ERROR_CLOSED;
    }
    code = gateway_spool_v2_flush(spool);
    if (code != GATEWAY_OK) {
        return code;
    }
    segment_number = spool->ack_segment;
    offset = spool->ack_offset;
    while (count < capacity) {
        v2_segment *segment = find_segment(spool, segment_number);

        if (segment == NULL || offset >= segment->size) {
            if (segment_number < spool->write_segment) {
                segment_number++;
                offset = 0;
                continue;
            }
            break;
        }
        code = read_record_at(spool, segment_number, offset, &records[count]);
        if (code != GATEWAY_OK ||
            (count != 0 &&
             records[count].gateway_seq <= records[count - 1].gateway_seq)) {
            spool->corruptions++;
            spool->failed = true;
            return code == GATEWAY_OK ? GATEWAY_ERROR_INVALID_VALUE : code;
        }
        count++;
        offset += GATEWAY_SPOOL_ENTRY_SIZE;
        if (offset == segment->size &&
            segment->number < spool->write_segment) {
            segment_number++;
            offset = 0;
        }
    }
    *record_count = count;
    *batch_seq = spool->next_batch_seq;
    if (count != 0) {
        spool->prepared_segment = segment_number;
        spool->prepared_offset = offset;
        spool->prepared_last_seq = records[count - 1].gateway_seq;
        spool->batch_prepared = true;
        spool->records_replayed += (uint64_t)count;
    }
    return GATEWAY_OK;
}

gateway_error_code gateway_spool_v2_ack_prepared(gateway_spool_v2 *spool)
{
    uint64_t old_ack_segment;
    uint64_t old_ack_offset;
    uint64_t old_last_acked;
    uint64_t old_next_batch;
    uint64_t old_write_segment;
    uint64_t old_write_offset;
    gateway_error_code code;

    if (spool == NULL || !spool->batch_prepared) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    if (spool->failed || spool->next_batch_seq == UINT64_MAX) {
        return spool->failed ? GATEWAY_ERROR_CLOSED : GATEWAY_ERROR_RANGE;
    }
    old_ack_segment = spool->ack_segment;
    old_ack_offset = spool->ack_offset;
    old_last_acked = spool->last_acked_seq;
    old_next_batch = spool->next_batch_seq;
    old_write_segment = spool->write_segment;
    old_write_offset = spool->write_offset;
    spool->ack_segment = spool->prepared_segment;
    spool->ack_offset = spool->prepared_offset;
    spool->last_acked_seq = spool->prepared_last_seq;
    spool->next_batch_seq++;
    if (spool->ack_segment == spool->write_segment &&
        spool->ack_offset == spool->write_offset &&
        spool->write_offset != 0) {
        if (spool->write_segment == UINT64_MAX) {
            return GATEWAY_ERROR_RANGE;
        }
        if (spool->segment_fd >= 0) {
            if (close(spool->segment_fd) != 0) {
                spool->segment_fd = -1;
                spool->failed = true;
                return GATEWAY_ERROR_IO;
            }
            spool->segment_fd = -1;
            spool->open_segment = 0;
        }
        spool->write_segment++;
        spool->write_offset = 0;
        spool->ack_segment = spool->write_segment;
        spool->ack_offset = 0;
    }
    code = persist_state(spool);
    if (code != GATEWAY_OK) {
        spool->ack_segment = old_ack_segment;
        spool->ack_offset = old_ack_offset;
        spool->last_acked_seq = old_last_acked;
        spool->next_batch_seq = old_next_batch;
        spool->write_segment = old_write_segment;
        spool->write_offset = old_write_offset;
        spool->failed = true;
        return code;
    }
    spool->batch_prepared = false;
    spool->prepared_segment = 0;
    spool->prepared_offset = 0;
    spool->prepared_last_seq = 0;
    code = remove_reclaimed_segments(spool);
    if (code != GATEWAY_OK) {
        spool->failed = true;
        return code;
    }
    return GATEWAY_OK;
}

void gateway_spool_v2_cancel_prepared(gateway_spool_v2 *spool)
{
    if (spool == NULL) {
        return;
    }
    spool->batch_prepared = false;
    spool->prepared_segment = 0;
    spool->prepared_offset = 0;
    spool->prepared_last_seq = 0;
}

gateway_error_code gateway_spool_v2_flush(gateway_spool_v2 *spool)
{
    if (spool == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    return spool->failed ? GATEWAY_ERROR_CLOSED : GATEWAY_OK;
}

gateway_error_code gateway_spool_v2_poll(gateway_spool_v2 *spool)
{
    return gateway_spool_v2_flush(spool);
}

uint64_t gateway_spool_v2_last_gateway_seq(const gateway_spool_v2 *spool)
{
    if (spool == NULL) {
        return 0;
    }
    return spool->sequence_fence > spool->last_allocated_seq
               ? spool->sequence_fence
               : spool->last_allocated_seq;
}

void gateway_spool_v2_read(const gateway_spool_v2 *spool,
                           gateway_spool_snapshot *snapshot)
{
    uint64_t pending;

    if (spool == NULL || snapshot == NULL) {
        return;
    }
    pending = pending_records(spool);
    (void)memset(snapshot, 0, sizeof(*snapshot));
    snapshot->total_records = spool->physical_bytes /
                              GATEWAY_SPOOL_ENTRY_SIZE;
    snapshot->pending_records = pending;
    snapshot->last_gateway_seq = gateway_spool_v2_last_gateway_seq(spool);
    snapshot->last_acked_gateway_seq = spool->last_acked_seq;
    snapshot->next_batch_seq = spool->next_batch_seq;
    snapshot->records_appended = spool->records_appended;
    snapshot->records_replayed = spool->records_replayed;
    snapshot->tail_recoveries = spool->tail_recoveries;
    snapshot->state_recoveries = spool->state_recoveries;
    snapshot->corruptions = spool->corruptions;
    snapshot->physical_bytes = spool->physical_bytes;
    snapshot->pending_bytes = pending * GATEWAY_SPOOL_ENTRY_SIZE;
    snapshot->segment_count = spool->segment_count;
    snapshot->segments_reclaimed = spool->segments_reclaimed;
    snapshot->sync_count = spool->sync_count;
    snapshot->sync_failures = spool->sync_failures;
    snapshot->v2 = true;
    snapshot->batch_prepared = spool->batch_prepared;
}

gateway_error_code gateway_spool_v2_fail_next(
    gateway_spool_v2 *spool,
    gateway_spool_fault_point point)
{
    if (spool == NULL || point <= GATEWAY_SPOOL_FAULT_NONE ||
        point > GATEWAY_SPOOL_FAULT_SEGMENT_DELETE ||
        spool->fault_once != GATEWAY_SPOOL_FAULT_NONE) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    spool->fault_once = point;
    return GATEWAY_OK;
}
