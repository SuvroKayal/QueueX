#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "storage.h"

// Parse date and time strings into Unix timestamp
static long parseDateTimeToTimestamp(const char *dateStr, const char *timeStr)
{
    int day = 1, mon = 1, year = 2026;
    int hour = 0, min = 0, sec = 0;

    if (sscanf(dateStr, "%d-%d-%d", &day, &mon, &year) != 3) {
        return (long)time(NULL);
    }
    sscanf(timeStr, "%d:%d:%d", &hour, &min, &sec);

    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_mday = day;
    t.tm_mon  = mon - 1;
    t.tm_year = year - 1900;
    t.tm_hour = hour;
    t.tm_min  = min;
    t.tm_sec  = sec;
    t.tm_isdst = -1;

    time_t res = mktime(&t);
    if (res == (time_t)-1) {
        return (long)time(NULL);
    }
    return (long)res;
}

// Convert priority string from CSV to numeric priority level
static int parsePriorityString(const char *str)
{
    if (strcmp(str, "Emergency") == 0) return PRIORITY_EMERGENCY;
    if (strcmp(str, "Assistance") == 0 || strcmp(str, "Assistance Required") == 0) return PRIORITY_ASSISTANCE;
    if (strcmp(str, "Senior") == 0 || strcmp(str, "Senior Citizen") == 0) return PRIORITY_SENIOR;
    return PRIORITY_NORMAL;
}

// Safely write a CSV field, quoting if it contains commas or quotes
static void writeCsvField(FILE *fp, const char *str, int isLast)
{
    int needsQuotes = 0;
    if (strchr(str, ',') != NULL || strchr(str, '"') != NULL || strchr(str, '\n') != NULL) {
        needsQuotes = 1;
    }

    if (needsQuotes) {
        fputc('"', fp);
        for (const char *p = str; *p != '\0'; p++) {
            if (*p == '"') {
                fputc('"', fp);
            }
            fputc(*p, fp);
        }
        fputc('"', fp);
    } else {
        fputs(str, fp);
    }

    if (isLast) {
        fputc('\n', fp);
    } else {
        fputc(',', fp);
    }
}

// Parse a CSV line into fields, correctly handling quoted fields
static int parseCsvRow(char *line, char *fields[], int maxFields)
{
    int count = 0;
    char *src = line;
    char *dst = line;
    int inQuotes = 0;

    // Strip trailing newline characters
    int len = (int)strlen(line);
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
        line[--len] = '\0';
    }

    while (*src != '\0' && count < maxFields) {
        fields[count++] = dst;

        while (*src != '\0') {
            if (*src == '"') {
                if (inQuotes && *(src + 1) == '"') {
                    *dst++ = '"';
                    src += 2;
                } else {
                    inQuotes = !inQuotes;
                    src++;
                }
            } else if (*src == ',' && !inQuotes) {
                src++;
                break;
            } else {
                *dst++ = *src++;
            }
        }
        *dst++ = '\0';
    }

    return count;
}

// Get current system date (DD-MM-YYYY) and time (HH:MM:SS)
void getCurrentDateTime(char *dateStr, int dateSize, char *timeStr, int timeSize)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    if (t) {
        strftime(dateStr, (size_t)dateSize, "%d-%m-%Y", t);
        strftime(timeStr, (size_t)timeSize, "%H:%M:%S", t);
    } else {
        strncpy(dateStr, "21-11-2026", (size_t)(dateSize - 1));
        dateStr[dateSize - 1] = '\0';
        strncpy(timeStr, "10:00:00", (size_t)(timeSize - 1));
        timeStr[timeSize - 1] = '\0';
    }
}

// Copy a file byte-by-byte for backup
static int copyFile(const char *srcPath, const char *dstPath)
{
    FILE *src = fopen(srcPath, "rb");
    if (!src) return 0;

    FILE *dst = fopen(dstPath, "wb");
    if (!dst) {
        fclose(src);
        return 0;
    }

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes, dst) != bytes) {
            fclose(src);
            fclose(dst);
            return 0;
        }
    }

    fclose(src);
    fclose(dst);
    return 1;
}

