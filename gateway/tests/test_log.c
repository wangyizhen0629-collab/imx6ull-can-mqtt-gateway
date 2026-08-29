#include "gateway/log.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,         \
                    __LINE__, #condition);                                     \
            return 1;                                                          \
        }                                                                      \
    } while (0)

int main(void)
{
    gateway_log_level level;
    gateway_logger logger;
    char output[1024];
    size_t bytes;
    FILE *stream = tmpfile();

    CHECK(stream != NULL);
    CHECK(gateway_log_level_parse("debug", &level) == GATEWAY_OK);
    CHECK(level == GATEWAY_LOG_DEBUG);
    CHECK(gateway_log_level_parse("error", &level) == GATEWAY_OK);
    CHECK(level == GATEWAY_LOG_ERROR);
    CHECK(gateway_log_level_parse("verbose", &level) ==
          GATEWAY_ERROR_INVALID_VALUE);
    CHECK(gateway_logger_init(&logger, stream, GATEWAY_LOG_INFO) == GATEWAY_OK);
    gateway_log(&logger, GATEWAY_LOG_DEBUG, "unit", "hidden");
    gateway_log(&logger, GATEWAY_LOG_INFO, "unit", "visible %d", 42);
    CHECK(fflush(stream) == 0);
    rewind(stream);
    bytes = fread(output, 1, sizeof(output) - 1, stream);
    output[bytes] = '\0';
    CHECK(strstr(output, "hidden") == NULL);
    CHECK(strstr(output, " INFO unit: visible 42\n") != NULL);
    CHECK(output[0] == '[');
    CHECK(strstr(output, "Z]") != NULL);
    gateway_logger_destroy(&logger);
    CHECK(fclose(stream) == 0);
    return 0;
}
