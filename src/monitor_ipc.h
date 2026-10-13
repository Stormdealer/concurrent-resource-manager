#ifndef MONITOR_IPC_H
#define MONITOR_IPC_H

#include <sys/types.h>
#include "../include/common.h"

/* Creates a POSIX shared-memory object and maps a SharedStats struct
 * into it, with a process-shared mutex ready to use. */
SharedStats *monitor_shared_create(const char *shm_name);

/* fork()s a child "monitor process" that polls the shared stats and
 * prints a live status line until expected_tasks have completed, then
 * exits. Uses a named semaphore so the child doesn't start reading
 * until the parent has finished initializing the shared struct. */
pid_t monitor_spawn(SharedStats *stats, const char *sem_name,
                     int poll_interval_ms, int expected_tasks);

/* Unmaps and unlinks the shared memory (and semaphore, if owner). */
void monitor_shared_destroy(SharedStats *stats, const char *shm_name,
                             const char *sem_name);

#endif /* MONITOR_IPC_H */