// Parse a line from CSV into TicketRecord
static int parseRecordFromFields(char *fields[], int numFields, TicketRecord *rec)
{
    if (numFields < 7) {
        return 0;
    }

    memset(rec, 0, sizeof(*rec));

    int tokenNum = 0;
    if (sscanf(fields[0], "T%d", &tokenNum) == 1) {
        rec->token = tokenNum;
    } else {
        rec->token = atoi(fields[0]);
    }

    strncpy(rec->counter, fields[1], sizeof(rec->counter) - 1);
    strncpy(rec->name, fields[2], sizeof(rec->name) - 1);
    strncpy(rec->destination, fields[3], sizeof(rec->destination) - 1);
    strncpy(rec->bookingDate, fields[4], sizeof(rec->bookingDate) - 1);
    strncpy(rec->bookingTime, fields[5], sizeof(rec->bookingTime) - 1);

    if (numFields >= 14) {
        rec->priority = parsePriorityString(fields[6]);
        strncpy(rec->status, fields[7], sizeof(rec->status) - 1);
        strncpy(rec->serviceDate, fields[8], sizeof(rec->serviceDate) - 1);
        strncpy(rec->serviceTime, fields[9], sizeof(rec->serviceTime) - 1);
        strncpy(rec->cancellationDate, fields[10], sizeof(rec->cancellationDate) - 1);
        strncpy(rec->cancellationTime, fields[11], sizeof(rec->cancellationTime) - 1);
        strncpy(rec->serviceWindow, fields[12], sizeof(rec->serviceWindow) - 1);
        rec->regSeq = atoi(fields[13]);
    } else {
        // Legacy migration mapping
        rec->priority = PRIORITY_NORMAL;
        strncpy(rec->status, fields[6], sizeof(rec->status) - 1);

        if (strcmp(rec->status, "SERVED") == 0) {
            if (numFields >= 8 && strcmp(fields[7], "-") != 0) {
                strncpy(rec->serviceDate, fields[7], sizeof(rec->serviceDate) - 1);
            } else {
                strncpy(rec->serviceDate, rec->bookingDate, sizeof(rec->serviceDate) - 1);
            }
            strncpy(rec->serviceTime, rec->bookingTime, sizeof(rec->serviceTime) - 1);
            snprintf(rec->serviceWindow, sizeof(rec->serviceWindow), "%s Window 1", rec->counter);
        } else {
            strncpy(rec->serviceDate, "-", sizeof(rec->serviceDate) - 1);
            strncpy(rec->serviceTime, "-", sizeof(rec->serviceTime) - 1);
            strncpy(rec->serviceWindow, "-", sizeof(rec->serviceWindow) - 1);
        }

        strncpy(rec->cancellationDate, "-", sizeof(rec->cancellationDate) - 1);
        strncpy(rec->cancellationTime, "-", sizeof(rec->cancellationTime) - 1);
        rec->regSeq = rec->token;
    }

    return 1;
}

// Write a TicketRecord struct as a line in CSV
static void writeRecordToCsv(FILE *fp, const TicketRecord *rec)
{
    char tokenStr[16];
    snprintf(tokenStr, sizeof(tokenStr), "T%03d", rec->token);
    writeCsvField(fp, tokenStr, 0);
    writeCsvField(fp, rec->counter, 0);
    writeCsvField(fp, rec->name, 0);
    writeCsvField(fp, rec->destination, 0);
    writeCsvField(fp, rec->bookingDate, 0);
    writeCsvField(fp, rec->bookingTime, 0);
    writeCsvField(fp, getPriorityName(rec->priority), 0);
    writeCsvField(fp, rec->status, 0);
    writeCsvField(fp, rec->serviceDate, 0);
    writeCsvField(fp, rec->serviceTime, 0);
    writeCsvField(fp, rec->cancellationDate, 0);
    writeCsvField(fp, rec->cancellationTime, 0);
    writeCsvField(fp, rec->serviceWindow, 0);

    char seqStr[16];
    snprintf(seqStr, sizeof(seqStr), "%d", rec->regSeq);
    writeCsvField(fp, seqStr, 1);
}

