#ifndef RESOURCE_MGR_H
#define RESOURCE_MGR_H

#include "../include/common.h"

void resource_mgr_init(void);

/* Registers a task's maximum claim so the safety algorithm knows about it.
 * Must be called once, before the task ever requests resources. */
void resource_mgr_register(Task *t);

/* Called when a task finishes (or exits): removes it from Banker's
 * bookkeeping so it no longer counts toward Need[][] during safety checks. */
void resource_mgr_unregister(Task *t);

/* Non-blocking: attempts to grant an INCREMENTAL request of `req[j]`
 * units of each resource j, on top of whatever t already holds.
 * Tentatively allocates, runs the Banker's safety check against
 * every registered task's remaining Need (max_need - allocated), and
 * either commits (returns 1) or rolls back (returns 0) without
 * waiting. The caller (a worker thread) stays free to service other
 * tasks on denial, which avoids a blocked worker holding up the very
 * tasks whose completion would free the resources it wants. */
int resource_mgr_try_request(Task *t, const int *req);

/* Releases everything currently allocated to t and wakes any tasks
 * that are waiting for resources to become available. */
void resource_mgr_release(Task *t);

/* Diagnostics used by stats.c / monitor_ipc.c */
int  resource_mgr_denied_count(void);

#endif /* RESOURCE_MGR_H */
