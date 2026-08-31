#include "gateway/can_receiver.h"
#include "gateway/config.h"
#include "gateway/lifecycle.h"
#include "gateway/log.h"
#include "gateway/mock_sink.h"
#include "gateway/pipeline.h"
#include "gateway/stats.h"
#include "gateway/version.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum {
    EXIT_FAILURE_STATUS = 1,
    EXIT_USAGE = 2,
    CAN_RECEIVE_COUNT_MAX = 1000000,
    CAN_RECEIVE_TIMEOUT_MS_DEFAULT = 5000,
    CAN_RECEIVE_TIMEOUT_MS_MAX = 60000,
    M5_RECEIVE_POLL_MS = 100,
    MOCK_SINK_DELAY_MS_MAX = 60000
};

static void print_usage(const char *program)
{
    printf("Usage: %s [--config PATH] [--set KEY=VALUE] [--print-config] "
           "[--wait-for-signal]\n"
           "       %s [--config PATH] [--set KEY=VALUE] "
           "--can-receive COUNT [--can-timeout-ms MS]\n"
           "       %s [--config PATH] [--set KEY=VALUE] "
           "--run-mock-sink [--mock-sink-delay-ms MS]\n"
           "       %s --help\n"
           "       %s --version\n",
           program, program, program, program, program);
}

static bool parse_unsigned_argument(const char *text,
                                    unsigned long long minimum,
                                    unsigned long long maximum,
                                    unsigned long long *value)
{
    unsigned long long parsed;
    char *end = NULL;

    if (text == NULL || text[0] == '\0' || text[0] == '-') {
        return false;
    }
    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || parsed < minimum ||
        parsed > maximum) {
        return false;
    }
    *value = parsed;
    return true;
}

static bool deadline_after_milliseconds(int timeout_ms,
                                        struct timespec *deadline)
{
    if (clock_gettime(CLOCK_MONOTONIC, deadline) != 0) {
        return false;
    }
    deadline->tv_sec += timeout_ms / 1000;
    deadline->tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (deadline->tv_nsec >= 1000000000L) {
        deadline->tv_sec++;
        deadline->tv_nsec -= 1000000000L;
    }
    return true;
}

static int milliseconds_until(const struct timespec *deadline)
{
    struct timespec now;
    int64_t seconds;
    int64_t nanoseconds;
    int64_t milliseconds;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return -1;
    }
    seconds = (int64_t)deadline->tv_sec - (int64_t)now.tv_sec;
    nanoseconds = (int64_t)deadline->tv_nsec - (int64_t)now.tv_nsec;
    if (nanoseconds < 0) {
        seconds--;
        nanoseconds += INT64_C(1000000000);
    }
    if (seconds < 0 || (seconds == 0 && nanoseconds == 0)) {
        return 0;
    }
    if (seconds > INT_MAX / 1000) {
        return INT_MAX;
    }
    milliseconds = seconds * INT64_C(1000) +
                   (nanoseconds + INT64_C(999999)) / INT64_C(1000000);
    return milliseconds > INT_MAX ? INT_MAX : (int)milliseconds;
}

static void count_can_rejection(gateway_stats *stats,
                                gateway_can_reject_reason reason)
{
    switch (reason) {
    case GATEWAY_CAN_REJECT_DATAGRAM_LENGTH:
        gateway_stats_increment(stats, GATEWAY_STAT_CAN_REJECTED_LENGTH);
        break;
    case GATEWAY_CAN_REJECT_DLC:
        gateway_stats_increment(stats, GATEWAY_STAT_CAN_REJECTED_DLC);
        break;
    case GATEWAY_CAN_REJECT_ID:
        gateway_stats_increment(stats, GATEWAY_STAT_CAN_REJECTED_ID);
        break;
    case GATEWAY_CAN_REJECT_CONTROL_TRUNCATED:
    case GATEWAY_CAN_REJECT_TIMESTAMP_MISSING:
    case GATEWAY_CAN_REJECT_TIMESTAMP_INVALID:
        gateway_stats_increment(stats, GATEWAY_STAT_CAN_TIMESTAMP_ERRORS);
        break;
    case GATEWAY_CAN_REJECT_NONE:
    default:
        gateway_stats_increment(stats, GATEWAY_STAT_CAN_RECEIVE_ERRORS);
        break;
    }
}