// Initialize ticket storage; handles missing files and migrates older schemas
int initStorage(void)
{
    FILE *fp = fopen(TICKETS_FILE, "r");
    if (!fp) {
        fp = fopen(TICKETS_FILE, "w");
        if (!fp) return 0;
        fprintf(fp, "Token,Counter,Name,Destination,BookingDate,BookingTime,Priority,Status,ServiceDate,ServiceTime,CancellationDate,CancellationTime,ServiceWindow,RegistrationSequence\n");
        fclose(fp);
        return 1;
    }

    char header[512];
    if (!fgets(header, sizeof(header), fp)) {
        fclose(fp);
        fp = fopen(TICKETS_FILE, "w");
        if (!fp) return 0;
        fprintf(fp, "Token,Counter,Name,Destination,BookingDate,BookingTime,Priority,Status,ServiceDate,ServiceTime,CancellationDate,CancellationTime,ServiceWindow,RegistrationSequence\n");
        fclose(fp);
        return 1;
    }

    char headerCopy[512];
    strncpy(headerCopy, header, sizeof(headerCopy) - 1);
    headerCopy[sizeof(headerCopy) - 1] = '\0';

    char *fields[20];
    int numCols = parseCsvRow(headerCopy, fields, 20);

    // If schema matches current 14 columns, no migration needed
    if (numCols >= 14) {
        fclose(fp);
        return 1;
    }

    // Schema migration needed: backup existing file first
    printf("Migrating existing tickets.csv to new schema...\n");
    copyFile(TICKETS_FILE, "tickets_backup_legacy.csv");

    FILE *temp = fopen("tickets_temp.csv", "w");
    if (!temp) {
        fclose(fp);
        return 0;
    }

    fprintf(temp, "Token,Counter,Name,Destination,BookingDate,BookingTime,Priority,Status,ServiceDate,ServiceTime,CancellationDate,CancellationTime,ServiceWindow,RegistrationSequence\n");

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char *f[20];
        char lineCopy[512];
        strncpy(lineCopy, line, sizeof(lineCopy) - 1);
        lineCopy[sizeof(lineCopy) - 1] = '\0';

        int cnt = parseCsvRow(lineCopy, f, 20);
        TicketRecord rec;
        if (parseRecordFromFields(f, cnt, &rec)) {
            writeRecordToCsv(temp, &rec);
        }
    }

    fclose(fp);
    fclose(temp);

    remove(TICKETS_FILE);
    rename("tickets_temp.csv", TICKETS_FILE);
    printf("Migration completed successfully.\n");
    return 1;
}

// Append a newly registered ticket record to CSV
int saveTicketSafe(const TicketRecord *rec)
{
    FILE *fp = fopen(TICKETS_FILE, "a");
    if (!fp) return 0;

    writeRecordToCsv(fp, rec);

    if (fflush(fp) != 0) {
        fclose(fp);
        return 0;
    }

    fclose(fp);
    return 1;
}

// Update a ticket's status to SERVED using safe temporary file replacement
int updateTicketToServedSafe(int token, const char *serviceWindow, const char *srvDate, const char *srvTime)
{
    FILE *src = fopen(TICKETS_FILE, "r");
    if (!src) return 0;

    FILE *temp = fopen("tickets_temp.csv", "w");
    if (!temp) {
        fclose(src);
        return 0;
    }

    char line[512];
    int found = 0;

    // Copy header
    if (fgets(line, sizeof(line), src)) {
        fputs(line, temp);
    }

    while (fgets(line, sizeof(line), src)) {
        char lineCopy[512];
        strncpy(lineCopy, line, sizeof(lineCopy) - 1);
        lineCopy[sizeof(lineCopy) - 1] = '\0';

        char *fields[20];
        int numFields = parseCsvRow(lineCopy, fields, 20);
        TicketRecord rec;

        if (parseRecordFromFields(fields, numFields, &rec)) {
            if (rec.token == token && strcmp(rec.status, "WAITING") == 0 && !found) {
                strncpy(rec.status, "SERVED", sizeof(rec.status) - 1);
                strncpy(rec.serviceDate, srvDate, sizeof(rec.serviceDate) - 1);
                strncpy(rec.serviceTime, srvTime, sizeof(rec.serviceTime) - 1);
                strncpy(rec.serviceWindow, serviceWindow, sizeof(rec.serviceWindow) - 1);
                found = 1;
            }
            writeRecordToCsv(temp, &rec);
        } else {
            fputs(line, temp);
        }
    }

    fflush(temp);
    fclose(src);
    fclose(temp);

    if (found) {
        copyFile(TICKETS_FILE, "tickets_backup.csv");
        remove(TICKETS_FILE);
        if (rename("tickets_temp.csv", TICKETS_FILE) != 0) {
            rename("tickets_backup.csv", TICKETS_FILE);
            return 0;
        }
        remove("tickets_backup.csv");
        return 1;
    }

    remove("tickets_temp.csv");
    return 0;
}

