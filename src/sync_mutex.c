#define _POSIX_C_SOURCE 200809L

#include "sync_mutex.h"

int sync_mutex_create(SyncMutex *m, SyncMutexStorage *st, const char *name) {
    return sync_sem_create(&m->sem, &st->storage, name, 1);
}

int sync_mutex_attach(SyncMutex *m, SyncMutexStorage *st, const char *name) {
    return sync_sem_attach(&m->sem, &st->storage, name);
}

void sync_mutex_destroy(SyncMutex *m) {
    sync_sem_close(&m->sem);
}

void sync_mutex_lock(SyncMutex *m)   { sync_sem_wait(&m->sem); }
void sync_mutex_unlock(SyncMutex *m) { sync_sem_post(&m->sem); }