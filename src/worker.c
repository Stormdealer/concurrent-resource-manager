#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "worker.h"
#include "resource_mgr.h"
#include "stats.h"
#include "scheduler.h"

void *worker_run(void *arg) {
    WorkerArgs *w = (WorkerArgs *)arg;

    for (;;) {
        Task *t = task_queue_pop(w->dispatch);
        if (!t) break; /* dispatch queue was shut down: no more work, ever */

        if (!t->registered) {
            resource_mgr_register(t); /* enters Banker's bookkeeping only once */
            t->registered = 1;
        }

        if (!t->started) {
            /* Phase 1: request the initial partial claim (need1). */
            if (!resource_mgr_try_request(t, t->need1)) {
                /* Not safe/available right now. Put the task back on its
                 * level queue and move on to other work instead of
                 * blocking this worker — a blocked worker can't run the
                 * already-granted tasks whose completion would free up
                 * the resources this task is waiting for. */
                /* Requeue at the LOWEST level, not the task's current one.
                 * Otherwise a denied (resource-less) task sits at level 0
                 * and, under strict priority, starves the demoted tasks
                 * that actually hold resources and could free them. */
                t->queue_level = MLFQ_LEVELS - 1;
                task_queue_push(w->levels[t->queue_level], t);
                usleep(2000); /* brief backoff: avoid busy-spinning on repeated denials */
                continue;
            }
            t->started = 1;
            clock_gettime(CLOCK_MONOTONIC, &t->start_time);
        } else if (!t->phase2_done && t->burst_ms * 2 <= t->total_burst_ms) {
            /* Phase 2: halfway through the burst, top up to the full
             * max_need. The task is genuinely holding need1 while it
             * waits here — real hold-and-wait, so a denial here is
             * Banker's algorithm actually earning its keep. */
            int topup[NUM_RESOURCES];
            for (int j = 0; j < NUM_RESOURCES; j++) topup[j] = t->max_need[j] - t->allocated[j];

            if (!resource_mgr_try_request(t, topup)) {
                task_queue_push(w->levels[t->queue_level], t);
                usleep(2000);
                continue;
            }
            t->phase2_done = 1;
        }

        int quantum = LEVEL_QUANTUM_MS[t->queue_level];
        int run_ms = (t->burst_ms < quantum) ? t->burst_ms : quantum;
        usleep(run_ms * 1000); /* simulated CPU burst */
        t->burst_ms -= run_ms;

        if (t->burst_ms <= 0) {
            clock_gettime(CLOCK_MONOTONIC, &t->completion_time);
            resource_mgr_release(t);
            stats_record_completion(w->stats, t);
            resource_mgr_unregister(t);
            printf("[worker %d] task %2d completed\n", w->worker_id, t->id);
            free(t);
        } else {
            /* used the full quantum without finishing: demote a level */
            if (t->queue_level < MLFQ_LEVELS - 1) t->queue_level++;
            task_queue_push(w->levels[t->queue_level], t);
        }
    }
    return NULL;
}