// Update a ticket's status to CANCELLED using safe temporary file replacement
int updateTicketToCancelledSafe(int token, const char *canDate, const char *canTime)
{
    FILE *src = fopen(TICKETS_FILE, "r");
    if (!src) return 0;

    FILE *temp = fopen("tickets_temp.csv", "w");
    if (!temp) {
        fclose(src);
        return 0;
    }

    char line[512];
    int found = 0;

    if (fgets(line, sizeof(line), src)) {
        fputs(line, temp);
    }

    while (fgets(line, sizeof(line), src)) {
        char lineCopy[512];
        strncpy(lineCopy, line, sizeof(lineCopy) - 1);
        lineCopy[sizeof(lineCopy) - 1] = '\0';

        char *fields[20];
        int numFields = parseCsvRow(lineCopy, fields, 20);
        TicketRecord rec;

        if (parseRecordFromFields(fields, numFields, &rec)) {
            if (rec.token == token && strcmp(rec.status, "WAITING") == 0 && !found) {
                strncpy(rec.status, "CANCELLED", sizeof(rec.status) - 1);
                strncpy(rec.cancellationDate, canDate, sizeof(rec.cancellationDate) - 1);
                strncpy(rec.cancellationTime, canTime, sizeof(rec.cancellationTime) - 1);
                found = 1;
            }
            writeRecordToCsv(temp, &rec);
        } else {
            fputs(line, temp);
        }
    }

    fflush(temp);
    fclose(src);
    fclose(temp);

    if (found) {
        copyFile(TICKETS_FILE, "tickets_backup.csv");
        remove(TICKETS_FILE);
        if (rename("tickets_temp.csv", TICKETS_FILE) != 0) {
            rename("tickets_backup.csv", TICKETS_FILE);
            return 0;
        }
        remove("tickets_backup.csv");
        return 1;
    }

    remove("tickets_temp.csv");
    return 0;
}

// Find ticket record by exact token number
int findTicketByToken(int token, TicketRecord *out)
{
    FILE *fp = fopen(TICKETS_FILE, "r");
    if (!fp) return 0;

    char line[512];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return 0;
    }

    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        char lineCopy[512];
        strncpy(lineCopy, line, sizeof(lineCopy) - 1);
        lineCopy[sizeof(lineCopy) - 1] = '\0';

        char *fields[20];
        int numFields = parseCsvRow(lineCopy, fields, 20);
        TicketRecord rec;

        if (parseRecordFromFields(fields, numFields, &rec)) {
            if (rec.token == token) {
                *out = rec;
                found = 1;
                break;
            }
        }
    }

    fclose(fp);
    return found;
}

// Case-insensitive substring search helper
static int strCaseContains(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return 0;
    if (*needle == '\0') return 1;

    int hLen = (int)strlen(haystack);
    int nLen = (int)strlen(needle);
    if (nLen > hLen) return 0;

    for (int i = 0; i <= hLen - nLen; i++) {
        int match = 1;
        for (int j = 0; j < nLen; j++) {
            if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j])) {
                match = 0;
                break;
            }
        }
        if (match) return 1;
    }
    return 0;
}

// Search ticket history by partial passenger name
int searchTicketsByName(const char *nameQuery, TicketRecord results[], int maxResults)
{
    FILE *fp = fopen(TICKETS_FILE, "r");
    if (!fp) return 0;

    char line[512];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return 0;
    }

    int count = 0;
    while (fgets(line, sizeof(line), fp) && count < maxResults) {
        char lineCopy[512];
        strncpy(lineCopy, line, sizeof(lineCopy) - 1);
        lineCopy[sizeof(lineCopy) - 1] = '\0';

        char *fields[20];
        int numFields = parseCsvRow(lineCopy, fields, 20);
        TicketRecord rec;

        if (parseRecordFromFields(fields, numFields, &rec)) {
            if (strCaseContains(rec.name, nameQuery)) {
                results[count++] = rec;
            }
        }
    }

    fclose(fp);
    return count;
}

// Restore waiting passengers into queues and track max token and regSeq
int loadTicketsSafe(Queue *railwayQ, Queue *busQ, int *maxToken, int *maxRegSeq)
{
    FILE *fp = fopen(TICKETS_FILE, "r");
    if (!fp) return 0;

    char line[512];
    *maxToken = 0;
    *maxRegSeq = 0;

    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return 0;
    }

    int restoredCount = 0;
    while (fgets(line, sizeof(line), fp)) {
        char lineCopy[512];
        strncpy(lineCopy, line, sizeof(lineCopy) - 1);
        lineCopy[sizeof(lineCopy) - 1] = '\0';

        char *fields[20];
        int numFields = parseCsvRow(lineCopy, fields, 20);
        TicketRecord rec;

        if (parseRecordFromFields(fields, numFields, &rec)) {
            if (rec.token > *maxToken) {
                *maxToken = rec.token;
            }
            if (rec.regSeq > *maxRegSeq) {
                *maxRegSeq = rec.regSeq;
            }

            // Restore only unserved WAITING tickets
            if (strcmp(rec.status, "WAITING") == 0) {
                Passenger p;
                memset(&p, 0, sizeof(p));
                p.token = rec.token;
                strncpy(p.name, rec.name, sizeof(p.name) - 1);
                strncpy(p.destination, rec.destination, sizeof(p.destination) - 1);
                p.priority = rec.priority;
                p.bookingTimestamp = parseDateTimeToTimestamp(rec.bookingDate, rec.bookingTime);
                p.regSeq = rec.regSeq;
                strncpy(p.bookingDate, rec.bookingDate, sizeof(p.bookingDate) - 1);
                strncpy(p.bookingTime, rec.bookingTime, sizeof(p.bookingTime) - 1);

                if (strcmp(rec.counter, "Railway") == 0) {
                    if (!enqueue(railwayQ, &p)) {
                        printf("Warning: Railway queue capacity exceeded while restoring Token T%03d.\n", rec.token);
                    } else {
                        restoredCount++;
                    }
                } else if (strcmp(rec.counter, "Bus") == 0) {
                    if (!enqueue(busQ, &p)) {
                        printf("Warning: Bus queue capacity exceeded while restoring Token T%03d.\n", rec.token);
                    } else {
                        restoredCount++;
                    }
                }
            }
        }
    }

    fclose(fp);
    return restoredCount;
}

