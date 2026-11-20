#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "storage.h"

// Helper to parse comma-separated fields from a line
static int parseCsvFields(char *line, char *fields[], int maxFields)
{
    int count = 0;
    char *p = line;

    while (*p && count < maxFields) {
        fields[count++] = p;
        while (*p && *p != ',' && *p != '\r' && *p != '\n') {
            p++;
        }
        if (*p == ',') {
            *p = '\0';
            p++;
        } else {
            *p = '\0';
            break;
        }
    }
    return count;
}

// Get current system date and time strings
void getCurrentDateTime(char *dateStr, int dateSize, char *timeStr, int timeSize)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    if (t) {
        strftime(dateStr, (size_t)dateSize, "%d-%m-%Y", t);
        strftime(timeStr, (size_t)timeSize, "%H:%M:%S", t);
    } else {
        strncpy(dateStr, "01-01-2026", (size_t)(dateSize - 1));
        dateStr[dateSize - 1] = '\0';
        strncpy(timeStr, "00:00:00", (size_t)(timeSize - 1));
        timeStr[timeSize - 1] = '\0';
    }
}

// Initialize ticket file with CSV header if not already created
int initStorage(void)
{
    FILE *fp = fopen(TICKETS_FILE, "r");
    if (fp) {
        fclose(fp);
        return 1;
    }

    fp = fopen(TICKETS_FILE, "w");
    if (!fp) {
        return 0;
    }

    fprintf(fp, "Token,Counter,Name,Destination,Date,Time,Status,ServedDate\n");
    fclose(fp);
    return 1;
}

// Save a newly registered passenger ticket to CSV file
int saveTicket(int token, const char *counter, const char *name, const char *dest, const char *date, const char *timeStr)
{
    FILE *fp = fopen(TICKETS_FILE, "a");
    if (!fp) {
        return 0;
    }

    fprintf(fp, "T%03d,%s,%s,%s,%s,%s,WAITING,-\n",
            token, counter, name, dest, date, timeStr);

    fclose(fp);
    return 1;
}

// Update ticket status to SERVED using temporary file replacement
int updateTicketStatus(int token, const char *newStatus)
{
    FILE *src = fopen(TICKETS_FILE, "r");
    if (!src) {
        return 0;
    }

    FILE *temp = fopen("tickets_temp.csv", "w");
    if (!temp) {
        fclose(src);
        return 0;
    }

    char todayDate[16];
    char timeStr[16];
    getCurrentDateTime(todayDate, sizeof(todayDate), timeStr, sizeof(timeStr));

    char targetToken[16];
    snprintf(targetToken, sizeof(targetToken), "T%03d", token);

    char line[512];
    int found = 0;

    // Read header line
    if (fgets(line, sizeof(line), src)) {
        fputs(line, temp);
    }

    // Process all ticket records
    while (fgets(line, sizeof(line), src)) {
        char copy[512];
        strncpy(copy, line, sizeof(copy) - 1);
        copy[sizeof(copy) - 1] = '\0';

        char *fields[8];
        int numFields = parseCsvFields(copy, fields, 8);

        if (numFields >= 7 && strcmp(fields[0], targetToken) == 0 && strcmp(fields[6], "WAITING") == 0 && !found) {
            fprintf(temp, "%s,%s,%s,%s,%s,%s,%s,%s\n",
                    fields[0], fields[1], fields[2], fields[3],
                    fields[4], fields[5], newStatus, todayDate);
            found = 1;
        } else {
            fputs(line, temp);
        }
    }

    fclose(src);
    fclose(temp);

    if (found) {
        remove(TICKETS_FILE);
        if (rename("tickets_temp.csv", TICKETS_FILE) != 0) {
            return 0;
        }
        return 1;
    }

    remove("tickets_temp.csv");
    return 0;
}

