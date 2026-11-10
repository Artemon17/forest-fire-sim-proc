#ifndef SYNC_BARRIER_H
#define SYNC_BARRIER_H

#include "sync.h"

typedef struct {
    int count;
    int total;
} SyncBarrierShared;

typedef struct {
    SyncBarrierShared shared;
    SyncSemStorage    mutex_storage;
    SyncSemStorage    release_storage;
} SyncBarrierStorage;

typedef struct {
    SyncBarrierShared *shared;
    SyncSem            mutex;
    SyncSem            release;
} SyncBarrier;

int  sync_barrier_create(SyncBarrier *b, SyncBarrierStorage *st,
                        const char *prefix, int total);
int  sync_barrier_attach(SyncBarrier *b, SyncBarrierStorage *st,
                        const char *prefix);
void sync_barrier_destroy(SyncBarrier *b);
void sync_barrier_wait(SyncBarrier *b);

#endif