// Display full details of a single ticket record
void displaySingleTicketRecord(const TicketRecord *rec)
{
    printf("\nTicket Details:\n");
    printf("  Token:             T%03d\n", rec->token);
    printf("  Counter:           %s\n", rec->counter);
    printf("  Passenger Name:    %s\n", rec->name);
    printf("  Destination:       %s\n", rec->destination);
    printf("  Priority:          %s\n", getPriorityName(rec->priority));
    printf("  Booking Date/Time: %s %s\n", rec->bookingDate, rec->bookingTime);
    printf("  Current Status:    %s\n", rec->status);

    if (strcmp(rec->status, "SERVED") == 0) {
        printf("  Service Window:    %s\n", rec->serviceWindow);
        printf("  Service Date/Time: %s %s\n", rec->serviceDate, rec->serviceTime);
    } else if (strcmp(rec->status, "CANCELLED") == 0) {
        printf("  Cancel Date/Time:  %s %s\n", rec->cancellationDate, rec->cancellationTime);
    }
}

// Display summary report of tickets booked, served, and cancelled today
void displayTodayReport(void)
{
    char today[16];
    char timeStr[16];
    getCurrentDateTime(today, sizeof(today), timeStr, sizeof(timeStr));

    int totalBooked = 0, railwayBooked = 0, busBooked = 0;
    int waitingToday = 0;
    int totalServed = 0, railwayServed = 0, busServed = 0;
    int totalCancelled = 0, railwayCancelled = 0, busCancelled = 0;

    FILE *fp = fopen(TICKETS_FILE, "r");
    if (fp) {
        char line[512];
        if (fgets(line, sizeof(line), fp)) {
            while (fgets(line, sizeof(line), fp)) {
                char lineCopy[512];
                strncpy(lineCopy, line, sizeof(lineCopy) - 1);
                lineCopy[sizeof(lineCopy) - 1] = '\0';

                char *fields[20];
                int n = parseCsvRow(lineCopy, fields, 20);
                TicketRecord rec;

                if (parseRecordFromFields(fields, n, &rec)) {
                    if (strcmp(rec.bookingDate, today) == 0) {
                        totalBooked++;
                        if (strcmp(rec.counter, "Railway") == 0) railwayBooked++;
                        else if (strcmp(rec.counter, "Bus") == 0) busBooked++;
                        if (strcmp(rec.status, "WAITING") == 0) waitingToday++;
                    }
                    if (strcmp(rec.status, "SERVED") == 0 && strcmp(rec.serviceDate, today) == 0) {
                        totalServed++;
                        if (strcmp(rec.counter, "Railway") == 0) railwayServed++;
                        else if (strcmp(rec.counter, "Bus") == 0) busServed++;
                    }
                    if (strcmp(rec.status, "CANCELLED") == 0 && strcmp(rec.cancellationDate, today) == 0) {
                        totalCancelled++;
                        if (strcmp(rec.counter, "Railway") == 0) railwayCancelled++;
                        else if (strcmp(rec.counter, "Bus") == 0) busCancelled++;
                    }
                }
            }
        }
        fclose(fp);
    }

    printf("\nQUEUEX - TODAY'S REPORT\n\n");
    printf("Date: %s\n\n", today);
    printf("Total tickets booked today:                     %d\n", totalBooked);
    printf("  Railway tickets booked today:                 %d\n", railwayBooked);
    printf("  Bus tickets booked today:                     %d\n\n", busBooked);

    printf("Total passengers currently waiting from today:  %d\n\n", waitingToday);

    printf("Total passengers served today:                  %d\n", totalServed);
    printf("  Railway passengers served:                    %d\n", railwayServed);
    printf("  Bus passengers served:                        %d\n\n", busServed);

    printf("Total tickets cancelled today:                  %d\n", totalCancelled);
    printf("  Railway cancellations:                        %d\n", railwayCancelled);
    printf("  Bus cancellations:                            %d\n", busCancelled);
}

