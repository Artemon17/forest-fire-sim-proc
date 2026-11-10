#define _POSIX_C_SOURCE 200809L

#include "sync.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int sync_sem_create(SyncSem *s, SyncSemStorage *storage, const char *name, unsigned value) {
    memset(s, 0, sizeof(*s));

#ifdef USE_UNNAMED
    (void)name;
    if (sem_init(&storage->storage, 1, value) != 0) {
        perror("sem_init");
        return -1;
    }
    s->sem = &storage->storage;
    s->owner = 0;
#else
    (void)storage;
    sem_unlink(name);
    sem_t *p = sem_open(name, O_CREAT | O_EXCL, 0600, value);
    if (p == SEM_FAILED) {
        perror("sem_open");
        return -1;
    }
    s->sem = p;
    s->owner = 1;
    strncpy(s->name, name, sizeof(s->name) - 1);
#endif
    return 0;
}

int sync_sem_attach(SyncSem *s, SyncSemStorage *storage, const char *name) {
    memset(s, 0, sizeof(*s));

#ifdef USE_UNNAMED
    (void)name;
    s->sem = &storage->storage;
    s->owner = 0;
#else
    (void)storage;
    sem_t *p = sem_open(name, 0);
    if (p == SEM_FAILED) {
        perror("sem_open");
        return -1;
    }
    s->sem = p;
    s->owner = 0;
    strncpy(s->name, name, sizeof(s->name) - 1);
#endif
    return 0;
}

void sync_sem_close(SyncSem *s) {
#ifdef USE_UNNAMED
    (void)s;
#else
    if (s->sem) {
        sem_close(s->sem);
        if (s->owner) sem_unlink(s->name);
        s->sem = NULL;
        s->owner = 0;
    }
#endif
}

void sync_sem_wait(SyncSem *s) {
    while (sem_wait(s->sem) != 0 && errno == EINTR) { }
}

void sync_sem_post(SyncSem *s) {
    sem_post(s->sem);
}