static void log_can_summary(gateway_logger *logger, gateway_stats *stats)
{
    gateway_stats_snapshot snapshot;

    gateway_stats_read(stats, &snapshot);
    gateway_log(
        logger, GATEWAY_LOG_INFO, "can",
        "M2_CAN_SUMMARY attempts=%" PRIu64 " accepted=%" PRIu64
        " timeouts=%" PRIu64 " rejected_length=%" PRIu64
        " rejected_dlc=%" PRIu64 " rejected_id=%" PRIu64
        " timestamp_errors=%" PRIu64 " receive_errors=%" PRIu64,
        snapshot.counters[GATEWAY_STAT_CAN_RECEIVE_ATTEMPTS],
        snapshot.counters[GATEWAY_STAT_CAN_RECEIVE_SUCCESS],
        snapshot.counters[GATEWAY_STAT_CAN_RECEIVE_TIMEOUTS],
        snapshot.counters[GATEWAY_STAT_CAN_REJECTED_LENGTH],
        snapshot.counters[GATEWAY_STAT_CAN_REJECTED_DLC],
        snapshot.counters[GATEWAY_STAT_CAN_REJECTED_ID],
        snapshot.counters[GATEWAY_STAT_CAN_TIMESTAMP_ERRORS],
        snapshot.counters[GATEWAY_STAT_CAN_RECEIVE_ERRORS]);
}

