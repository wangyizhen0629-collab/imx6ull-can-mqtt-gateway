#ifndef GATEWAY_SPOOL_H
#define GATEWAY_SPOOL_H

#include "gateway/error.h"
#include "gateway/telemetry_record.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    GATEWAY_SPOOL_ENTRY_SIZE = 80,
    GATEWAY_SPOOL_V2_SEGMENT_RECORDS = 65536,
    GATEWAY_SPOOL_SYNC_RECORDS_DEFAULT = 1,
    GATEWAY_SPOOL_SYNC_INTERVAL_MS_DEFAULT = 1000
};

#define GATEWAY_SPOOL_MAX_BYTES_DEFAULT UINT64_C(268435456)
#define GATEWAY_SPOOL_MAX_BYTES_MIN UINT64_C(5242880)
#define GATEWAY_SPOOL_MAX_BYTES_MAX UINT64_C(68719476736)

typedef enum {
    GATEWAY_SPOOL_FORMAT_LEGACY = 0,
    GATEWAY_SPOOL_FORMAT_V2 = 1
} gateway_spool_format;

typedef struct {
    uint64_t max_bytes;
    uint32_t sync_records;
    uint32_t sync_interval_ms;
} gateway_spool_v2_options;

typedef enum {
    GATEWAY_SPOOL_FAULT_NONE = 0,
    GATEWAY_SPOOL_FAULT_APPEND_WRITE,
    GATEWAY_SPOOL_FAULT_SEGMENT_SYNC,
    GATEWAY_SPOOL_FAULT_STATE_WRITE,
    GATEWAY_SPOOL_FAULT_STATE_SYNC,
    GATEWAY_SPOOL_FAULT_STATE_RENAME,
    GATEWAY_SPOOL_FAULT_DIRECTORY_SYNC,
    GATEWAY_SPOOL_FAULT_SEGMENT_DELETE
} gateway_spool_fault_point;

typedef struct gateway_spool gateway_spool;

typedef struct {
    uint64_t total_records;
    uint64_t pending_records;
    uint64_t last_gateway_seq;
    uint64_t last_acked_gateway_seq;
    uint64_t next_batch_seq;
    uint64_t records_appended;
    uint64_t records_replayed;
    uint64_t tail_recoveries;
    uint64_t state_recoveries;
    uint64_t corruptions;
    uint64_t physical_bytes;
    uint64_t pending_bytes;
    uint64_t segment_count;
    uint64_t segments_reclaimed;
    uint64_t sync_count;
    uint64_t sync_failures;
    uint64_t unsynced_records;
    bool v2;
    bool batch_prepared;
} gateway_spool_snapshot;

gateway_error_code gateway_spool_open(gateway_spool **spool,
                                      const char *data_path);
void gateway_spool_v2_options_init(gateway_spool_v2_options *options);
gateway_error_code gateway_spool_open_v2(
    gateway_spool **spool,
    const char *directory,
    const gateway_spool_v2_options *options);
void gateway_spool_close(gateway_spool *spool);
gateway_error_code gateway_spool_append(gateway_spool *spool,
                                        const telemetry_record *record);
gateway_error_code gateway_spool_prepare_batch(gateway_spool *spool,
                                               telemetry_record *records,
                                               size_t capacity,
                                               size_t *record_count,
                                               uint64_t *batch_seq);
gateway_error_code gateway_spool_ack_prepared(gateway_spool *spool);
void gateway_spool_cancel_prepared(gateway_spool *spool);
gateway_error_code gateway_spool_flush(gateway_spool *spool);
gateway_error_code gateway_spool_poll(gateway_spool *spool);
uint64_t gateway_spool_last_gateway_seq(const gateway_spool *spool);
void gateway_spool_read(const gateway_spool *spool,
                        gateway_spool_snapshot *snapshot);

/* 仅供离线单元测试缩短滚动边界和注入一次性 I/O 故障。 */
gateway_error_code gateway_spool_test_open_v2(
    gateway_spool **spool,
    const char *directory,
    const gateway_spool_v2_options *options,
    uint64_t segment_records);
gateway_error_code gateway_spool_test_fail_next(
    gateway_spool *spool,
    gateway_spool_fault_point point);

#endif