// Restore waiting tickets into queues and find highest token number
int loadTickets(Queue *railwayQ, Queue *busQ)
{
    FILE *fp = fopen(TICKETS_FILE, "r");
    if (!fp) {
        return 0;
    }

    char line[512];
    int maxToken = 0;

    // Skip header line
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return 0;
    }

    while (fgets(line, sizeof(line), fp)) {
        char *fields[8];
        int numFields = parseCsvFields(line, fields, 8);

        if (numFields >= 7) {
            int tokenNum = 0;
            if (sscanf(fields[0], "T%d", &tokenNum) == 1) {
                if (tokenNum > maxToken) {
                    maxToken = tokenNum;
                }
            }

            // Restore only WAITING passengers into active queues
            if (strcmp(fields[6], "WAITING") == 0) {
                if (strcmp(fields[1], "Railway") == 0) {
                    if (!enqueue(railwayQ, tokenNum, fields[2], fields[3])) {
                        printf("Warning: Railway queue capacity exceeded during restore (Token %s).\n", fields[0]);
                    }
                } else if (strcmp(fields[1], "Bus") == 0) {
                    if (!enqueue(busQ, tokenNum, fields[2], fields[3])) {
                        printf("Warning: Bus queue capacity exceeded during restore (Token %s).\n", fields[0]);
                    }
                }
            }
        }
    }

    fclose(fp);
    return maxToken;
}

// Display today's ticket summary report
void displayTodayReport(void)
{
    char today[16];
    char timeStr[16];
    getCurrentDateTime(today, sizeof(today), timeStr, sizeof(timeStr));

    int totalToday = 0;
    int railwayToday = 0;
    int busToday = 0;
    int servedToday = 0;
    int waitingToday = 0;

    FILE *fp = fopen(TICKETS_FILE, "r");
    if (fp) {
        char line[512];
        // Skip header
        if (fgets(line, sizeof(line), fp)) {
            while (fgets(line, sizeof(line), fp)) {
                char *fields[8];
                int numFields = parseCsvFields(line, fields, 8);

                if (numFields >= 7) {
                    char *counter = fields[1];
                    char *date = fields[4];
                    char *status = fields[6];
                    char *servedDate = (numFields >= 8) ? fields[7] : "-";

                    // Check if ticket was booked today
                    if (strcmp(date, today) == 0) {
                        totalToday++;
                        if (strcmp(counter, "Railway") == 0) {
                            railwayToday++;
                        } else if (strcmp(counter, "Bus") == 0) {
                            busToday++;
                        }
                        if (strcmp(status, "WAITING") == 0) {
                            waitingToday++;
                        }
                    }

                    // Check if ticket was served today
                    if (strcmp(status, "SERVED") == 0) {
                        if (strcmp(servedDate, today) == 0) {
                            servedToday++;
                        } else if (strcmp(servedDate, "-") == 0 && strcmp(date, today) == 0) {
                            servedToday++;
                        }
                    }
                }
            }
        }
        fclose(fp);
    }

    printf("\nQUEUEX - TODAY'S REPORT\n\n");
    printf("Date: %s\n\n", today);
    printf("Total tickets issued today: %d\n\n", totalToday);
    printf("Railway tickets: %d\n", railwayToday);
    printf("Bus tickets:     %d\n\n", busToday);
    printf("Tickets served today:                           %d\n", servedToday);
    printf("Tickets currently waiting from today's bookings: %d\n", waitingToday);
}

// Display all records from ticket history
void displayTicketHistory(void)
{
    FILE *fp = fopen(TICKETS_FILE, "r");
    if (!fp) {
        printf("\nNo ticket records found.\n");
        return;
    }

    char line[512];

    // Read header line
    if (!fgets(line, sizeof(line), fp)) {
        printf("\nNo ticket records found.\n");
        fclose(fp);
        return;
    }

    int recordCount = 0;

    while (fgets(line, sizeof(line), fp)) {
        char *fields[8];
        int numFields = parseCsvFields(line, fields, 8);

        if (numFields >= 7) {
            if (recordCount == 0) {
                printf("\n%-7s %-9s %-18s %-16s %-11s %-9s %-8s\n",
                       "Token", "Counter", "Name", "Destination", "Date", "Time", "Status");
                printf("--------------------------------------------------------------------------------\n");
            }

            printf("%-7s %-9s %-18s %-16s %-11s %-9s %-8s\n",
                   fields[0], fields[1], fields[2], fields[3],
                   fields[4], fields[5], fields[6]);

            recordCount++;
        }
    }

    fclose(fp);

    if (recordCount == 0) {
        printf("\nNo ticket records found.\n");
    } else {
        printf("--------------------------------------------------------------------------------\n");
        printf("Total records: %d\n", recordCount);
    }
}
