#include "gateway/version.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

enum {
    EXIT_USAGE = 2,
    READ_BUFFER_SIZE = 512
};

static void print_usage(const char *program)
{
    printf("Usage: %s [--help] [--version] [--config PATH]\n", program);
}

static int check_config_readable(const char *path, size_t *bytes_read)
{
    unsigned char buffer[READ_BUFFER_SIZE];
    FILE *stream;
    size_t total = 0;
    size_t count;

    stream = fopen(path, "rb");
    if (stream == NULL) {
        fprintf(stderr, "gatewayd: cannot open config '%s': %s\n",
                path, strerror(errno));
        return -1;
    }

    while ((count = fread(buffer, 1, sizeof(buffer), stream)) > 0) {
        total += count;
    }

    if (ferror(stream)) {
        fprintf(stderr, "gatewayd: cannot read config '%s': %s\n",
                path, strerror(errno));
        (void)fclose(stream);
        return -1;
    }

    if (fclose(stream) != 0) {
        fprintf(stderr, "gatewayd: cannot close config '%s': %s\n",
                path, strerror(errno));
        return -1;
    }

    *bytes_read = total;
    return 0;
}

int main(int argc, char **argv)
{
    const char *config_path = NULL;
    size_t config_bytes = 0;

    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("gatewayd %s\n", GATEWAYD_VERSION);
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "--config") == 0) {
        config_path = argv[2];
    } else if (argc != 1) {
        print_usage(argv[0]);
        return EXIT_USAGE;
    }

    printf("gatewayd %s starting\n", GATEWAYD_VERSION);

    if (config_path == NULL) {
        puts("configuration: built-in M0 placeholder defaults (non-operational)");
    } else {
        if (check_config_readable(config_path, &config_bytes) != 0) {
            return 1;
        }
        printf("configuration: loaded %zu bytes from %s\n",
               config_bytes, config_path);
        puts("configuration parsing: deferred to M1");
    }

    puts("gatewayd M0 skeleton exiting normally");
    return 0;
}

