#ifndef STORAGE_H
#define STORAGE_H

#include "queue.h"

#define TICKETS_FILE "tickets.csv"

// Initialize ticket file with CSV header if file does not exist
int initStorage(void);

// Save a newly registered ticket to CSV file
int saveTicket(int token, const char *counter, const char *name, const char *dest, const char *date, const char *timeStr);

// Update status of a ticket to SERVED using temporary file replacement
int updateTicketStatus(int token, const char *newStatus);

// Restore waiting passengers from file into queues on startup
// Returns the highest token number found in the file, or 0 if none
int loadTickets(Queue *railwayQ, Queue *busQ);

// Display summary report of tickets issued and served today
void displayTodayReport(void);

// Display entire ticket history from CSV file
void displayTicketHistory(void);

// Get current system date (DD-MM-YYYY) and time (HH:MM:SS)
void getCurrentDateTime(char *dateStr, int dateSize, char *timeStr, int timeSize);

#endif // STORAGE_H
