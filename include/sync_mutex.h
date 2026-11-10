#ifndef SYNC_MUTEX_H
#define SYNC_MUTEX_H

#include "sync.h"

typedef struct {
    SyncSemStorage storage;
} SyncMutexStorage;

typedef struct {
    SyncSem sem;
} SyncMutex;

int  sync_mutex_create(SyncMutex *m, SyncMutexStorage *st, const char *name);
int  sync_mutex_attach(SyncMutex *m, SyncMutexStorage *st, const char *name);
void sync_mutex_destroy(SyncMutex *m);
void sync_mutex_lock(SyncMutex *m);
void sync_mutex_unlock(SyncMutex *m);

#endif