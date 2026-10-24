#ifndef MLFQ_BOOST_H
#define MLFQ_BOOST_H

#include "task_queue.h"

/*
 * Periodic MLFQ priority boost helper.
 *
 * Promotes every task currently WAITING in levels 1 and 2 to level 0.
 * It does not alter tasks currently executing in workers or tasks already
 * in the dispatch queue. The destination queue's existing tasks remain
 * ahead of promoted tasks; promoted tasks retain FIFO order within each
 * source queue (level 1 tasks are appended before level 2 tasks).
 *
 * IMPORTANT: call this from the scheduler thread, not concurrently from
 * a worker. Queue operations remain protected by each TaskQueue mutex.
 * A boost is not an immediate preemption mechanism for running workers.
 *
 * Returns the number of tasks promoted, or -1 if arguments are invalid.
 */
int mlfq_priority_boost(TaskQueue *level0,
                        TaskQueue *level1,
                        TaskQueue *level2);

#endif /* MLFQ_BOOST_H */
