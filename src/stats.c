#include "stats.h"
#include "resource_mgr.h"

static long ms_diff(struct timespec *a, struct timespec *b) {
    return (b->tv_sec - a->tv_sec) * 1000L + (b->tv_nsec - a->tv_nsec) / 1000000L;
}

void stats_record_completion(SharedStats *stats, Task *t) {
    long turnaround = ms_diff(&t->arrival_time, &t->completion_time);
    long waiting = turnaround - t->total_burst_ms;
    if (waiting < 0) waiting = 0;

    pthread_mutex_lock(&stats->lock);
    stats->completed_count++;
    stats->total_waiting_ms += waiting;
    stats->total_turnaround_ms += turnaround;
    stats->denied_unsafe_requests = resource_mgr_denied_count();
    pthread_mutex_unlock(&stats->lock);
}