// Display daily statistics for a specific target date
void displayDailyStats(const char *targetDate)
{
    int booked = 0, served = 0, cancelled = 0, waiting = 0;

    FILE *fp = fopen(TICKETS_FILE, "r");
    if (fp) {
        char line[512];
        if (fgets(line, sizeof(line), fp)) {
            while (fgets(line, sizeof(line), fp)) {
                char lineCopy[512];
                strncpy(lineCopy, line, sizeof(lineCopy) - 1);
                lineCopy[sizeof(lineCopy) - 1] = '\0';

                char *fields[20];
                int n = parseCsvRow(lineCopy, fields, 20);
                TicketRecord rec;

                if (parseRecordFromFields(fields, n, &rec)) {
                    if (strcmp(rec.bookingDate, targetDate) == 0) {
                        booked++;
                        if (strcmp(rec.status, "WAITING") == 0) waiting++;
                    }
                    if (strcmp(rec.status, "SERVED") == 0 && strcmp(rec.serviceDate, targetDate) == 0) {
                        served++;
                    }
                    if (strcmp(rec.status, "CANCELLED") == 0 && strcmp(rec.cancellationDate, targetDate) == 0) {
                        cancelled++;
                    }
                }
            }
        }
        fclose(fp);
    }

    printf("\nDaily Statistics (%s):\n", targetDate);
    printf("  Tickets Booked:            %d\n", booked);
    printf("  Tickets Served:            %d\n", served);
    printf("  Tickets Cancelled:         %d\n", cancelled);
    printf("  Currently Waiting (today): %d\n", waiting);
}

// Display weekly statistics for the past 7 days
void displayWeeklyStats(void)
{
    time_t now = time(NULL);
    long sevenDaysAgo = (long)now - (7 * 86400L);

    int booked = 0, served = 0, cancelled = 0, waiting = 0;

    FILE *fp = fopen(TICKETS_FILE, "r");
    if (fp) {
        char line[512];
        if (fgets(line, sizeof(line), fp)) {
            while (fgets(line, sizeof(line), fp)) {
                char lineCopy[512];
                strncpy(lineCopy, line, sizeof(lineCopy) - 1);
                lineCopy[sizeof(lineCopy) - 1] = '\0';

                char *fields[20];
                int n = parseCsvRow(lineCopy, fields, 20);
                TicketRecord rec;

                if (parseRecordFromFields(fields, n, &rec)) {
                    long bTime = parseDateTimeToTimestamp(rec.bookingDate, rec.bookingTime);
                    if (bTime >= sevenDaysAgo && bTime <= (long)now) {
                        booked++;
                        if (strcmp(rec.status, "WAITING") == 0) waiting++;
                    }
                    if (strcmp(rec.status, "SERVED") == 0) {
                        long sTime = parseDateTimeToTimestamp(rec.serviceDate, rec.serviceTime);
                        if (sTime >= sevenDaysAgo && sTime <= (long)now) served++;
                    }
                    if (strcmp(rec.status, "CANCELLED") == 0) {
                        long cTime = parseDateTimeToTimestamp(rec.cancellationDate, rec.cancellationTime);
                        if (cTime >= sevenDaysAgo && cTime <= (long)now) cancelled++;
                    }
                }
            }
        }
        fclose(fp);
    }

    printf("\nWeekly Statistics (Last 7 Days):\n");
    printf("  Tickets Booked:            %d\n", booked);
    printf("  Tickets Served:            %d\n", served);
    printf("  Tickets Cancelled:         %d\n", cancelled);
    printf("  Currently Waiting (week):  %d\n", waiting);
}

