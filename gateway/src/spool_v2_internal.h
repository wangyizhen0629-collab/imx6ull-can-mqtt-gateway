#ifndef GATEWAY_SPOOL_V2_INTERNAL_H
#define GATEWAY_SPOOL_V2_INTERNAL_H

#include "gateway/spool.h"

typedef struct gateway_spool_v2 gateway_spool_v2;

gateway_error_code gateway_spool_v2_open(
    gateway_spool_v2 **spool,
    const char *directory,
    const gateway_spool_v2_options *options,
    uint64_t segment_records);
void gateway_spool_v2_close(gateway_spool_v2 *spool);
gateway_error_code gateway_spool_v2_append(gateway_spool_v2 *spool,
                                            const telemetry_record *record);
gateway_error_code gateway_spool_v2_prepare_batch(gateway_spool_v2 *spool,
                                                  telemetry_record *records,
                                                  size_t capacity,
                                                  size_t *record_count,
                                                  uint64_t *batch_seq);
gateway_error_code gateway_spool_v2_ack_prepared(gateway_spool_v2 *spool);
void gateway_spool_v2_cancel_prepared(gateway_spool_v2 *spool);
gateway_error_code gateway_spool_v2_flush(gateway_spool_v2 *spool);
gateway_error_code gateway_spool_v2_poll(gateway_spool_v2 *spool);
uint64_t gateway_spool_v2_last_gateway_seq(const gateway_spool_v2 *spool);
void gateway_spool_v2_read(const gateway_spool_v2 *spool,
                           gateway_spool_snapshot *snapshot);
gateway_error_code gateway_spool_v2_fail_next(
    gateway_spool_v2 *spool,
    gateway_spool_fault_point point);

#endif
