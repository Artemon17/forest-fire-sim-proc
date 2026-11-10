#ifndef SYNC_H
#define SYNC_H

#include <semaphore.h>

typedef struct {
    sem_t storage;
} SyncSemStorage;

typedef struct {
    sem_t *sem;
    int    owner;
    char   name[64];
} SyncSem;

int  sync_sem_create(SyncSem *s, SyncSemStorage *storage, const char *name, unsigned value);
int  sync_sem_attach(SyncSem *s, SyncSemStorage *storage, const char *name);
void sync_sem_close(SyncSem *s);
void sync_sem_wait(SyncSem *s);
void sync_sem_post(SyncSem *s);

#endif