# OS-IPC-Boating

An IPC/synchronization simulation of the classic **Boating (Boats and Visitors)** problem, implemented in C with POSIX threads (pthreads). Built for the LA7 assignment (see `LA7.pdf`).

## Problem

A lake has `m` boats and `n` visitors. Each visitor:
1. Sightsees for a random amount of time.
2. Waits for a boat to become available.
3. Takes a ride of a random duration.
4. Leaves.

Each boat repeatedly waits for a visitor, gives them a ride, then becomes available again — until all visitors have left. The synchronization must ensure a boat and a visitor are correctly paired up (a boat doesn't start a ride without a visitor, and a visitor doesn't board a boat that isn't actually free) without race conditions or busy-waiting deadlocks.

## Synchronization design

- **Custom semaphore** (`semaphore` struct): built from a `pthread_mutex_t` + `pthread_cond_t`, with `wait`/`signal` implementing classic P/V operations.
- **`boat` semaphore**: signaled by a visitor when ready to ride, waited on by boats.
- **`rider` semaphore**: signaled by a boat when available, waited on by visitors.
- **Per-boat barrier (`BB[i]`)**: a 2-party `pthread_barrier_t` used to rendezvous a specific boat with the specific visitor that claimed it.
- **`bmtx` mutex**: protects the shared boat-state arrays (`BA` availability, `BC` assigned visitor, `BT` ride time).
- **`EOS` barrier**: signals end-of-simulation once all visitors have left.

## Build

```bash
gcc -o boating boating.c -lpthread
```

## Run

```bash
./boating <number of boats> <number of visitors>
```

Constraints: `5 <= m <= 10` boats, `20 <= n <= 100` visitors.

Example:

```bash
./boating 5 20
```

## Files

- `boating.c` — simulation source code
- `LA7.pdf` — assignment specification