// Display monthly statistics for the current month
void displayMonthlyStats(void)
{
    time_t now = time(NULL);
    struct tm *curr = localtime(&now);
    int currentMon = curr ? (curr->tm_mon + 1) : 11;
    int currentYear = curr ? (curr->tm_year + 1900) : 2026;

    int booked = 0, served = 0, cancelled = 0, waiting = 0;

    FILE *fp = fopen(TICKETS_FILE, "r");
    if (fp) {
        char line[512];
        if (fgets(line, sizeof(line), fp)) {
            while (fgets(line, sizeof(line), fp)) {
                char lineCopy[512];
                strncpy(lineCopy, line, sizeof(lineCopy) - 1);
                lineCopy[sizeof(lineCopy) - 1] = '\0';

                char *fields[20];
                int n = parseCsvRow(lineCopy, fields, 20);
                TicketRecord rec;

                if (parseRecordFromFields(fields, n, &rec)) {
                    int d = 0, m = 0, y = 0;
                    if (sscanf(rec.bookingDate, "%d-%d-%d", &d, &m, &y) == 3) {
                        if (m == currentMon && y == currentYear) {
                            booked++;
                            if (strcmp(rec.status, "WAITING") == 0) waiting++;
                        }
                    }
                    if (strcmp(rec.status, "SERVED") == 0 && sscanf(rec.serviceDate, "%d-%d-%d", &d, &m, &y) == 3) {
                        if (m == currentMon && y == currentYear) served++;
                    }
                    if (strcmp(rec.status, "CANCELLED") == 0 && sscanf(rec.cancellationDate, "%d-%d-%d", &d, &m, &y) == 3) {
                        if (m == currentMon && y == currentYear) cancelled++;
                    }
                }
            }
        }
        fclose(fp);
    }

    printf("\nMonthly Statistics (%02d-%d):\n", currentMon, currentYear);
    printf("  Tickets Booked:            %d\n", booked);
    printf("  Tickets Served:            %d\n", served);
    printf("  Tickets Cancelled:         %d\n", cancelled);
    printf("  Currently Waiting (month): %d\n", waiting);
}

// Display comparison of Railway versus Bus statistics
void displayComparisonStats(const Queue *railwayQ, const Queue *busQ)
{
    int rBooked = 0, bBooked = 0;
    int rServed = 0, bServed = 0;
    int rCancelled = 0, bCancelled = 0;

    FILE *fp = fopen(TICKETS_FILE, "r");
    if (fp) {
        char line[512];
        if (fgets(line, sizeof(line), fp)) {
            while (fgets(line, sizeof(line), fp)) {
                char lineCopy[512];
                strncpy(lineCopy, line, sizeof(lineCopy) - 1);
                lineCopy[sizeof(lineCopy) - 1] = '\0';

                char *fields[20];
                int n = parseCsvRow(lineCopy, fields, 20);
                TicketRecord rec;

                if (parseRecordFromFields(fields, n, &rec)) {
                    if (strcmp(rec.counter, "Railway") == 0) {
                        rBooked++;
                        if (strcmp(rec.status, "SERVED") == 0) rServed++;
                        else if (strcmp(rec.status, "CANCELLED") == 0) rCancelled++;
                    } else if (strcmp(rec.counter, "Bus") == 0) {
                        bBooked++;
                        if (strcmp(rec.status, "SERVED") == 0) bServed++;
                        else if (strcmp(rec.status, "CANCELLED") == 0) bCancelled++;
                    }
                }
            }
        }
        fclose(fp);
    }

    printf("\nRailway vs Bus Comparison:\n\n");
    printf("%-26s %-12s %-12s\n", "Metric", "Railway", "Bus");
    printf("--------------------------------------------------\n");
    printf("%-26s %-12d %-12d\n", "Total Bookings", rBooked, bBooked);
    printf("%-26s %-12d %-12d\n", "Passengers Served", rServed, bServed);
    printf("%-26s %-12d %-12d\n", "Tickets Cancelled", rCancelled, bCancelled);
    printf("%-26s %-12d %-12d\n", "Currently Waiting", railwayQ->count, busQ->count);
    printf("--------------------------------------------------\n");
}

// Display peak booking hours distribution
void displayPeakHours(void)
{
    int hourlyCounts[24] = {0};
    int total = 0;

    FILE *fp = fopen(TICKETS_FILE, "r");
    if (fp) {
        char line[512];
        if (fgets(line, sizeof(line), fp)) {
            while (fgets(line, sizeof(line), fp)) {
                char lineCopy[512];
                strncpy(lineCopy, line, sizeof(lineCopy) - 1);
                lineCopy[sizeof(lineCopy) - 1] = '\0';

                char *fields[20];
                int n = parseCsvRow(lineCopy, fields, 20);
                TicketRecord rec;

                if (parseRecordFromFields(fields, n, &rec)) {
                    int h = 0, m = 0, s = 0;
                    if (sscanf(rec.bookingTime, "%d:%d:%d", &h, &m, &s) >= 1) {
                        if (h >= 0 && h < 24) {
                            hourlyCounts[h]++;
                            total++;
                        }
                    }
                }
            }
        }
        fclose(fp);
    }

    printf("\nPeak Booking Hours Distribution:\n\n");
    if (total == 0) {
        printf("No ticket records found.\n");
        return;
    }

    printf("%-15s %-12s %-10s\n", "Hour Window", "Bookings", "Share");
    printf("-----------------------------------------\n");

    for (int h = 0; h < 24; h++) {
        if (hourlyCounts[h] > 0) {
            double pct = ((double)hourlyCounts[h] / (double)total) * 100.0;
            printf("%02d:00 - %02d:59   %-12d %5.1f%%\n", h, h, hourlyCounts[h], pct);
        }
    }
    printf("-----------------------------------------\n");
    printf("Total Bookings Analyzed: %d\n", total);
}

