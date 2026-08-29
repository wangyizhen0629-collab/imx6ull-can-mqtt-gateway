#include "gateway/config.h"

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

static int write_temporary_config(const char *contents, char *path)
{
    int descriptor = mkstemp(path);
    int write_failed;
    int close_failed;
    FILE *stream;

    if (descriptor < 0) {
        return -1;
    }
    stream = fdopen(descriptor, "w");
    if (stream == NULL) {
        (void)close(descriptor);
        (void)unlink(path);
        return -1;
    }
    write_failed = fputs(contents, stream) == EOF;
    close_failed = fclose(stream) != 0;
    if (write_failed || close_failed) {
        (void)unlink(path);
        return -1;
    }
    return 0;
}

static int test_defaults_file_and_override(void)
{
    static const char contents[] =
        "# test configuration\n"
        "device_id = unit-gateway\n"
        "can_interface=vcan0\n"
        "broker_host=broker.example.invalid\n"
        "broker_port=2883\n"
        "broker_username=test-user\n"
        "broker_password=fake-test-secret\n"
        "mqtt_topic=test/unit/telemetry\n"
        "queue_capacity=4\n"
        "queue_push_timeout_ms=10\n"
        "batch_interval_ms=100\n"
        "spool_path=/tmp/gateway-test-spool.data\n"
        "log_level=debug\n";
    char path[] = "/tmp/gateway-config-valid-XXXXXX";
    char output[2048];
    size_t bytes;
    gateway_config config;
    gateway_config_error error;
    gateway_logger logger;
    FILE *log_stream;

    gateway_config_init_defaults(&config);
    CHECK(config.queue_capacity == 1024);
    CHECK(gateway_config_validate(&config, &error) == GATEWAY_OK);
    CHECK(write_temporary_config(contents, path) == 0);
    CHECK(gateway_config_load_file(&config, path, &error) == GATEWAY_OK);
    CHECK(unlink(path) == 0);
    CHECK(config.queue_capacity == 4);
    CHECK(config.broker_port == 2883);
    CHECK(config.log_level == GATEWAY_LOG_DEBUG);
    CHECK(gateway_config_apply_assignment(&config, "queue_capacity=8", &error) ==
          GATEWAY_OK);
    CHECK(config.queue_capacity == 8);

    log_stream = tmpfile();
    CHECK(log_stream != NULL);
    CHECK(gateway_logger_init(&logger, log_stream, GATEWAY_LOG_DEBUG) ==
          GATEWAY_OK);
    gateway_config_log_redacted(&config, &logger);
    CHECK(fflush(log_stream) == 0);
    rewind(log_stream);
    bytes = fread(output, 1, sizeof(output) - 1, log_stream);
    output[bytes] = '\0';
    CHECK(strstr(output, "fake-test-secret") == NULL);
    CHECK(strstr(output, "test-user") == NULL);
    CHECK(strstr(output, "broker_password=<redacted>") != NULL);
    CHECK(strstr(output, "broker_username=<redacted>") != NULL);
    gateway_logger_destroy(&logger);
    CHECK(fclose(log_stream) == 0);
    return 0;
}

static int expect_file_error(const char *contents, gateway_error_code expected,
                             size_t expected_line)
{
    char path[] = "/tmp/gateway-config-invalid-XXXXXX";
    gateway_config config;
    gateway_config_error error;
    gateway_error_code result;

    gateway_config_init_defaults(&config);
    CHECK(write_temporary_config(contents, path) == 0);
    result = gateway_config_load_file(&config, path, &error);
    CHECK(unlink(path) == 0);
    CHECK(result == expected);
    CHECK(error.code == expected);
    CHECK(error.line == expected_line);
    return 0;
}

static int test_invalid_inputs_and_boundaries(void)
{
    gateway_config config;
    gateway_config_error error;

    CHECK(expect_file_error("unknown=value\n", GATEWAY_ERROR_UNKNOWN_KEY, 1) ==
          0);
    CHECK(expect_file_error("queue_capacity=2\nqueue_capacity=3\n",
                            GATEWAY_ERROR_DUPLICATE_KEY, 2) == 0);
    CHECK(expect_file_error("queue_capacity\n", GATEWAY_ERROR_PARSE, 1) == 0);
    CHECK(expect_file_error("queue_capacity=0\n", GATEWAY_ERROR_RANGE, 1) == 0);
    CHECK(expect_file_error("mqtt_topic=test/+\n",
                            GATEWAY_ERROR_INVALID_VALUE, 1) == 0);
    CHECK(expect_file_error("spool_path=relative/path\n",
                            GATEWAY_ERROR_INVALID_VALUE, 1) == 0);

    gateway_config_init_defaults(&config);
    CHECK(gateway_config_apply_assignment(&config, "queue_capacity=65536",
                                          &error) == GATEWAY_OK);
    CHECK(gateway_config_apply_assignment(&config, "queue_capacity=65537",
                                          &error) == GATEWAY_ERROR_RANGE);
    CHECK(gateway_config_apply_assignment(&config,
                                          "queue_push_timeout_ms=60000",
                                          &error) == GATEWAY_OK);
    CHECK(gateway_config_apply_assignment(&config,
                                          "queue_push_timeout_ms=60001",
                                          &error) == GATEWAY_ERROR_RANGE);
    CHECK(gateway_config_apply_assignment(&config, "batch_interval_ms=100",
                                          &error) == GATEWAY_OK);
    CHECK(gateway_config_apply_assignment(&config, "batch_interval_ms=99",
                                          &error) == GATEWAY_ERROR_RANGE);
    CHECK(gateway_config_apply_assignment(&config, "broker_port=65535", &error) ==
          GATEWAY_OK);
    CHECK(gateway_config_apply_assignment(&config, "broker_port=65536", &error) ==
          GATEWAY_ERROR_RANGE);
    CHECK(gateway_config_apply_assignment(&config, "not-an-assignment", &error) ==
          GATEWAY_ERROR_PARSE);
    return 0;
}

int main(void)
{
    CHECK(test_defaults_file_and_override() == 0);
    CHECK(test_invalid_inputs_and_boundaries() == 0);
    return 0;
}