static int run_can_receive(const gateway_config *config,
                           gateway_logger *logger,
                           uint64_t receive_count,
                           int timeout_ms)
{
    gateway_can_receiver receiver;
    gateway_stats stats;
    struct timespec deadline;
    uint64_t accepted = 0;
    int result = EXIT_FAILURE_STATUS;

    gateway_can_receiver_init(&receiver);
    if (gateway_stats_init(&stats) != GATEWAY_OK) {
        gateway_log(logger, GATEWAY_LOG_ERROR, "can",
                    "stats initialization failed");
        return EXIT_FAILURE_STATUS;
    }
    if (!deadline_after_milliseconds(timeout_ms, &deadline)) {
        gateway_log(logger, GATEWAY_LOG_ERROR, "can",
                    "CLOCK_MONOTONIC query failed: %s", strerror(errno));
        goto cleanup;
    }
    if (gateway_can_receiver_open(&receiver, config->can_interface) !=
        GATEWAY_OK) {
        int saved_errno = errno;
        gateway_log(logger, GATEWAY_LOG_ERROR, "can",
                    "failed to open interface=%s: %s", config->can_interface,
                    strerror(saved_errno));
        goto cleanup;
    }
    gateway_log(logger, GATEWAY_LOG_INFO, "can",
                "M2 receive started interface=%s target_ids=0x100,0x101,0x102 "
                "classic_dlc=8 kernel_timestamp=SO_TIMESTAMPNS expected=%" PRIu64
                " timeout_ms=%d",
                config->can_interface, receive_count, timeout_ms);

    while (accepted < receive_count) {
        gateway_can_reject_reason reject_reason = GATEWAY_CAN_REJECT_NONE;
        telemetry_record record;
        gateway_error_code code;
        int remaining_ms = milliseconds_until(&deadline);

        if (remaining_ms < 0) {
            gateway_stats_increment(&stats,
                                    GATEWAY_STAT_CAN_RECEIVE_ERRORS);
            gateway_log(logger, GATEWAY_LOG_ERROR, "can",
                        "CLOCK_MONOTONIC query failed: %s", strerror(errno));
            break;
        }
        if (remaining_ms == 0) {
            gateway_stats_increment(&stats,
                                    GATEWAY_STAT_CAN_RECEIVE_TIMEOUTS);
            gateway_log(logger, GATEWAY_LOG_ERROR, "can",
                        "total receive deadline expired before expected "
                        "frame count");
            break;
        }
        gateway_stats_increment(&stats, GATEWAY_STAT_CAN_RECEIVE_ATTEMPTS);
        code = gateway_can_receiver_receive(&receiver, remaining_ms,
                                            accepted + 1, &record,
                                            &reject_reason);
        if (code == GATEWAY_OK) {
            gateway_stats_increment(&stats, GATEWAY_STAT_CAN_RECEIVE_SUCCESS);
            accepted++;
            gateway_log(
                logger, GATEWAY_LOG_INFO, "can",
                "M2_CAN_FRAME seq=%" PRIu64 " can_id=0x%03" PRIx32
                " dlc=%u kernel_timestamp_ns=%" PRId64
                " data=%02x%02x%02x%02x%02x%02x%02x%02x",
                record.gateway_seq, record.can_id, (unsigned int)record.dlc,
                record.kernel_timestamp_ns, (unsigned int)record.data[0],
                (unsigned int)record.data[1], (unsigned int)record.data[2],
                (unsigned int)record.data[3], (unsigned int)record.data[4],
                (unsigned int)record.data[5], (unsigned int)record.data[6],
                (unsigned int)record.data[7]);
            continue;
        }
        if (code == GATEWAY_ERROR_TIMEOUT) {
            gateway_stats_increment(&stats, GATEWAY_STAT_CAN_RECEIVE_TIMEOUTS);
            gateway_log(logger, GATEWAY_LOG_ERROR, "can",
                        "receive timed out before expected frame count");
            break;
        }
        if (code == GATEWAY_ERROR_INVALID_VALUE) {
            count_can_rejection(&stats, reject_reason);
            gateway_log(logger, GATEWAY_LOG_WARN, "can",
                        "rejected frame reason=%s",
                        gateway_can_reject_reason_name(reject_reason));
            continue;
        }
        gateway_stats_increment(&stats, GATEWAY_STAT_CAN_RECEIVE_ERRORS);
        gateway_log(logger, GATEWAY_LOG_ERROR, "can",
                    "receive failed: %s", gateway_error_string(code));
        break;
    }
    if (accepted == receive_count) {
        result = 0;
    }

cleanup:
    log_can_summary(logger, &stats);
    gateway_can_receiver_close(&receiver);
    gateway_stats_destroy(&stats);
    return result;
}

static gateway_error_code receive_pipeline_record(
    void *context,
    int timeout_ms,
    uint64_t gateway_seq,
    telemetry_record *record,
    gateway_can_reject_reason *reject_reason)
{
    return gateway_can_receiver_receive(context, timeout_ms, gateway_seq,
                                        record, reject_reason);
}

