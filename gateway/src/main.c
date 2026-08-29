#include "gateway/config.h"
#include "gateway/lifecycle.h"
#include "gateway/log.h"
#include "gateway/version.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum {
    EXIT_FAILURE_STATUS = 1,
    EXIT_USAGE = 2
};

static void print_usage(const char *program)
{
    printf("Usage: %s [--config PATH] [--set KEY=VALUE] [--print-config] "
           "[--wait-for-signal]\n"
           "       %s --help\n"
           "       %s --version\n",
           program, program, program);
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
        } else {
            print_usage(argv[0]);
            return EXIT_USAGE;
        }
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
    if (!wait_for_signal) {
        gateway_log(&logger, GATEWAY_LOG_INFO, "main",
                    "M1 host infrastructure check complete; operational data "
                    "path is not started");
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
