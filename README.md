# QueueX - Ticket Counter Queue Management System

QueueX is a terminal-based Railway and Bus ticket counter management application written in standard C (C11). It combines an array-based **Circular Priority Queue** with **dynamic aging for starvation prevention**, multi-window service routing, and **ACID-like persistent CSV storage** with transaction safety and automated schema migration.

---

## 1. Project Overview

At transportation reservation hubs, thousands of passengers arrive daily requiring services across multiple counters. QueueX models a modern, reliable ticket counter system:
- **Two Transport Modes**: Dedicated, independent waiting lines for Railway and Bus services.
- **Four Service Windows**: Railway Window 1, Railway Window 2, Bus Window 1, and Bus Window 2.
- **Global Sequential Token Issuance**: Unified token generation (`T001`, `T002`, `T003`, ...) shared across both counters, preserved across application restarts.
- **Priority-Based Service with FIFO Tie-Breaking**: Emergency, Assistance, and Senior citizens are served before Normal passengers, while strictly preserving First-In, First-Out order among passengers with equal priority.
- **Starvation Prevention via Dynamic Aging**: Older tickets waiting in the queue automatically gain priority boosts after a configured waiting duration to prevent normal passengers from waiting indefinitely.
- **Middle-of-Queue Cancellation**: Allows passengers with `WAITING` status to cancel their tokens, seamlessly compacting the circular array without corrupting the queue.
- **Reliable CSV Storage**: Persistent record-keeping in `tickets.csv` using atomic file replacement, backup files, and robust RFC 4180-style CSV quoting and parsing.
- **Real-Time & Historical Analytics**: Daily sales reports, weekly/monthly metrics, Railway vs Bus comparisons, peak booking hours, and service window performance.

---

## 2. Core DSA Concepts

### A. Circular Queue (Array-Based)

QueueX implements circular queues using fixed-size static arrays (`MAX_SIZE = 100`).

In a standard linear queue, dequeuing elements increments `front`, leaving earlier array slots permanently unusable and leading to false queue overflows. A **Circular Queue** eliminates wasted memory by wrapping indices back to `0` using the modulo operator (`%`):

```text
next_rear  = (rear + 1) % MAX_SIZE
next_front = (front + 1) % MAX_SIZE
```

#### Queue Invariants
- **Capacity**: 100 passengers per counter.
- **Empty Condition**: `count == 0`
- **Full Condition**: `count == MAX_SIZE`
- **Initialization**: `front = 0`, `rear = -1`, `count = 0`

### B. Circular Queue Cancellation (Arbitrary Position Removal)

A major challenge in static array queues is removing an element from the middle (e.g. ticket cancellation) or serving out-of-order high-priority passengers.

QueueX uses a **logical-index circular shift**:
1. Scan the circular buffer from `front` for `count` steps to locate the target token at logical position `k` (`0 <= k < count`).
2. Shift every subsequent passenger from logical position `k + 1` up to `count - 1` forward by one slot:
   ```c
   for (int i = k; i < q->count - 1; i++) {
       int curr = (q->front + i) % MAX_SIZE;
       int next = (q->front + i + 1) % MAX_SIZE;
       q->data[curr] = q->data[next];
   }
   ```
3. Decrement `rear` with wrap-around:
   ```c
   q->rear = (q->rear - 1 + MAX_SIZE) % MAX_SIZE;
   q->count--;
   ```
4. If `count` drops to `0`, `initQueue(q)` resets pointers to `front = 0, rear = -1`.

**Why this works**:
- Front remains unchanged.
- Relative arrival order (FIFO) of all remaining passengers is strictly preserved.
- It operates safely whether the removed passenger is at `front`, `rear`, or wrapped around index `0`.
- Time Complexity: `O(n)`, Space Complexity: `O(1)`.

### C. Priority Queue & Aging (Starvation Prevention)

QueueX defines four priority tiers:

| Numeric Level | Category | Description |
|---|---|---|
| 1 (Highest) | Emergency | Medical emergencies or critical transit needs |
| 2 | Assistance Required | Persons with disabilities or requiring mobility aid |
| 3 | Senior Citizen | Elderly passengers |
| 4 (Lowest) | Normal | General public passengers |

