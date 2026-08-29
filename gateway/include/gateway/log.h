#ifndef GATEWAY_LOG_H
#define GATEWAY_LOG_H

#include "gateway/error.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>

typedef enum {
    GATEWAY_LOG_DEBUG = 0,
    GATEWAY_LOG_INFO = 1,
    GATEWAY_LOG_WARN = 2,
    GATEWAY_LOG_ERROR = 3
} gateway_log_level;

typedef struct {
    pthread_mutex_t mutex;
    FILE *stream;
    gateway_log_level minimum_level;
} gateway_logger;

gateway_error_code gateway_logger_init(gateway_logger *logger,
                                       FILE *stream,
                                       gateway_log_level minimum_level);
void gateway_logger_destroy(gateway_logger *logger);
gateway_error_code gateway_log_level_parse(const char *text,
                                           gateway_log_level *level);
const char *gateway_log_level_name(gateway_log_level level);
void gateway_log(gateway_logger *logger,
                 gateway_log_level level,
                 const char *component,
                 const char *format,
                 ...);

#endif
