#ifndef LOG_H
#define LOG_H

#include <pthread.h>

#define LOG_MAX_EVENTS  512
#define LOG_MAX_TEXT    160

typedef struct {
    int  tick;
    char text[LOG_MAX_TEXT];
} LogEvent;

typedef struct {
    LogEvent        events[LOG_MAX_EVENTS];
    int             head;
    int             count;
    pthread_mutex_t lock;
} EventLog;

void log_init(EventLog *log);
void log_destroy(EventLog *log);
void log_add(EventLog *log, int tick, const char *fmt, ...);
int  log_last_n(const EventLog *log, int n, LogEvent *out);

#endif