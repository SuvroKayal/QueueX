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

// Add a passenger to the rear of the queue
int enqueue(Queue *q, int token, const char *name, const char *dest)
{
    if (isFull(q)) {
        return 0;
    }

    // Move rear forward with wrap-around
    q->rear = (q->rear + 1) % MAX_SIZE;

    q->data[q->rear].token = token;

    strncpy(q->data[q->rear].name, name, NAME_LEN - 1);
    q->data[q->rear].name[NAME_LEN - 1] = '\0';

    strncpy(q->data[q->rear].destination, dest, DEST_LEN - 1);
    q->data[q->rear].destination[DEST_LEN - 1] = '\0';

    q->count++;
    return 1;
}

// Remove the front passenger from the queue
int dequeue(Queue *q, Passenger *out)
{
    if (isEmpty(q)) {
        return 0;
    }

    *out = q->data[q->front];

    // Move front forward with wrap-around
    q->front = (q->front + 1) % MAX_SIZE;
    q->count--;
    return 1;
}

// Look at the front passenger without removing them
int peek(const Queue *q, Passenger *out)
{
    if (isEmpty(q)) {
        return 0;
    }

    *out = q->data[q->front];
    return 1;
}

// Display all passengers waiting in the queue
void displayQueue(const Queue *q, const char *counterName)
{
    printf("\n%s Queue\n\n", counterName);

    if (isEmpty(q)) {
        printf("Queue is empty.\n");
        return;
    }

    printf("%-8s %-20s %-20s\n", "Token", "Name", "Destination");
    printf("--------------------------------------------------\n");

    for (int i = 0; i < q->count; i++) {
        int idx = (q->front + i) % MAX_SIZE;
        printf("T%03d     %-20s %-20s\n",
               q->data[idx].token,
               q->data[idx].name,
               q->data[idx].destination);
    }

    printf("--------------------------------------------------\n");
    printf("Total waiting: %d\n", q->count);
}