#### Selection Algorithm
When `dequeuePriority()` or `peekPriority()` is called:
1. Iterate from `front` to `rear` across all `count` waiting passengers.
2. Calculate each passenger's **Effective Priority**:
   ```c
   effective_priority = original_priority - (wait_seconds / AGING_INTERVAL_SEC);
   if (effective_priority < PRIORITY_EMERGENCY) {
       effective_priority = PRIORITY_EMERGENCY;
   }
   ```
   *(Default `AGING_INTERVAL_SEC` is 120 seconds).*
3. Select the passenger with the smallest numerical effective priority value.
4. **FIFO Tie-Breaking**: When comparing candidates, an earlier passenger is only replaced if a later passenger has a strictly lower effective priority (`eff < bestEff`). If two passengers have equal effective priority, the one who arrived earlier is selected.
5. Extract the selected passenger using the circular shift removal algorithm.

#### Viva Explanation for Starvation Prevention
> *"In a pure priority queue, low-priority passengers can starve indefinitely if higher-priority passengers keep arriving. QueueX implements dynamic aging: every 120 seconds a passenger waits, their effective priority improves by 1 level without altering their original record in storage. A normal passenger waiting 6 minutes reaches Emergency effective priority, ensuring fair service while maintaining arrival FIFO order for equal priorities."*

---

## 3. Multiple Service Windows

QueueX models four dedicated operator windows:
- **Railway Window 1** (Serves Railway Queue)
- **Railway Window 2** (Serves Railway Queue)
- **Bus Window 1** (Serves Bus Queue)
- **Bus Window 2** (Serves Bus Queue)

### Workload Balancing
- The operator selects the window when serving a passenger.
- Railway windows cannot serve Bus passengers, and Bus windows cannot serve Railway passengers.
- Both windows for each transport mode draw from the shared, unified circular queue for that mode, ensuring global priority and FIFO fairness.
- Every served ticket logs the exact service window in `tickets.csv`.
- The system computes historical window workloads and percentage distribution from saved records.

---

## 4. Persistent CSV Storage & Reliability

All ticket records are stored in `tickets.csv` in the application root directory.

### CSV Schema (14 Fields)
```csv
Token,Counter,Name,Destination,BookingDate,BookingTime,Priority,Status,ServiceDate,ServiceTime,CancellationDate,CancellationTime,ServiceWindow,RegistrationSequence
T001,Railway,"Das, Rahul",Kolkata,21-11-2026,10:00:00,Senior,SERVED,21-11-2026,10:15:00,-,-,Railway Window 1,1
T002,Bus,Amit Roy,Durgapur,21-11-2026,10:05:00,Emergency,WAITING,-,-,-,-,-,2
```

### Reliability & Transaction Safety
1. **Safe CSV Parsing & Quoting**: Fields containing commas or double quotes are wrapped in quotation marks with internal quotes escaped as `""` (RFC 4180 standard).
2. **Safe Atomic Updates**:
   - Updates (Serving, Cancelling) stream records from `tickets.csv` into `tickets_temp.csv`.
   - File writes are verified with `fflush()` and error checks.
   - A backup `tickets_backup.csv` is created before replacing the original file.
   - The temp file is renamed over the original using `remove()` followed by `rename()`. If `rename()` fails, the backup is restored.
3. **Queue-Storage Consistency**:
   - During registration, in-memory enqueue happens first. If writing to CSV fails, the queue entry is rolled back.
   - During service or cancellation, if the file update fails, the in-memory queue state is rolled back.
4. **Automated Schema Migration**:
   - If an existing `tickets.csv` from an older version (7 or 8 columns) is detected on startup, QueueX automatically backs it up to `tickets_backup_legacy.csv`, migrates all records to the 14-column format with appropriate defaults, and preserves token continuity.

---

## 5. Project File Structure

```text
QueueX/
|-- queue.h       # Passenger and Queue struct definitions, priority constants, and prototypes
|-- queue.c       # Circular queue operations, priority selection, aging, and circular shift removal
|-- storage.h     # Storage interface, TicketRecord struct, and reporting prototypes
|-- storage.c     # CSV I/O, atomic transactions, quoting, searches, and statistical reports
|-- main.c        # User interface, input validation, service window routing, and menu loop
|-- README.md     # System documentation and DSA study guide
\-- QueueX.exe    # Compiled executable binary
```

