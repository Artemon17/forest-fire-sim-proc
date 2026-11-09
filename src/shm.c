#define _POSIX_C_SOURCE 200809L

#include "shm.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

SharedState *shm_create(const char *name) {
    shm_unlink(name);

    int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) {
        fprintf(stderr, "shm_create: shm_open(%s): %s\n", name, strerror(errno));
        return NULL;
    }

    if (ftruncate(fd, sizeof(SharedState)) < 0) {
        fprintf(stderr, "shm_create: ftruncate: %s\n", strerror(errno));
        close(fd);
        shm_unlink(name);
        return NULL;
    }

    void *p = mmap(NULL, sizeof(SharedState),
                   PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    if (p == MAP_FAILED) {
        fprintf(stderr, "shm_create: mmap: %s\n", strerror(errno));
        shm_unlink(name);
        return NULL;
    }

    SharedState *sh = (SharedState *)p;
    memset(sh, 0, sizeof(*sh));
    return sh;
}

SharedState *shm_attach(const char *name) {
    int fd = shm_open(name, O_RDWR, 0);
    if (fd < 0) {
        fprintf(stderr, "shm_attach: shm_open(%s): %s\n", name, strerror(errno));
        return NULL;
    }

    void *p = mmap(NULL, sizeof(SharedState),
                   PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    if (p == MAP_FAILED) {
        fprintf(stderr, "shm_attach: mmap: %s\n", strerror(errno));
        return NULL;
    }
    return (SharedState *)p;
}

void shm_detach(SharedState *sh) {
    if (sh) munmap(sh, sizeof(SharedState));
}

void shm_destroy(const char *name) {
    shm_unlink(name);
}