#ifndef STORAGE_H
#define STORAGE_H

#include "queue.h"

#define TICKETS_FILE "tickets.csv"

// Complete ticket record representing a line in tickets.csv
typedef struct {
    int token;
    char counter[16];           // "Railway" or "Bus"
    char name[NAME_LEN];
    char destination[DEST_LEN];
    char bookingDate[16];       // DD-MM-YYYY
    char bookingTime[16];       // HH:MM:SS
    int priority;               // 1: Emergency, 2: Assistance, 3: Senior, 4: Normal
    char status[16];            // "WAITING", "SERVED", "CANCELLED"
    char serviceDate[16];       // DD-MM-YYYY or "-"
    char serviceTime[16];       // HH:MM:SS or "-"
    char cancellationDate[16];  // DD-MM-YYYY or "-"
    char cancellationTime[16];  // HH:MM:SS or "-"
    char serviceWindow[32];     // "Railway Window 1", etc. or "-"
    int regSeq;                 // Registration sequence number
} TicketRecord;

// Storage initialization and migration
int initStorage(void);

// Safe ticket persistence operations
int saveTicketSafe(const TicketRecord *rec);
int updateTicketToServedSafe(int token, const char *serviceWindow, const char *srvDate, const char *srvTime);
int updateTicketToCancelledSafe(int token, const char *canDate, const char *canTime);

// Search operations
int findTicketByToken(int token, TicketRecord *out);
int searchTicketsByName(const char *nameQuery, TicketRecord results[], int maxResults);

// Restore queues and track token/sequence counters on startup
int loadTicketsSafe(Queue *railwayQ, Queue *busQ, int *maxToken, int *maxRegSeq);

// Report and display functions
void displayTodayReport(void);
void displayDailyStats(const char *targetDate);
void displayWeeklyStats(void);
void displayMonthlyStats(void);
void displayComparisonStats(const Queue *railwayQ, const Queue *busQ);
void displayPeakHours(void);
void displayWindowPerformance(void);
void displayTicketHistory(void);
void displaySingleTicketRecord(const TicketRecord *rec);

// Time utility helper
void getCurrentDateTime(char *dateStr, int dateSize, char *timeStr, int timeSize);

#endif // STORAGE_H
