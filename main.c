#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "queue.h"
#include "storage.h"

#define RAILWAY 0
#define BUS     1

static Queue counters[2];
static int nextToken = 1;
static int nextRegSeq = 0;
static const char *counterNames[] = { "Railway", "Bus" };

// Remove leading and trailing whitespace
static void trim(char *str)
{
    if (!str) return;

    int start = 0;
    while (str[start] == ' ' || str[start] == '\t' || str[start] == '\n' || str[start] == '\r') {
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

    int len = (int)strlen(str);
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' || str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[--len] = '\0';
    }
}

// Safely read a string from stdin
static int readLine(char *dest, int size, const char *prompt)
{
    char buffer[128];

    printf("%s", prompt);

    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return 0;
    }

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

// Parse token input whether entered as "T005" or "5"
static int readToken(const char *prompt)
{
    char buffer[32];
    printf("%s", prompt);

    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return -1;
    }

    if (strchr(buffer, '\n') == NULL) {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF)
            ;
    }

    trim(buffer);
    if (strlen(buffer) == 0) return -1;

    int tokenNum = -1;
    if (buffer[0] == 'T' || buffer[0] == 't') {
        if (sscanf(buffer + 1, "%d", &tokenNum) != 1) return -1;
    } else {
        if (sscanf(buffer, "%d", &tokenNum) != 1) return -1;
    }

    return tokenNum;
}

// Prompt operator to select Railway or Bus counter
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

// 1. Register new passenger with priority
static void registerPassenger(void)
{
    int c = selectCounter();
    if (c == -1) return;

    if (isFull(&counters[c])) {
        printf("\nQueue is full for %s counter (max capacity %d).\n", counterNames[c], MAX_SIZE);
        return;
    }

    char name[NAME_LEN];
    char dest[DEST_LEN];

    printf("\n");
    if (!readLine(name, NAME_LEN, "Passenger name: ")) {
        printf("Passenger name cannot be empty.\n");
        return;
    }

    if (!readLine(dest, DEST_LEN, "Destination: ")) {
        printf("Destination cannot be empty.\n");
        return;
    }

    printf("\nSelect Priority Category:\n");
    printf("1. Emergency\n");
    printf("2. Passenger requiring assistance\n");
    printf("3. Senior citizen\n");
    printf("4. Normal passenger\n\n");

    int prioChoice = readChoice("Enter priority (1-4): ");
    if (prioChoice < 1 || prioChoice > 4) {
        printf("Invalid priority choice. Registration cancelled.\n");
        return;
    }

    int token = nextToken++;
    int seq = ++nextRegSeq;

    char dateStr[16];
    char timeStr[16];
    getCurrentDateTime(dateStr, sizeof(dateStr), timeStr, sizeof(timeStr));
    time_t now = time(NULL);

    Passenger p;
    memset(&p, 0, sizeof(p));
    p.token = token;
    strncpy(p.name, name, sizeof(p.name) - 1);
    strncpy(p.destination, dest, sizeof(p.destination) - 1);
    p.priority = prioChoice;
    p.bookingTimestamp = (long)now;
    p.regSeq = seq;
    strncpy(p.bookingDate, dateStr, sizeof(p.bookingDate) - 1);
    strncpy(p.bookingTime, timeStr, sizeof(p.bookingTime) - 1);

    // Enqueue into in-memory circular queue
    if (!enqueue(&counters[c], &p)) {
        printf("Failed to register passenger in queue.\n");
        nextToken--;
        nextRegSeq--;
        return;
    }

    // Persist to CSV storage
    TicketRecord rec;
    memset(&rec, 0, sizeof(rec));
    rec.token = token;
    strncpy(rec.counter, counterNames[c], sizeof(rec.counter) - 1);
    strncpy(rec.name, name, sizeof(rec.name) - 1);
    strncpy(rec.destination, dest, sizeof(rec.destination) - 1);
    strncpy(rec.bookingDate, dateStr, sizeof(rec.bookingDate) - 1);
    strncpy(rec.bookingTime, timeStr, sizeof(rec.bookingTime) - 1);
    rec.priority = prioChoice;
    strncpy(rec.status, "WAITING", sizeof(rec.status) - 1);
    strncpy(rec.serviceDate, "-", sizeof(rec.serviceDate) - 1);
    strncpy(rec.serviceTime, "-", sizeof(rec.serviceTime) - 1);
    strncpy(rec.cancellationDate, "-", sizeof(rec.cancellationDate) - 1);
    strncpy(rec.cancellationTime, "-", sizeof(rec.cancellationTime) - 1);
    strncpy(rec.serviceWindow, "-", sizeof(rec.serviceWindow) - 1);
    rec.regSeq = seq;

    if (!saveTicketSafe(&rec)) {
        printf("Storage error: Failed to save ticket to file.\n");
        removeByToken(&counters[c], token, NULL);
        nextToken--;
        nextRegSeq--;
        return;
    }

    printf("\nPassenger registered successfully.\n\n");
    printf("Token:       T%03d\n", token);
    printf("Counter:     %s\n", counterNames[c]);
    printf("Name:        %s\n", name);
    printf("Destination: %s\n", dest);
    printf("Priority:    %s\n", getPriorityName(prioChoice));
    printf("Date:        %s\n", dateStr);
    printf("Time:        %s\n", timeStr);
}

