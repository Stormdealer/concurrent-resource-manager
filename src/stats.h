#ifndef STATS_H
#define STATS_H

#include "../include/common.h"

/* Records that task t has just completed: computes waiting/turnaround
 * time and updates the shared-memory stats struct under its lock. */
void stats_record_completion(SharedStats *stats, Task *t);

#endif /* STATS_H */
