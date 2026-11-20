# QueueX - Ticket Counter Queue Management System

QueueX is a terminal-based Railway and Bus ticket counter simulator written in standard C (C11). It combines in-memory circular queues for fast First-In, First-Out (FIFO) queue management with persistent CSV file storage to manage ticket lifecycles, restore pending queues across program restarts, and generate daily sales reports.

---

## Overview

At busy railway and bus reservation stations, managing passenger queues fairly requires structured queue tracking and reliable record-keeping. QueueX simulates this real-world process:
- Each service counter (Railway and Bus) maintains its own in-memory Circular Queue with a maximum capacity of 100 passengers.
- A single unified token generator ensures that all tickets issued across counters have unique sequential numbers (e.g., T001, T002, T003).
- Every ticket booking and status update is saved to `tickets.csv`.
- On startup, the system scans the CSV file, determines the next available token number, and restores any passengers with status `WAITING` directly back into their active queues.

---

## Features Implemented

1. **Circular Queue Engine**: Fixed-capacity circular array (100 passengers per counter) with O(1) enqueue, dequeue, peek, and capacity checks.
2. **Simplified One-Time Menu**: Displays the full menu once at startup and prompts with a concise `Enter choice:` prompt after every operation, avoiding screen clutter.
3. **Independent Counters**: Independent waiting lines for Railway and Bus.
4. **Shared Token Sequence**: Tokens start from T001 and continue sequentially across both counters without duplicates.
5. **Persistent CSV Storage**: All tickets are stored in `tickets.csv` with fields for token, counter, passenger name, destination, booking date, booking time, status, and service date.
6. **Queue Restoration on Startup**: Unserved (`WAITING`) tickets are restored to their respective circular queues when the program starts.
7. **Safe Status Updates**: When a passenger is served, their record in `tickets.csv` is updated to `SERVED` along with the service date using temporary file replacement.
8. **Today's Sales Report**: Aggregates total tickets issued today, broken down by Railway and Bus, along with tickets served today and currently waiting tickets from today's bookings.
9. **Ticket History**: Displays all stored ticket records in a formatted table.
10. **Robust Input Validation**: Safe reading using `fgets`, whitespace trimming, validation against empty fields and comma injection, and clean handling of invalid menu choices.

---

## Planned Features (Future Phases)

- Token cancellation from the middle of an active queue.
- Dedicated priority lane for senior citizens and disabled passengers.
- Multiple active service windows per counter category.

---

## Modular Project Architecture

QueueX is designed with clean modular separation:

```text
QueueX/
|-- queue.h       # Structure definitions (Passenger, Queue) and queue prototypes
|-- queue.c       # Circular queue algorithms (DSA layer)
|-- storage.h     # Storage interface definitions and file constants
|-- storage.c     # File I/O, CSV parsing, persistence, and reporting
|-- main.c        # User interface, input validation, and main program loop
\-- README.md     # Documentation and guide
```

### Role of `queue.h` and `queue.c`
This layer handles the core Data Structures & Algorithms logic. It manages array indices (`front`, `rear`, `count`) and implements circular wrap-around using the modulo operator (`%`). It operates purely on memory and has no dependency on file operations or UI printing.

### Role of `storage.h` and `storage.c`
This layer handles persistence and report generation. It reads and writes to `tickets.csv`, parses comma-separated records, manages file-level atomic status updates via temporary files, checks system date/time using `<time.h>`, and generates sales summaries.

---

## Circular Queue and FIFO Principle

### Why a Circular Queue?
In a basic linear array queue, dequeuing an element increments `front`, leaving unused slots at the beginning. Once `rear` reaches the last index, new elements cannot be added even if earlier slots are empty.

A **Circular Queue** wraps `rear` and `front` back to index 0 using the modulo operator:
```text
next_rear  = (rear + 1) % MAX_SIZE
next_front = (front + 1) % MAX_SIZE
```
This ensures that memory is reused as passengers are served, eliminating wasted space.

### State Tracking
Queue state is tracked cleanly with a separate `count` field:
- Empty: `count == 0`
- Full: `count == MAX_SIZE`