// 2. Serve next passenger at selected service window
static void servePassenger(void)
{
    printf("\nSelect Service Window:\n");
    printf("1. Railway Window 1\n");
    printf("2. Railway Window 2\n");
    printf("3. Bus Window 1\n");
    printf("4. Bus Window 2\n\n");

    int winChoice = readChoice("Enter service window (1-4): ");
    if (winChoice < 1 || winChoice > 4) {
        printf("Invalid window choice.\n");
        return;
    }

    int c = (winChoice <= 2) ? RAILWAY : BUS;
    const char *winName;
    if (winChoice == 1) winName = "Railway Window 1";
    else if (winChoice == 2) winName = "Railway Window 2";
    else if (winChoice == 3) winName = "Bus Window 1";
    else winName = "Bus Window 2";

    if (isEmpty(&counters[c])) {
        printf("\nQueue is empty for %s counter.\n", counterNames[c]);
        return;
    }

    Passenger p;
    time_t now = time(NULL);

    if (!dequeuePriority(&counters[c], &p, (long)now)) {
        printf("\nQueue is empty for %s counter.\n", counterNames[c]);
        return;
    }

    char srvDate[16];
    char srvTime[16];
    getCurrentDateTime(srvDate, sizeof(srvDate), srvTime, sizeof(srvTime));

    if (!updateTicketToServedSafe(p.token, winName, srvDate, srvTime)) {
        printf("\nStorage error: Failed to update ticket status in file.\n");
        enqueue(&counters[c], &p); // Rollback memory dequeue
        return;
    }

    printf("\nServing next passenger at %s...\n\n", winName);
    printf("Token:       T%03d\n", p.token);
    printf("Name:        %s\n", p.name);
    printf("Destination: %s\n", p.destination);
    printf("Priority:    %s\n\n", getPriorityName(p.priority));
    printf("Passenger served successfully.\n");
}

// 3. View waiting queue
static void viewQueue(void)
{
    int c = selectCounter();
    if (c == -1) return;

    displayQueue(&counters[c], counterNames[c], (long)time(NULL));
}

// 4. View next passenger (Peek)
static void viewNextPassenger(void)
{
    int c = selectCounter();
    if (c == -1) return;

    Passenger p;
    if (!peekPriority(&counters[c], &p, (long)time(NULL))) {
        printf("\nQueue is empty.\n");
        return;
    }

    int eff = getEffectivePriority(&p, (long)time(NULL));

    printf("\nNext passenger in queue:\n\n");
    printf("Token:              T%03d\n", p.token);
    printf("Name:               %s\n", p.name);
    printf("Destination:        %s\n", p.destination);
    printf("Original Priority:  %s\n", getPriorityName(p.priority));
    printf("Effective Priority: %s\n", getPriorityName(eff));
}

// 5. Search ticket / passenger
static void searchTicket(void)
{
    printf("\nSearch Options:\n");
    printf("1. Search by Token\n");
    printf("2. Search by Passenger Name\n\n");

    int choice = readChoice("Enter choice (1-2): ");
    if (choice == 1) {
        int token = readToken("\nEnter ticket token (e.g. T001 or 1): ");
        if (token < 0) {
            printf("Invalid token format.\n");
            return;
        }

        TicketRecord rec;
        if (findTicketByToken(token, &rec)) {
            displaySingleTicketRecord(&rec);
        } else {
            printf("\nTicket T%03d not found in records.\n", token);
        }
    } else if (choice == 2) {
        char query[NAME_LEN];
        printf("\nEnter passenger name (or partial name): ");
        if (!readLine(query, NAME_LEN, "")) {
            printf("Search query cannot be empty.\n");
            return;
        }

        TicketRecord results[50];
        int count = searchTicketsByName(query, results, 50);

        if (count == 0) {
            printf("\nNo tickets matching '%s' found.\n", query);
            return;
        }

        printf("\nFound %d matching record(s):\n\n", count);
        printf("%-7s %-8s %-16s %-12s %-11s %-10s %-9s\n",
               "Token", "Counter", "Name", "Destination", "Date", "Priority", "Status");
        printf("--------------------------------------------------------------------------------\n");
        for (int i = 0; i < count; i++) {
            printf("T%03d   %-8s %-16s %-12s %-11s %-10s %-9s\n",
                   results[i].token,
                   results[i].counter,
                   results[i].name,
                   results[i].destination,
                   results[i].bookingDate,
                   getPriorityName(results[i].priority),
                   results[i].status);
        }
        printf("--------------------------------------------------------------------------------\n");
    } else {
        printf("Invalid search choice.\n");
    }
}

