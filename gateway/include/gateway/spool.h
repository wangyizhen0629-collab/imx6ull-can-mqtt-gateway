#ifndef GATEWAY_SPOOL_H
#define GATEWAY_SPOOL_H

#include "gateway/error.h"
#include "gateway/telemetry_record.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    GATEWAY_SPOOL_ENTRY_SIZE = 80
};

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
    bool batch_prepared;
} gateway_spool_snapshot;

gateway_error_code gateway_spool_open(gateway_spool **spool,
                                      const char *data_path);
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
uint64_t gateway_spool_last_gateway_seq(const gateway_spool *spool);
void gateway_spool_read(const gateway_spool *spool,
                        gateway_spool_snapshot *snapshot);

#endif