static void log_pipeline_summary(gateway_logger *logger,
                                 gateway_stats *stats,
                                 gateway_mock_sink *sink,
                                 gateway_pipeline *pipeline)
{
    gateway_stats_snapshot stats_snapshot;
    gateway_mock_sink_snapshot sink_snapshot;
    gateway_pipeline_snapshot pipeline_snapshot;

    gateway_stats_read(stats, &stats_snapshot);
    gateway_mock_sink_read(sink, &sink_snapshot);
    gateway_pipeline_read(pipeline, &pipeline_snapshot);
    gateway_log(
        logger, GATEWAY_LOG_INFO, "pipeline",
        "M5_PIPELINE_SUMMARY can_attempts=%" PRIu64
        " can_accepted=%" PRIu64 " can_timeouts=%" PRIu64
        " can_rejected_length=%" PRIu64 " can_rejected_dlc=%" PRIu64
        " can_rejected_id=%" PRIu64 " timestamp_errors=%" PRIu64
        " receive_errors=%" PRIu64 " decode_success=%" PRIu64
        " decode_errors=%" PRIu64 " queue_attempts=%" PRIu64
        " queue_success=%" PRIu64 " queue_drop=%" PRIu64
        " queue_closed=%" PRIu64 " queue_pop=%" PRIu64
        " queue_high_watermark=%zu queue_capacity=%zu sink_consumed=%" PRIu64
        " sink_gap_records=%" PRIu64 " sink_non_monotonic=%" PRIu64
        " sink_invalid=%" PRIu64,
        stats_snapshot.counters[GATEWAY_STAT_CAN_RECEIVE_ATTEMPTS],
        stats_snapshot.counters[GATEWAY_STAT_CAN_RECEIVE_SUCCESS],
        stats_snapshot.counters[GATEWAY_STAT_CAN_RECEIVE_TIMEOUTS],
        stats_snapshot.counters[GATEWAY_STAT_CAN_REJECTED_LENGTH],
        stats_snapshot.counters[GATEWAY_STAT_CAN_REJECTED_DLC],
        stats_snapshot.counters[GATEWAY_STAT_CAN_REJECTED_ID],
        stats_snapshot.counters[GATEWAY_STAT_CAN_TIMESTAMP_ERRORS],
        stats_snapshot.counters[GATEWAY_STAT_CAN_RECEIVE_ERRORS],
        stats_snapshot.counters[GATEWAY_STAT_CAN_DECODE_SUCCESS],
        stats_snapshot.counters[GATEWAY_STAT_CAN_DECODE_ERRORS],
        stats_snapshot.counters[GATEWAY_STAT_QUEUE_PUSH_ATTEMPTS],
        stats_snapshot.counters[GATEWAY_STAT_QUEUE_PUSH_SUCCESS],
        stats_snapshot.counters[GATEWAY_STAT_QUEUE_PUSH_TIMEOUTS],
        stats_snapshot.counters[GATEWAY_STAT_QUEUE_PUSH_CLOSED],
        stats_snapshot.counters[GATEWAY_STAT_QUEUE_POP_SUCCESS],
        stats_snapshot.queue_high_watermark,
        pipeline_snapshot.queue_capacity, sink_snapshot.consumed,
        sink_snapshot.sequence_gap_records,
        sink_snapshot.non_monotonic_records, sink_snapshot.invalid_records);
}