// 6. Cancel ticket
static void cancelTicket(void)
{
    int token = readToken("\nEnter ticket token to cancel (e.g. T001 or 1): ");
    if (token < 0) {
        printf("Invalid token format.\n");
        return;
    }

    TicketRecord rec;
    if (!findTicketByToken(token, &rec)) {
        printf("\nTicket T%03d not found in records.\n", token);
        return;
    }

    if (strcmp(rec.status, "SERVED") == 0) {
        printf("\nCannot cancel ticket T%03d: passenger has already been SERVED.\n", token);
        return;
    }

    if (strcmp(rec.status, "CANCELLED") == 0) {
        printf("\nCannot cancel ticket T%03d: ticket is already CANCELLED.\n", token);
        return;
    }

    if (strcmp(rec.status, "WAITING") != 0) {
        printf("\nCannot cancel ticket T%03d: status is %s.\n", token, rec.status);
        return;
    }

    displaySingleTicketRecord(&rec);

    char confirm[16];
    printf("\nAre you sure you want to cancel this ticket? (y/n): ");
    if (!readLine(confirm, sizeof(confirm), "") || (confirm[0] != 'y' && confirm[0] != 'Y')) {
        printf("Cancellation cancelled.\n");
        return;
    }

    int c = (strcmp(rec.counter, "Railway") == 0) ? RAILWAY : BUS;

    // Remove passenger from in-memory queue
    Passenger removed;
    if (!removeByToken(&counters[c], token, &removed)) {
        printf("\nWarning: Ticket not found in active waiting queue.\n");
    }

    // Update status in CSV file
    char canDate[16];
    char canTime[16];
    getCurrentDateTime(canDate, sizeof(canDate), canTime, sizeof(canTime));

    if (!updateTicketToCancelledSafe(token, canDate, canTime)) {
        printf("\nStorage error: Failed to update cancellation in file.\n");
        // Rollback memory queue if removed
        enqueue(&counters[c], &removed);
        return;
    }

    printf("\nTicket T%03d cancelled successfully.\n", token);
}

// 8. Advanced reports submenu
static void advancedReportsMenu(void)
{
    printf("\nAdvanced Reports:\n");
    printf("1. Daily statistics\n");
    printf("2. Weekly statistics (last 7 days)\n");
    printf("3. Monthly statistics (current month)\n");
    printf("4. Railway vs Bus comparison\n");
    printf("5. Peak booking hours\n");
    printf("6. Service-window performance\n\n");

    int choice = readChoice("Enter choice (1-6): ");
    switch (choice) {
        case 1: {
            char dateStr[16];
            char timeStr[16];
            getCurrentDateTime(dateStr, sizeof(dateStr), timeStr, sizeof(timeStr));
            displayDailyStats(dateStr);
            break;
        }
        case 2:
            displayWeeklyStats();
            break;
        case 3:
            displayMonthlyStats();
            break;
        case 4:
            displayComparisonStats(&counters[RAILWAY], &counters[BUS]);
            break;
        case 5:
            displayPeakHours();
            break;
        case 6:
            displayWindowPerformance();
            break;
        default:
            printf("Invalid report choice.\n");
            break;
    }
}

// 10. View service window status
static void viewWindowStatus(void)
{
    displayWindowPerformance();
    printf("\nActive Backlog:\n");
    printf("  Railway waiting: %d\n", counters[RAILWAY].count);
    printf("  Bus waiting:     %d\n", counters[BUS].count);
}

int main(void)
{
    initQueue(&counters[RAILWAY]);
    initQueue(&counters[BUS]);

    // Initialize storage, migrate legacy schema if needed, and restore waiting queues
    initStorage();
    int maxToken = 0;
    int maxSeq = 0;
    loadTicketsSafe(&counters[RAILWAY], &counters[BUS], &maxToken, &maxSeq);
    nextToken = maxToken + 1;
    nextRegSeq = maxSeq;

    // Show menu once on startup
    printf("QUEUEX - TICKET COUNTER\n");
    printf("Railway and Bus Management System\n\n");
    printf("1.  Register New Passenger\n");
    printf("2.  Serve Next Passenger\n");
    printf("3.  View Waiting Queue\n");
    printf("4.  View Next Passenger\n");
    printf("5.  Search Ticket / Passenger\n");
    printf("6.  Cancel Ticket\n");
    printf("7.  Today's Sales Report\n");
    printf("8.  Advanced Reports\n");
    printf("9.  View Ticket History\n");
    printf("10. View Service Window Status\n");
    printf("11. Exit\n\n");

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
                searchTicket();
                break;
            case 6:
                cancelTicket();
                break;
            case 7:
                displayTodayReport();
                break;
            case 8:
                advancedReportsMenu();
                break;
            case 9:
                displayTicketHistory();
                break;
            case 10:
                viewWindowStatus();
                break;
            case 11:
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
