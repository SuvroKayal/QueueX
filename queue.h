#ifndef QUEUE_H
#define QUEUE_H

#define MAX_SIZE 100
#define NAME_LEN 60
#define DEST_LEN 60

// Priority levels (1 = highest priority, 4 = lowest)
#define PRIORITY_EMERGENCY   1
#define PRIORITY_ASSISTANCE  2
#define PRIORITY_SENIOR      3
#define PRIORITY_NORMAL      4

// Aging threshold: effective priority improves by 1 level every 120 seconds
#define AGING_INTERVAL_SEC   120

// Structure to store a single passenger's details in the queue
typedef struct {
    int token;
    char name[NAME_LEN];
    char destination[DEST_LEN];
    int priority;              // Original priority (1-4)
    long bookingTimestamp;     // Unix timestamp in seconds
    int regSeq;                // Registration sequence number for FIFO tie-breaking
    char bookingDate[16];      // DD-MM-YYYY
    char bookingTime[16];      // HH:MM:SS
} Passenger;

// Circular queue implementation using a fixed-size array
typedef struct {
    Passenger data[MAX_SIZE];
    int front;
    int rear;
    int count;
} Queue;

// Queue operations
void initQueue(Queue *q);
int isFull(const Queue *q);
int isEmpty(const Queue *q);
int enqueue(Queue *q, const Passenger *p);
int dequeuePriority(Queue *q, Passenger *out, long currentTime);
int peekPriority(const Queue *q, Passenger *out, long currentTime);
int removeByToken(Queue *q, int token, Passenger *out);
void displayQueue(const Queue *q, const char *counterName, long currentTime);

// Priority helper functions
int getEffectivePriority(const Passenger *p, long currentTime);
const char* getPriorityName(int priority);

#endif // QUEUE_H