static int run_mock_sink_pipeline(const gateway_config *config,
                                  gateway_logger *logger,
                                  uint32_t mock_sink_delay_ms)
{
    gateway_can_receiver receiver;
    gateway_stats stats;
    gateway_lifecycle lifecycle;
    gateway_mock_sink sink;
    gateway_pipeline *pipeline = NULL;
    gateway_pipeline_config pipeline_config;
    bool stats_initialized = false;
    bool lifecycle_initialized = false;
    bool sink_initialized = false;
    bool pipeline_started = false;
    bool pipeline_joined = false;
    bool control_error = false;
    int result = EXIT_FAILURE_STATUS;

    gateway_can_receiver_init(&receiver);
    if (gateway_stats_init(&stats) != GATEWAY_OK) {
        gateway_log(logger, GATEWAY_LOG_ERROR, "pipeline",
                    "stats initialization failed");
        goto cleanup;
    }
    stats_initialized = true;
    if (gateway_lifecycle_init(&lifecycle) != GATEWAY_OK) {
        gateway_log(logger, GATEWAY_LOG_ERROR, "pipeline",
                    "lifecycle initialization failed");
        goto cleanup;
    }
    lifecycle_initialized = true;
    if (gateway_lifecycle_install_signal_handlers(&lifecycle) != GATEWAY_OK) {
        gateway_log(logger, GATEWAY_LOG_ERROR, "pipeline",
                    "signal handler installation failed");
        goto cleanup;
    }
    if (gateway_can_receiver_open(&receiver, config->can_interface) !=
        GATEWAY_OK) {
        int saved_errno = errno;
        gateway_log(logger, GATEWAY_LOG_ERROR, "pipeline",
                    "failed to open interface=%s: %s",
                    config->can_interface, strerror(saved_errno));
        goto cleanup;
    }
    if (gateway_mock_sink_init(&sink, mock_sink_delay_ms) != GATEWAY_OK) {
        gateway_log(logger, GATEWAY_LOG_ERROR, "pipeline",
                    "mock sink initialization failed");
        goto cleanup;
    }
    sink_initialized = true;

    (void)memset(&pipeline_config, 0, sizeof(pipeline_config));
    pipeline_config.queue_capacity = config->queue_capacity;
    pipeline_config.queue_push_timeout_ms = config->queue_push_timeout_ms;
    pipeline_config.receive_timeout_ms = M5_RECEIVE_POLL_MS;
    pipeline_config.receive = receive_pipeline_record;
    pipeline_config.receive_context = &receiver;
    pipeline_config.consume = gateway_mock_sink_consume;
    pipeline_config.consume_context = &sink;
    pipeline_config.lifecycle = &lifecycle;
    pipeline_config.stats = &stats;
    if (gateway_pipeline_create(&pipeline, &pipeline_config) != GATEWAY_OK ||
        gateway_pipeline_start(pipeline) != GATEWAY_OK) {
        gateway_log(logger, GATEWAY_LOG_ERROR, "pipeline",
                    "producer-consumer startup failed");
        goto cleanup;
    }
    pipeline_started = true;
    gateway_log(logger, GATEWAY_LOG_INFO, "pipeline",
                "M5 mock sink started interface=%s queue_capacity=%zu "
                "queue_push_timeout_ms=%u sink_delay_ms=%u",
                config->can_interface, config->queue_capacity,
                config->queue_push_timeout_ms, mock_sink_delay_ms);

    for (;;) {
        gateway_pipeline_snapshot snapshot;
        int signal_number = 0;
        gateway_error_code code = gateway_lifecycle_wait_signal(
            &lifecycle, M5_RECEIVE_POLL_MS, &signal_number);

        if (code == GATEWAY_OK) {
            gateway_log(logger, GATEWAY_LOG_INFO, "pipeline",
                        "stop requested by signal %d", signal_number);
            (void)gateway_pipeline_request_stop(pipeline, signal_number);
            break;
        }
        if (code != GATEWAY_ERROR_TIMEOUT) {
            gateway_log(logger, GATEWAY_LOG_ERROR, "pipeline",
                        "signal wait failed: %s", gateway_error_string(code));
            (void)gateway_pipeline_request_stop(pipeline, 0);
            control_error = true;
            break;
        }
        gateway_pipeline_read(pipeline, &snapshot);
        if (snapshot.producer_done || snapshot.consumer_done) {
            (void)gateway_pipeline_request_stop(pipeline, 0);
            control_error = true;
            break;
        }
        if (gateway_lifecycle_is_stop_requested(&lifecycle, NULL)) {
            (void)gateway_pipeline_request_stop(pipeline, 0);
            control_error = true;
            break;
        }
    }

    if (gateway_pipeline_join(pipeline) != GATEWAY_OK) {
        gateway_log(logger, GATEWAY_LOG_ERROR, "pipeline",
                    "worker join failed");
        goto cleanup;
    }
    pipeline_joined = true;
    log_pipeline_summary(logger, &stats, &sink, pipeline);
    {
        gateway_pipeline_snapshot pipeline_snapshot;
        gateway_mock_sink_snapshot sink_snapshot;

        gateway_pipeline_read(pipeline, &pipeline_snapshot);
        gateway_mock_sink_read(&sink, &sink_snapshot);
        if (!control_error &&
            pipeline_snapshot.producer_error == GATEWAY_OK &&
            pipeline_snapshot.consumer_error == GATEWAY_OK &&
            sink_snapshot.non_monotonic_records == 0 &&
            sink_snapshot.invalid_records == 0) {
            result = 0;
        }
    }

cleanup:
    if (pipeline_started && !pipeline_joined) {
        (void)gateway_pipeline_request_stop(pipeline, 0);
        (void)gateway_pipeline_join(pipeline);
    }
    gateway_pipeline_destroy(pipeline);
    if (sink_initialized) {
        gateway_mock_sink_destroy(&sink);
    }
    gateway_can_receiver_close(&receiver);
    if (lifecycle_initialized) {
        gateway_lifecycle_destroy(&lifecycle);
    }
    if (stats_initialized) {
        gateway_stats_destroy(&stats);
    }
    return result;
}

