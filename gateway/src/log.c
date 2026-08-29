#include "gateway/log.h"

#include <stdbool.h>
#include <string.h>
#include <time.h>

gateway_error_code gateway_logger_init(gateway_logger *logger,
                                       FILE *stream,
                                       gateway_log_level minimum_level)
{
    if (logger == NULL || stream == NULL || minimum_level < GATEWAY_LOG_DEBUG ||
        minimum_level > GATEWAY_LOG_ERROR) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    if (pthread_mutex_init(&logger->mutex, NULL) != 0) {
        return GATEWAY_ERROR_SYSTEM;
    }
    logger->stream = stream;
    logger->minimum_level = minimum_level;
    return GATEWAY_OK;
}

void gateway_logger_destroy(gateway_logger *logger)
{
    if (logger != NULL) {
        (void)pthread_mutex_destroy(&logger->mutex);
    }
}

gateway_error_code gateway_log_level_parse(const char *text,
                                           gateway_log_level *level)
{
    if (text == NULL || level == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    if (strcmp(text, "debug") == 0) {
        *level = GATEWAY_LOG_DEBUG;
    } else if (strcmp(text, "info") == 0) {
        *level = GATEWAY_LOG_INFO;
    } else if (strcmp(text, "warn") == 0) {
        *level = GATEWAY_LOG_WARN;
    } else if (strcmp(text, "error") == 0) {
        *level = GATEWAY_LOG_ERROR;
    } else {
        return GATEWAY_ERROR_INVALID_VALUE;
    }
    return GATEWAY_OK;
}

const char *gateway_log_level_name(gateway_log_level level)
{
    switch (level) {
    case GATEWAY_LOG_DEBUG:
        return "DEBUG";
    case GATEWAY_LOG_INFO:
        return "INFO";
    case GATEWAY_LOG_WARN:
        return "WARN";
    case GATEWAY_LOG_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

void gateway_log(gateway_logger *logger,
                 gateway_log_level level,
                 const char *component,
                 const char *format,
                 ...)
{
    struct timespec now;
    struct tm utc;
    char timestamp[32];
    va_list arguments;

    if (logger == NULL || component == NULL || format == NULL ||
        level < logger->minimum_level) {
        return;
    }

    if (clock_gettime(CLOCK_REALTIME, &now) != 0 ||
        gmtime_r(&now.tv_sec, &utc) == NULL) {
        (void)snprintf(timestamp, sizeof(timestamp), "time-unavailable");
    } else {
        (void)snprintf(timestamp, sizeof(timestamp),
                       "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ",
                       utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                       utc.tm_hour, utc.tm_min, utc.tm_sec,
                       now.tv_nsec / 1000000L);
    }

    (void)pthread_mutex_lock(&logger->mutex);
    (void)fprintf(logger->stream, "[%s] %s %s: ", timestamp,
                  gateway_log_level_name(level), component);
    va_start(arguments, format);
    (void)vfprintf(logger->stream, format, arguments);
    va_end(arguments);
    (void)fputc('\n', logger->stream);
    (void)fflush(logger->stream);
    (void)pthread_mutex_unlock(&logger->mutex);
}
