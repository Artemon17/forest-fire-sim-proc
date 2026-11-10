#define _POSIX_C_SOURCE 200809L

#include "sync_barrier.h"

#include <stdio.h>

#ifndef USE_UNNAMED
static void make_name(char *out, size_t sz, const char *prefix, const char *suffix) {
    snprintf(out, sz, "%s_%s", prefix, suffix);
}
#endif

int sync_barrier_create(SyncBarrier *b, SyncBarrierStorage *st,
                        const char *prefix, int total) {
    st->shared.count = 0;
    st->shared.total = total;
    b->shared = &st->shared;

#ifdef USE_UNNAMED
    (void)prefix;
    if (sync_sem_create(&b->mutex,   &st->mutex_storage,   "", 1) != 0) return -1;
    if (sync_sem_create(&b->release, &st->release_storage, "", 0) != 0) return -1;
#else
    char n[80];
    make_name(n, sizeof(n), prefix, "mtx");
    if (sync_sem_create(&b->mutex,   &st->mutex_storage,   n, 1) != 0) return -1;
    make_name(n, sizeof(n), prefix, "rel");
    if (sync_sem_create(&b->release, &st->release_storage, n, 0) != 0) return -1;
#endif
    return 0;
}

int sync_barrier_attach(SyncBarrier *b, SyncBarrierStorage *st,
                        const char *prefix) {
    b->shared = &st->shared;

#ifdef USE_UNNAMED
    (void)prefix;
    if (sync_sem_attach(&b->mutex,   &st->mutex_storage,   "") != 0) return -1;
    if (sync_sem_attach(&b->release, &st->release_storage, "") != 0) return -1;
#else
    char n[80];
    make_name(n, sizeof(n), prefix, "mtx");
    if (sync_sem_attach(&b->mutex,   &st->mutex_storage,   n) != 0) return -1;
    make_name(n, sizeof(n), prefix, "rel");
    if (sync_sem_attach(&b->release, &st->release_storage, n) != 0) return -1;
#endif
    return 0;
}

void sync_barrier_destroy(SyncBarrier *b) {
    sync_sem_close(&b->mutex);
    sync_sem_close(&b->release);
}

void sync_barrier_wait(SyncBarrier *b) {
    sync_sem_wait(&b->mutex);
    b->shared->count++;
    if (b->shared->count == b->shared->total) {
        b->shared->count = 0;
        sync_sem_post(&b->mutex);
        for (int i = 0; i < b->shared->total - 1; i++)
            sync_sem_post(&b->release);
    } else {
        sync_sem_post(&b->mutex);
        sync_sem_wait(&b->release);
    }
}