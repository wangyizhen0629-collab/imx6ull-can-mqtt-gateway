#include "gateway/stats.h"

#include <string.h>

gateway_error_code gateway_stats_init(gateway_stats *stats)
{
    if (stats == NULL) {
        return GATEWAY_ERROR_ARGUMENT;
    }
    if (pthread_mutex_init(&stats->mutex, NULL) != 0) {
        return GATEWAY_ERROR_SYSTEM;
    }
    (void)memset(&stats->values, 0, sizeof(stats->values));
    return GATEWAY_OK;
}

void gateway_stats_destroy(gateway_stats *stats)
{
    if (stats != NULL) {
        (void)pthread_mutex_destroy(&stats->mutex);
    }
}

void gateway_stats_increment(gateway_stats *stats, gateway_stat_counter counter)
{
    if (stats == NULL || counter < 0 || counter >= GATEWAY_STAT_COUNT) {
        return;
    }
    (void)pthread_mutex_lock(&stats->mutex);
    stats->values.counters[counter]++;
    (void)pthread_mutex_unlock(&stats->mutex);
}

void gateway_stats_update_queue_high_watermark(gateway_stats *stats,
                                               size_t count)
{
    if (stats == NULL) {
        return;
    }
    (void)pthread_mutex_lock(&stats->mutex);
    if (count > stats->values.queue_high_watermark) {
        stats->values.queue_high_watermark = count;
    }
    (void)pthread_mutex_unlock(&stats->mutex);
}

void gateway_stats_read(gateway_stats *stats, gateway_stats_snapshot *snapshot)
{
    if (stats == NULL || snapshot == NULL) {
        return;
    }
    (void)pthread_mutex_lock(&stats->mutex);
    *snapshot = stats->values;
    (void)pthread_mutex_unlock(&stats->mutex);
}
