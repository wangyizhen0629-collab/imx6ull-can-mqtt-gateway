#include "gateway/error.h"

const char *gateway_error_string(gateway_error_code code)
{
    switch (code) {
    case GATEWAY_OK:
        return "ok";
    case GATEWAY_ERROR_ARGUMENT:
        return "invalid argument";
    case GATEWAY_ERROR_IO:
        return "I/O error";
    case GATEWAY_ERROR_PARSE:
        return "parse error";
    case GATEWAY_ERROR_UNKNOWN_KEY:
        return "unknown configuration key";
    case GATEWAY_ERROR_DUPLICATE_KEY:
        return "duplicate configuration key";
    case GATEWAY_ERROR_RANGE:
        return "value out of range";
    case GATEWAY_ERROR_INVALID_VALUE:
        return "invalid value";
    case GATEWAY_ERROR_TIMEOUT:
        return "timeout";
    case GATEWAY_ERROR_CLOSED:
        return "closed";
    case GATEWAY_ERROR_SYSTEM:
        return "system error";
    case GATEWAY_ERROR_CAPACITY:
        return "capacity limit reached";
    default:
        return "unknown error";
    }
}