// Display historical service window performance and workload distribution
void displayWindowPerformance(void)
{
    int rw1 = 0, rw2 = 0, bw1 = 0, bw2 = 0;
    int total = 0;

    FILE *fp = fopen(TICKETS_FILE, "r");
    if (fp) {
        char line[512];
        if (fgets(line, sizeof(line), fp)) {
            while (fgets(line, sizeof(line), fp)) {
                char lineCopy[512];
                strncpy(lineCopy, line, sizeof(lineCopy) - 1);
                lineCopy[sizeof(lineCopy) - 1] = '\0';

                char *fields[20];
                int n = parseCsvRow(lineCopy, fields, 20);
                TicketRecord rec;

                if (parseRecordFromFields(fields, n, &rec)) {
                    if (strcmp(rec.status, "SERVED") == 0) {
                        if (strcmp(rec.serviceWindow, "Railway Window 1") == 0) { rw1++; total++; }
                        else if (strcmp(rec.serviceWindow, "Railway Window 2") == 0) { rw2++; total++; }
                        else if (strcmp(rec.serviceWindow, "Bus Window 1") == 0) { bw1++; total++; }
                        else if (strcmp(rec.serviceWindow, "Bus Window 2") == 0) { bw2++; total++; }
                    }
                }
            }
        }
        fclose(fp);
    }

    printf("\nService Window Performance:\n\n");
    if (total == 0) {
        printf("No served passengers found in records.\n");
        return;
    }

    printf("%-20s %-16s %-12s\n", "Service Window", "Served Count", "Workload %%");
    printf("--------------------------------------------------\n");
    printf("%-20s %-16d %5.1f%%\n", "Railway Window 1", rw1, total > 0 ? (rw1 * 100.0 / total) : 0.0);
    printf("%-20s %-16d %5.1f%%\n", "Railway Window 2", rw2, total > 0 ? (rw2 * 100.0 / total) : 0.0);
    printf("%-20s %-16d %5.1f%%\n", "Bus Window 1",     bw1, total > 0 ? (bw1 * 100.0 / total) : 0.0);
    printf("%-20s %-16d %5.1f%%\n", "Bus Window 2",     bw2, total > 0 ? (bw2 * 100.0 / total) : 0.0);
    printf("--------------------------------------------------\n");
    printf("Total Passengers Served: %d\n", total);
}

// Display all stored ticket records in formatted tabular form
void displayTicketHistory(void)
{
    FILE *fp = fopen(TICKETS_FILE, "r");
    if (!fp) {
        printf("\nNo ticket records found.\n");
        return;
    }

    char line[512];
    if (!fgets(line, sizeof(line), fp)) {
        printf("\nNo ticket records found.\n");
        fclose(fp);
        return;
    }

    int count = 0;
    while (fgets(line, sizeof(line), fp)) {
        char lineCopy[512];
        strncpy(lineCopy, line, sizeof(lineCopy) - 1);
        lineCopy[sizeof(lineCopy) - 1] = '\0';

        char *fields[20];
        int n = parseCsvRow(lineCopy, fields, 20);
        TicketRecord rec;

        if (parseRecordFromFields(fields, n, &rec)) {
            if (count == 0) {
                printf("\n%-7s %-8s %-16s %-12s %-11s %-10s %-9s\n",
                       "Token", "Counter", "Name", "Destination", "Date", "Priority", "Status");
                printf("--------------------------------------------------------------------------------\n");
            }

            printf("T%03d   %-8s %-16s %-12s %-11s %-10s %-9s\n",
                   rec.token,
                   rec.counter,
                   rec.name,
                   rec.destination,
                   rec.bookingDate,
                   getPriorityName(rec.priority),
                   rec.status);
            count++;
        }
    }

    fclose(fp);

    if (count == 0) {
        printf("\nNo ticket records found.\n");
    } else {
        printf("--------------------------------------------------------------------------------\n");
        printf("Total records: %d\n", count);
    }
}
