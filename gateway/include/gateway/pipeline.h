#ifndef GATEWAY_PIPELINE_H
#define GATEWAY_PIPELINE_H

#include "gateway/can_receiver.h"
#include "gateway/error.h"
#include "gateway/lifecycle.h"
#include "gateway/stats.h"
#include "gateway/telemetry_record.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef gateway_error_code (*gateway_pipeline_receive_fn)(
    void *context,
    int timeout_ms,
    uint64_t gateway_seq,
    telemetry_record *record,
    gateway_can_reject_reason *reject_reason);

/*
 * receive 回调必须在 timeout_ms 内返回，使 request_stop 后的 join 有明确上界。
 * CLOSED 表示有限测试源正常耗尽；真实 CAN 源以 TIMEOUT 周期性检查退出请求。
 */

typedef gateway_error_code (*gateway_pipeline_consume_fn)(
    void *context,
    const telemetry_record *record);

typedef gateway_error_code (*gateway_pipeline_idle_fn)(void *context);

typedef struct {
    size_t queue_capacity;
    uint32_t queue_push_timeout_ms;
    int receive_timeout_ms;
    gateway_pipeline_receive_fn receive;
    void *receive_context;
    gateway_pipeline_consume_fn consume;
    void *consume_context;
    uint32_t consumer_idle_timeout_ms;
    gateway_pipeline_idle_fn consume_idle;
    gateway_lifecycle *lifecycle;
    gateway_stats *stats;
} gateway_pipeline_config;

typedef struct gateway_pipeline gateway_pipeline;

typedef struct {
    bool started;
    bool producer_done;
    bool consumer_done;
    bool joined;
    gateway_error_code producer_error;
    gateway_error_code consumer_error;
    uint64_t next_gateway_seq;
    size_t queue_capacity;
    size_t queue_count;
    bool queue_closed;
} gateway_pipeline_snapshot;

gateway_error_code gateway_pipeline_create(gateway_pipeline **pipeline,
                                           const gateway_pipeline_config *config);
void gateway_pipeline_destroy(gateway_pipeline *pipeline);
gateway_error_code gateway_pipeline_start(gateway_pipeline *pipeline);
gateway_error_code gateway_pipeline_request_stop(gateway_pipeline *pipeline,
                                                  int signal_number);
gateway_error_code gateway_pipeline_join(gateway_pipeline *pipeline);
void gateway_pipeline_read(gateway_pipeline *pipeline,
                           gateway_pipeline_snapshot *snapshot);

#endif
