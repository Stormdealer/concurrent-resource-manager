# Test Cases and Results

All results below were captured by actually running `./crm <N>` in the
project's Linux/Ubuntu build environment (gcc, `-Wall -Wextra`, zero
warnings). The random seed is fixed (`srand(42)`), so task
characteristics are reproducible; the exact contention pattern can
still vary slightly between runs because thread scheduling is
nondeterministic — that variability is itself worth noting in your
report as a real OS phenomenon.

## TC1 — Light load, sanity check

**Setup:** `./crm 8` (8 tasks, default resource pool: 4 CPU cores, 2 I/O
channels, 8 memory blocks)
**Expected:** All tasks complete; low/no Banker's-algorithm denials
since supply comfortably exceeds demand.
**Result:** 8/8 completed, 0 unsafe requests blocked. ✅

## TC2 — Moderate load

**Setup:** `./crm 12`
**Expected:** All tasks complete; contention starts to appear but
rarely forces a denial.
**Result:** 12/12 completed, 0 unsafe requests blocked. ✅

## TC3 — Heavier load, Banker's algorithm engaged

**Setup:** `./crm 18` and `./crm 25`
**Expected:** All tasks still complete, but as more tasks compete for
the same fixed pool, some resource top-up requests should be
correctly identified as unsafe and deferred.
**Result:**
- n=18: 18/18 completed, 1 unsafe request blocked
- n=25: 25/25 completed, 4 unsafe requests blocked
Both confirm the safety algorithm is being exercised, not just a
simple availability check. ✅

## TC4 — Repeatability / flakiness check

**Setup:** `./crm 20` run 5 times back-to-back.
**Expected:** All 5 runs complete all 20 tasks (no deadlock, no
lost/duplicated task), even though the exact denial count may differ
run to run due to thread-interleaving nondeterminism.
**Result:** 5/5 runs completed 20/20 tasks. Unsafe-denial counts
varied (3, 0, 1, 0, 0) — expected variance, no hangs. ✅

## TC5 — Stress test

**Setup:** `./crm 40`
**Expected:** System remains stable and completes under significantly
higher load than it was designed around (10 workers-worth of default
pool vs. 40 tasks).
**Result:** 40/40 completed, 5 unsafe requests blocked, no hang,
clean exit. ✅

## TC6 — Deadlock-avoidance under artificially tight resources

**Setup:** Temporarily reduce `TOTAL_MEM_BLOCKS` from 8 to 4 in
`include/common.h`, rebuild, run `./crm 15` three times.
**Expected:** More frequent unsafe-request denials as the memory pool
becomes the binding constraint; system still completes all tasks
rather than deadlocking.
**Result:** All 3 runs completed 15/15 tasks with no hang. (Use this
as your "before/after" comparison in the report — it's a clean way to
visually demonstrate the safety algorithm actually working harder
under pressure.)

## TC7 — Edge case: single task

**Setup:** `./crm 1`
**Expected:** Task is granted resources immediately (nothing else
competing), runs to completion, monitor and main process both exit
cleanly.
**How to verify:** run it and confirm `tasks completed: 1` with `0`
unsafe requests blocked (no contention possible).

## TC8 — Error handling: invalid argument

**Setup:** `./crm abc` (non-numeric task count)
**Expected:** `atoi("abc")` returns 0, so the generator loop
(`for i = 1 to 0`) simply never runs — the program should exit
cleanly with `tasks completed: 0` rather than crashing.
**Suggested improvement before Review II:** add explicit validation
in `main.c` (e.g., reject non-positive counts with a usage message)
so this is handled deliberately rather than incidentally — this is a
good thing to point out in your "error handling" demonstration, since
graders specifically look for it.

## Suggested additions before your final submission

- **Comparative scheduling test:** temporarily swap the MLFQ
  scheduler for a plain FCFS/Round-Robin pass over the same fixed-seed
  workload and compare average waiting/turnaround time — this
  directly satisfies the "performance evaluation" requirement for
  Review II.
- **Resource-starvation test:** generate a workload where one type of
  task (e.g., CPU-bound tasks needing 2 cores) dominates, and confirm
  the priority-boost/round-robin nature of MLFQ still lets smaller
  I/O-bound tasks make progress rather than starving.