---

## 6. Compilation and Execution

### Requirements
- GCC (MinGW-w64 on Windows, or GCC on Linux/macOS)
- Standard C11 support (`-std=c11`)

### Compilation Command

```powershell
gcc -Wall -Wextra -Wpedantic -std=c11 main.c queue.c storage.c -o QueueX.exe
```

### Execution

```powershell
.\QueueX.exe
```

---

## 7. Menu Interface

The full menu is printed **once** at startup. Subsequent operations return directly to the `Enter choice: ` prompt:

```text
QUEUEX - TICKET COUNTER
Railway and Bus Management System

1.  Register New Passenger
2.  Serve Next Passenger
3.  View Waiting Queue
4.  View Next Passenger
5.  Search Ticket / Passenger
6.  Cancel Ticket
7.  Today's Sales Report
8.  Advanced Reports
9.  View Ticket History
10. View Service Window Status
11. Exit

Enter choice:
```

---

## 8. Time and Space Complexity

| Operation | Function | Time Complexity | Space Complexity |
|---|---|---|---|
| Initialize Queue | `initQueue()` | O(1) | O(1) |
| Check Full / Empty | `isFull()`, `isEmpty()` | O(1) | O(1) |
| Enqueue Passenger | `enqueue()` | O(1) | O(1) |
| Peek Priority Passenger | `peekPriority()` | O(n) | O(1) |
| Dequeue Priority Passenger | `dequeuePriority()` | O(n) | O(1) |
| Cancel / Remove by Token | `removeByToken()` | O(n) | O(1) |
| Display Queue | `displayQueue()` | O(n) | O(1) |
| Append Ticket to File | `saveTicketSafe()` | O(1) | O(1) |
| Update Ticket (Serve/Cancel)| `updateTicketToServedSafe()` | O(N) | O(1) |
| Search by Token | `findTicketByToken()` | O(N) | O(1) |
| Search by Name | `searchTicketsByName()` | O(N) | O(1) |
| Daily / Advanced Reports | `displayTodayReport()`, etc. | O(N) | O(1) |

*`n` is the number of passengers in memory queue (`n <= 100`); `N` is the total number of historical records in `tickets.csv`.*

---

## 9. Verification & Testing Performed

All 28 verification scenarios were tested and passed:
1. **Passenger Registration**: Verified Railway and Bus registration with all 4 priority levels.
2. **Token Uniqueness**: Confirmed tokens sequentially increment (`T001`, `T002`, `T003`...).
3. **Queue Restoration**: Tested program restarts; unserved (`WAITING`) tickets are loaded back into active queues while preserving timestamps and priorities.
4. **Token Search**: Exact token lookups return complete record details.
5. **Partial Name Search**: Case-insensitive substring matching returns all matching historical records.
6. **Ticket Cancellation**: Successfully cancelled waiting Railway and Bus passengers.
7. **Cancellation Guards**: Attempting to cancel a `SERVED` or `CANCELLED` ticket is strictly rejected.
8. **Circular Queue Removal**: Verified removal from front, middle, and rear of circular buffer with correct index compaction.
9. **Circular Wraparound**: Enqueued and dequeued past `MAX_SIZE` boundary to confirm circular slot reuse.
10. **Priority Ordering**: Emergency and Assistance passengers are served before Normal passengers.
11. **FIFO Tie-Break**: Equal-priority passengers are served strictly in registration order.
12. **Starvation Aging**: Verified that waiting Normal passengers age to higher effective priority tiers over time.
13. **Multi-Window Routing**: Windows 1 & 2 serve Railway; Windows 3 & 4 serve Bus; cross-serving is blocked.
14. **Service Window Workloads**: Accurate historical workload counts and percentage breakdown calculated.
15. **Storage Recovery & Atomic Writes**: Verified temporary file replacement and recovery under interrupted writes.
16. **CSV Quoting**: Passenger names and destinations containing commas and quotation marks are preserved without column shifting.