### First-In, First-Out (FIFO)
Passengers are enqueued at `rear` and dequeued from `front`. The first person to take a token is guaranteed to be served first.

---

## CSV Storage and Lifecycle

Tickets are stored in `tickets.csv` using the following schema:
```csv
Token,Counter,Name,Destination,Date,Time,Status,ServedDate
T001,Railway,Rahul Das,Kolkata,20-11-2026,10:30:00,SERVED,20-11-2026
T002,Bus,Amit Roy,Durgapur,20-11-2026,10:35:00,WAITING,-
```

### Ticket Lifecycle
1. **Registration**: Passenger details are enqueued. If enqueue succeeds, a new line with status `WAITING` and `ServedDate` as `-` is appended to `tickets.csv`.
2. **Restoration**: On program launch, `loadTickets` reads `tickets.csv`. Any ticket marked `WAITING` is enqueued into the appropriate counter queue with its original token, name, and destination.
3. **Serving**: When a passenger is served, `updateTicketStatus` reads `tickets.csv`, rewrites records to `tickets_temp.csv` with status `SERVED` and the current date as `ServedDate`, removes the old file, and renames the temporary file. If writing fails, the queue change is rolled back.

### Today's Sales Report
The report inspects all records in `tickets.csv`:
- **Issued today**: Matches records where `Date` equals the current system date (`DD-MM-YYYY`).
- **Counter breakdown**: Categorizes today's bookings into Railway and Bus counts.
- **Served today**: Matches records with status `SERVED` where `ServedDate` equals today's date.
- **Currently waiting from today**: Matches records with status `WAITING` booked today.

---

## Compilation and Execution

### Requirements
- GCC (MinGW-w64 on Windows, or GCC on Linux/macOS)
- Standard C11 support

### Compilation Command

```powershell
gcc -Wall -Wextra -Wpedantic -std=c11 main.c queue.c storage.c -o QueueX.exe
```

### Execution

```powershell
.\QueueX.exe
```

---

## Example Usage

### Startup and Menu
```text
QUEUEX - TICKET COUNTER
Railway and Bus Management System

1. Register passenger
2. Serve next passenger
3. View waiting queue
4. View next passenger
5. Today's sales report
6. View ticket history
7. Exit

Enter choice: 1
```

### Registering a Passenger
```text
Select counter:
1. Railway
2. Bus

Enter choice: 1

Passenger name: Rahul Das
Destination: Kolkata

Passenger registered successfully.

Token:       T001
Counter:     Railway
Name:        Rahul Das
Destination: Kolkata
Date:        20-11-2026
Time:        10:30:00

Enter choice:
```

### Today's Sales Report
```text
Enter choice: 5

QUEUEX - TODAY'S REPORT

Date: 20-11-2026

Total tickets issued today: 2

Railway tickets: 1
Bus tickets:     1

Tickets served today:                           1
Tickets currently waiting from today's bookings: 1

Enter choice:
```

---

## Complexity Analysis

| Operation | Function | Time Complexity | Space Complexity |
|---|---|---|---|
| Initialize Queue | `initQueue()` | O(1) | O(1) |
| Enqueue Passenger | `enqueue()` | O(1) | O(1) |
| Dequeue Passenger | `dequeue()` | O(1) | O(1) |
| Peek Next Passenger | `peek()` | O(1) | O(1) |
| Display Queue | `displayQueue()` | O(n) | O(1) |
| Append Ticket to File | `saveTicket()` | O(1) | O(1) |
| Update Ticket Status | `updateTicketStatus()` | O(N) | O(1) |
| Restore Queues on Startup | `loadTickets()` | O(N) | O(1) |
| Daily Sales Report | `displayTodayReport()` | O(N) | O(1) |

*`n` is the number of passengers in memory queue; `N` is the total number of records in `tickets.csv`.*

---

## Current Limitations

- Queue capacity is fixed at 100 passengers per counter.
- Priority queueing is not yet supported.
- Cancellation of tickets from the middle of the queue is planned for a future phase.
