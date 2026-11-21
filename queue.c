#include <stdio.h>
#include <string.h>
#include "queue.h"

// Initialize queue to empty state
void initQueue(Queue *q)
{
    q->front = 0;
    q->rear = -1;
    q->count = 0;
}

// Check if queue has reached maximum capacity
int isFull(const Queue *q)
{
    return q->count == MAX_SIZE;
}

// Check if queue is empty
int isEmpty(const Queue *q)
{
    return q->count == 0;
}

// Get readable name for priority level
const char* getPriorityName(int priority)
{
    switch (priority) {
        case PRIORITY_EMERGENCY:   return "Emergency";
        case PRIORITY_ASSISTANCE:  return "Assistance";
        case PRIORITY_SENIOR:      return "Senior";
        case PRIORITY_NORMAL:      return "Normal";
        default:                   return "Normal";
    }
}

// Calculate effective priority by applying aging to prevent starvation
int getEffectivePriority(const Passenger *p, long currentTime)
{
    int eff = p->priority;
    if (currentTime > p->bookingTimestamp && AGING_INTERVAL_SEC > 0) {
        long waitSec = currentTime - p->bookingTimestamp;
        int boost = (int)(waitSec / AGING_INTERVAL_SEC);
        eff -= boost;
        if (eff < PRIORITY_EMERGENCY) {
            eff = PRIORITY_EMERGENCY;
        }
    }
    return eff;
}

// Add a passenger to the rear of the circular queue
int enqueue(Queue *q, const Passenger *p)
{
    if (isFull(q)) {
        return 0;
    }

    // Advance rear with circular wrap-around
    q->rear = (q->rear + 1) % MAX_SIZE;
    q->data[q->rear] = *p;
    q->count++;
    return 1;
}

// Peek at the highest priority passenger without removing them
int peekPriority(const Queue *q, Passenger *out, long currentTime)
{
    if (isEmpty(q)) {
        return 0;
    }

    int bestIdx = q->front;
    int bestEff = getEffectivePriority(&q->data[bestIdx], currentTime);

    // Search from front to rear; ties keep earlier passenger (FIFO)
    for (int i = 1; i < q->count; i++) {
        int idx = (q->front + i) % MAX_SIZE;
        int eff = getEffectivePriority(&q->data[idx], currentTime);
        if (eff < bestEff) {
            bestEff = eff;
            bestIdx = idx;
        }
    }

    *out = q->data[bestIdx];
    return 1;
}

// Remove and return the highest priority passenger from circular queue
int dequeuePriority(Queue *q, Passenger *out, long currentTime)
{
    if (isEmpty(q)) {
        return 0;
    }

    int bestPos = 0;
    int bestIdx = q->front;
    int bestEff = getEffectivePriority(&q->data[bestIdx], currentTime);

    // Find passenger with highest effective priority (FIFO tie-break)
    for (int i = 1; i < q->count; i++) {
        int idx = (q->front + i) % MAX_SIZE;
        int eff = getEffectivePriority(&q->data[idx], currentTime);
        if (eff < bestEff) {
            bestEff = eff;
            bestPos = i;
            bestIdx = idx;
        }
    }

    *out = q->data[bestIdx];

    // Shift subsequent elements forward by one to maintain circular queue order
    for (int i = bestPos; i < q->count - 1; i++) {
        int curr = (q->front + i) % MAX_SIZE;
        int next = (q->front + i + 1) % MAX_SIZE;
        q->data[curr] = q->data[next];
    }

    q->rear = (q->rear - 1 + MAX_SIZE) % MAX_SIZE;
    q->count--;

    if (q->count == 0) {
        initQueue(q);
    }

    return 1;
}

// Remove a specific passenger by token from anywhere in circular queue
int removeByToken(Queue *q, int token, Passenger *out)
{
    if (isEmpty(q)) {
        return 0;
    }

    int targetPos = -1;
    for (int i = 0; i < q->count; i++) {
        int idx = (q->front + i) % MAX_SIZE;
        if (q->data[idx].token == token) {
            targetPos = i;
            if (out != NULL) {
                *out = q->data[idx];
            }
            break;
        }
    }

    if (targetPos == -1) {
        return 0;
    }

    // Shift subsequent passengers to fill the gap
    for (int i = targetPos; i < q->count - 1; i++) {
        int curr = (q->front + i) % MAX_SIZE;
        int next = (q->front + i + 1) % MAX_SIZE;
        q->data[curr] = q->data[next];
    }

    q->rear = (q->rear - 1 + MAX_SIZE) % MAX_SIZE;
    q->count--;

    if (q->count == 0) {
        initQueue(q);
    }

    return 1;
}

// Display all waiting passengers with original and effective priority
void displayQueue(const Queue *q, const char *counterName, long currentTime)
{
    printf("\n%s Waiting Queue\n\n", counterName);

    if (isEmpty(q)) {
        printf("Queue is empty.\n");
        return;
    }

    printf("%-7s %-18s %-14s %-12s %-14s %-8s\n",
           "Token", "Name", "Destination", "Priority", "Effective", "Wait(s)");
    printf("----------------------------------------------------------------------------\n");

    for (int i = 0; i < q->count; i++) {
        int idx = (q->front + i) % MAX_SIZE;
        const Passenger *p = &q->data[idx];
        int eff = getEffectivePriority(p, currentTime);
        long waitSec = (currentTime > p->bookingTimestamp) ? (currentTime - p->bookingTimestamp) : 0;

        printf("T%03d   %-18s %-14s %-12s %-14s %-8ld\n",
               p->token,
               p->name,
               p->destination,
               getPriorityName(p->priority),
               getPriorityName(eff),
               waitSec);
    }

    printf("----------------------------------------------------------------------------\n");
    printf("Total waiting: %d\n", q->count);
}
