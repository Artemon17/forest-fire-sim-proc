#define _POSIX_C_SOURCE 200809L

#include "model.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

void log_shared_init(SharedState *sh) {
    memset(&sh->log, 0, sizeof(sh->log));
}

void log_shared_add(Ctx *ctx, int tick, const char *fmt, ...) {
    SyncMutex *m = ctx->log_lock;
    EventLogShared *log = &ctx->sh->log;

    if (m) sync_mutex_lock(m);

    LogEvent *e = &log->events[log->head];
    e->tick = tick;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->text, SHM_LOG_MAX_TEXT, fmt, ap);
    va_end(ap);

    log->head = (log->head + 1) % SHM_LOG_MAX_EVENTS;
    log->count++;

    if (m) sync_mutex_unlock(m);
}

int log_shared_last_n(Ctx *ctx, int n, LogEvent *out) {
    SyncMutex *m = ctx->log_lock;
    EventLogShared *log = &ctx->sh->log;

    if (m) sync_mutex_lock(m);

    if (n <= 0) { if (m) sync_mutex_unlock(m); return 0; }

    int have = log->count < SHM_LOG_MAX_EVENTS ? log->count : SHM_LOG_MAX_EVENTS;
    if (n > have) n = have;
    int start = (log->head - n + SHM_LOG_MAX_EVENTS) % SHM_LOG_MAX_EVENTS;

    for (int i = 0; i < n; i++) {
        int idx = (start + i) % SHM_LOG_MAX_EVENTS;
        out[i] = log->events[idx];
    }

    if (m) sync_mutex_unlock(m);
    return n;
}