static int report_config_error(const char *context,
                               const gateway_config_error *error)
{
    fprintf(stderr, "gatewayd: %s: %s", context,
            gateway_error_string(error->code));
    if (error->line != 0) {
        fprintf(stderr, " at line %zu", error->line);
    }
    if (error->key[0] != '\0') {
        fprintf(stderr, " for key '%s'", error->key);
    }
    if (error->message[0] != '\0') {
        fprintf(stderr, ": %s", error->message);
    }
    fputc('\n', stderr);
    return EXIT_FAILURE_STATUS;
}

int main(int argc, char **argv)
{
    gateway_config config;
    gateway_config_error config_error;
    gateway_logger logger;
    gateway_lifecycle lifecycle;
    const char *config_path = NULL;
    bool print_config = false;
    bool wait_for_signal = false;
    bool can_receive_enabled = false;
    bool can_timeout_set = false;
    bool run_mock_sink = false;
    bool mock_sink_delay_set = false;
    uint64_t can_receive_count = 0;
    int can_timeout_ms = CAN_RECEIVE_TIMEOUT_MS_DEFAULT;
    uint32_t mock_sink_delay_ms = 0;
    int index;

    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("gatewayd %s\n", GATEWAYD_VERSION);
        return 0;
    }

    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--config") == 0) {
            if (++index >= argc || config_path != NULL) {
                print_usage(argv[0]);
                return EXIT_USAGE;
            }
            config_path = argv[index];
        } else if (strcmp(argv[index], "--set") == 0) {
            if (++index >= argc) {
                print_usage(argv[0]);
                return EXIT_USAGE;
            }
        } else if (strcmp(argv[index], "--print-config") == 0) {
            print_config = true;
        } else if (strcmp(argv[index], "--wait-for-signal") == 0) {
            wait_for_signal = true;
        } else if (strcmp(argv[index], "--can-receive") == 0) {
            unsigned long long parsed;
            if (++index >= argc || can_receive_enabled ||
                !parse_unsigned_argument(argv[index], 1,
                                         CAN_RECEIVE_COUNT_MAX, &parsed)) {
                print_usage(argv[0]);
                return EXIT_USAGE;
            }
            can_receive_enabled = true;
            can_receive_count = (uint64_t)parsed;
        } else if (strcmp(argv[index], "--can-timeout-ms") == 0) {
            unsigned long long parsed;
            if (++index >= argc || can_timeout_set ||
                !parse_unsigned_argument(argv[index], 1,
                                         CAN_RECEIVE_TIMEOUT_MS_MAX,
                                         &parsed)) {
                print_usage(argv[0]);
                return EXIT_USAGE;
            }
            can_timeout_set = true;
            can_timeout_ms = (int)parsed;
        } else if (strcmp(argv[index], "--run-mock-sink") == 0) {
            if (run_mock_sink) {
                print_usage(argv[0]);
                return EXIT_USAGE;
            }
            run_mock_sink = true;
        } else if (strcmp(argv[index], "--mock-sink-delay-ms") == 0) {
            unsigned long long parsed;
            if (++index >= argc || mock_sink_delay_set ||
                !parse_unsigned_argument(argv[index], 0,
                                         MOCK_SINK_DELAY_MS_MAX, &parsed)) {
                print_usage(argv[0]);
                return EXIT_USAGE;
            }
            mock_sink_delay_set = true;
            mock_sink_delay_ms = (uint32_t)parsed;
        } else {
            print_usage(argv[0]);
            return EXIT_USAGE;
        }
    }
    if ((wait_for_signal && (can_receive_enabled || run_mock_sink)) ||
        (can_receive_enabled && run_mock_sink) ||
        (can_timeout_set && !can_receive_enabled) ||
        (mock_sink_delay_set && !run_mock_sink)) {
        print_usage(argv[0]);
        return EXIT_USAGE;
    }

    gateway_config_init_defaults(&config);
    if (config_path != NULL &&
        gateway_config_load_file(&config, config_path, &config_error) !=
            GATEWAY_OK) {
        return report_config_error(config_path, &config_error);
    }
    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--config") == 0) {
            index++;
        } else if (strcmp(argv[index], "--set") == 0) {
            index++;
            if (gateway_config_apply_assignment(&config, argv[index],
                                                &config_error) != GATEWAY_OK) {
                return report_config_error("command-line override",
                                           &config_error);
            }
        } else if (strcmp(argv[index], "--can-receive") == 0 ||
                   strcmp(argv[index], "--can-timeout-ms") == 0 ||
                   strcmp(argv[index], "--mock-sink-delay-ms") == 0) {
            index++;
        }
    }
    if (gateway_config_validate(&config, &config_error) != GATEWAY_OK) {
        return report_config_error("merged configuration", &config_error);
    }
    if (gateway_logger_init(&logger, stderr, config.log_level) != GATEWAY_OK) {
        fputs("gatewayd: logger initialization failed\n", stderr);
        return EXIT_FAILURE_STATUS;
    }

    gateway_log(&logger, GATEWAY_LOG_INFO, "main", "gatewayd %s starting",
                GATEWAYD_VERSION);
    if (print_config) {
        gateway_config_log_redacted(&config, &logger);
    }
    if (can_receive_enabled) {
        int status = run_can_receive(&config, &logger, can_receive_count,
                                     can_timeout_ms);
        gateway_logger_destroy(&logger);
        return status;
    }
    if (run_mock_sink) {
        int status = run_mock_sink_pipeline(&config, &logger,
                                            mock_sink_delay_ms);
        gateway_logger_destroy(&logger);
        return status;
    }
    if (!wait_for_signal) {
        gateway_log(&logger, GATEWAY_LOG_INFO, "main",
                    "configuration check complete; SocketCAN receive is not "
                    "started without --can-receive");
        gateway_logger_destroy(&logger);
        return 0;
    }

    if (gateway_lifecycle_init(&lifecycle) != GATEWAY_OK) {
        gateway_log(&logger, GATEWAY_LOG_ERROR, "lifecycle",
                    "initialization failed");
        gateway_logger_destroy(&logger);
        return EXIT_FAILURE_STATUS;
    }
    if (gateway_lifecycle_install_signal_handlers(&lifecycle) != GATEWAY_OK) {
        gateway_log(&logger, GATEWAY_LOG_ERROR, "lifecycle",
                    "signal handler installation failed");
        gateway_lifecycle_destroy(&lifecycle);
        gateway_logger_destroy(&logger);
        return EXIT_FAILURE_STATUS;
    }
    gateway_log(&logger, GATEWAY_LOG_INFO, "lifecycle",
                "waiting for SIGINT or SIGTERM");
    {
        int signal_number = 0;
        gateway_error_code code = gateway_lifecycle_wait_signal(
            &lifecycle, -1, &signal_number);
        if (code != GATEWAY_OK) {
            gateway_log(&logger, GATEWAY_LOG_ERROR, "lifecycle",
                        "signal wait failed: %s", gateway_error_string(code));
            gateway_lifecycle_destroy(&lifecycle);
            gateway_logger_destroy(&logger);
            return EXIT_FAILURE_STATUS;
        }
        gateway_log(&logger, GATEWAY_LOG_INFO, "lifecycle",
                    "stop requested by signal %d", signal_number);
    }
    gateway_lifecycle_destroy(&lifecycle);
    gateway_logger_destroy(&logger);
    return 0;
}
