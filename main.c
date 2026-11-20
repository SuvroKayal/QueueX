#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"
#include "storage.h"

#define RAILWAY 0
#define BUS 1

static Queue counters[2];
static int nextToken = 1;
static const char *counterNames[] = { "Railway", "Bus" };

// Remove leading and trailing whitespace from string
static void trim(char *str)
{
    if (!str) return;

    // Trim leading whitespace
    int start = 0;
    while (str[start] == ' ' || str[start] == '\t' || str[start] == '\n') {
        start++;
    }

    if (start > 0) {
        int i = 0;
        while (str[start + i] != '\0') {
            str[i] = str[start + i];
            i++;
        }
        str[i] = '\0';
    }

    // Trim trailing whitespace
    int len = (int)strlen(str);
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' || str[len - 1] == '\n')) {
        str[--len] = '\0';
    }
}

// Safely read a line of text using fgets
static int readLine(char *dest, int size, const char *prompt)
{
    char buffer[128];

    printf("%s", prompt);

    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return 0;
    }

    // Clear remainder if line exceeded buffer
    if (strchr(buffer, '\n') == NULL) {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF)
            ;
    }

    trim(buffer);

    if (strlen(buffer) == 0) {
        return 0;
    }

    strncpy(dest, buffer, (size_t)(size - 1));
    dest[size - 1] = '\0';
    return 1;
}

// Safely read an integer choice from user
static int readChoice(const char *prompt)
{
    char buffer[32];
    int val = -1;

    printf("%s", prompt);

    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return -1;
    }

    if (strchr(buffer, '\n') == NULL) {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF)
            ;
    }

    if (sscanf(buffer, "%d", &val) != 1) {
        return -1;
    }

    return val;
}

// Ask user to pick Railway or Bus counter
static int selectCounter(void)
{
    printf("\nSelect counter:\n");
    printf("1. Railway\n");
    printf("2. Bus\n\n");

    int choice = readChoice("Enter choice: ");

    if (choice == 1) return RAILWAY;
    if (choice == 2) return BUS;

    printf("Invalid choice. Try again.\n");
    return -1;
}

// Register a new passenger and persist ticket to file
static void registerPassenger(void)
{
    int c = selectCounter();
    if (c == -1) return;

    if (isFull(&counters[c])) {
        printf("\nQueue is full for %s counter.\n", counterNames[c]);
        return;
    }

    char name[NAME_LEN];
    char dest[DEST_LEN];

    printf("\n");
    if (!readLine(name, NAME_LEN, "Passenger name: ")) {
        printf("Passenger name cannot be empty.\n");
        return;
    }
    if (strchr(name, ',') != NULL) {
        printf("Commas are not allowed in passenger name.\n");
        return;
    }

    if (!readLine(dest, DEST_LEN, "Destination: ")) {
        printf("Destination cannot be empty.\n");
        return;
    }
    if (strchr(dest, ',') != NULL) {
        printf("Commas are not allowed in destination.\n");
        return;
    }

    int token = nextToken++;

    // Add passenger to the circular queue first
    if (!enqueue(&counters[c], token, name, dest)) {
        printf("Failed to register passenger in queue.\n");
        nextToken--;
        return;
    }

    // Get current date and time
    char dateStr[16];
    char timeStr[16];
    getCurrentDateTime(dateStr, sizeof(dateStr), timeStr, sizeof(timeStr));

    // Save ticket to file
    if (!saveTicket(token, counterNames[c], name, dest, dateStr, timeStr)) {
        printf("Storage error: Failed to save ticket to file.\n");
        // Rollback queue insertion
        counters[c].rear = (counters[c].rear - 1 + MAX_SIZE) % MAX_SIZE;
        counters[c].count--;
        nextToken--;
        return;
    }

    printf("\nPassenger registered successfully.\n\n");
    printf("Token:       T%03d\n", token);
    printf("Counter:     %s\n", counterNames[c]);
    printf("Name:        %s\n", name);
    printf("Destination: %s\n", dest);
    printf("Date:        %s\n", dateStr);
    printf("Time:        %s\n", timeStr);
}

// Serve the next passenger in queue and update ticket status in file
static void servePassenger(void)
{
    int c = selectCounter();
    if (c == -1) return;

    Passenger p;
    if (!dequeue(&counters[c], &p)) {
        printf("\nQueue is empty.\n");
        return;
    }

    // Update status in tickets.csv to SERVED
    if (!updateTicketStatus(p.token, "SERVED")) {
        printf("\nStorage error: Failed to update ticket status in file.\n");
        // Rollback dequeue to maintain consistency
        counters[c].front = (counters[c].front - 1 + MAX_SIZE) % MAX_SIZE;
        counters[c].count++;
        return;
    }

    printf("\nServing next passenger...\n\n");
    printf("Token:       T%03d\n", p.token);
    printf("Name:        %s\n", p.name);
    printf("Destination: %s\n\n", p.destination);
    printf("Passenger served successfully.\n");
}

// Show all passengers waiting in queue
static void viewQueue(void)
{
    int c = selectCounter();
    if (c == -1) return;

    displayQueue(&counters[c], counterNames[c]);
}

// View next passenger without removing from queue
static void viewNextPassenger(void)
{
    int c = selectCounter();
    if (c == -1) return;

    Passenger p;
    if (!peek(&counters[c], &p)) {
        printf("\nQueue is empty.\n");
        return;
    }

    printf("\nNext passenger in queue:\n\n");
    printf("Token:       T%03d\n", p.token);
    printf("Name:        %s\n", p.name);
    printf("Destination: %s\n", p.destination);
}

int main(void)
{
    initQueue(&counters[RAILWAY]);
    initQueue(&counters[BUS]);

    // Initialize CSV storage and restore waiting passengers
    initStorage();
    int maxToken = loadTickets(&counters[RAILWAY], &counters[BUS]);
    nextToken = maxToken + 1;

    // Show complete menu only once on startup
    printf("QUEUEX - TICKET COUNTER\n");
    printf("Railway and Bus Management System\n\n");
    printf("1. Register passenger\n");
    printf("2. Serve next passenger\n");
    printf("3. View waiting queue\n");
    printf("4. View next passenger\n");
    printf("5. Today's sales report\n");
    printf("6. View ticket history\n");
    printf("7. Exit\n\n");

    while (1) {
        int choice = readChoice("Enter choice: ");
        if (choice == -1) {
            printf("Invalid choice. Try again.\n\n");
            continue;
        }

        switch (choice) {
            case 1:
                registerPassenger();
                break;
            case 2:
                servePassenger();
                break;
            case 3:
                viewQueue();
                break;
            case 4:
                viewNextPassenger();
                break;
            case 5:
                displayTodayReport();
                break;
            case 6:
                displayTicketHistory();
                break;
            case 7:
                printf("\nExiting program.\n");
                return 0;
            default:
                printf("Invalid choice. Try again.\n");
                break;
        }

        printf("\n");
    }

    return 0;
}
