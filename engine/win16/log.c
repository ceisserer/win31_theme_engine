/*
 * log.c - ring buffer for diagnostics. Any task may write; only the loader
 * task drains it to RETHEME.LOG (Win16 file handles belong to a task).
 */
#include "rt16.h"
#include <stdarg.h>

#define LOG_SIZE 16384

static char ring[LOG_SIZE];
static UINT head, tail;     /* write / read positions */

void log_init(void)
{
    head = tail = 0;
}

static void put(char c)
{
    UINT next = (head + 1) % LOG_SIZE;
    if (next == tail)
        tail = (tail + 1) % LOG_SIZE;   /* overwrite the oldest byte */
    ring[head] = c;
    head = next;
}

void log_msg(const char FAR *fmt, ...)
{
    char line[160];
    va_list ap;
    int i;
    va_start(ap, fmt);
    wvsprintf(line, fmt, (void FAR *)ap);
    va_end(ap);
    for (i = 0; line[i]; i++)
        put(line[i]);
    put('\r');
    put('\n');
}

int log_drain(char FAR *buf, int size)
{
    int n = 0;
    while (tail != head && n < size) {
        buf[n++] = ring[tail];
        tail = (tail + 1) % LOG_SIZE;
    }
    return n;
}
