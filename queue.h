#ifndef QUEUE_H
#define QUEUE_H

#define MAX_SIZE 100
#define NAME_LEN 60
#define DEST_LEN 60

// Structure to store a single passenger's details
typedef struct {
    int token;
    char name[NAME_LEN];
    char destination[DEST_LEN];
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
int enqueue(Queue *q, int token, const char *name, const char *dest);
int dequeue(Queue *q, Passenger *out);
int peek(const Queue *q, Passenger *out);
void displayQueue(const Queue *q, const char *counterName);

#endif // QUEUE_H
