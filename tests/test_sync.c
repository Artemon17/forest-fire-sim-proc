#define _POSIX_C_SOURCE 200809L

#include "shm.h"
#include "sync.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

typedef struct {
    SyncSemStorage storage;
} TestShm;

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static void sleep_ms(long ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) != 0) { }
}

int main(void) {
    const char *shm_name = "/fire_test_sync";
    const char *sem_name = "/fire_test_sync_sem";
    (void)sem_name;

    TestShm *sh = shm_create_sized(shm_name, sizeof(TestShm));
    if (!sh) return 1;

    SyncSem sem;

#ifdef USE_UNNAMED
    if (sync_sem_create(&sem, &sh->storage, NULL, 0) != 0) return 1;
    printf("mode: UNNAMED\n");
#else
    if (sync_sem_create(&sem, NULL, sem_name, 0) != 0) return 1;
    printf("mode: NAMED\n");
#endif

    double t0 = now_ms();

    pid_t pid = fork();
    if (pid == 0) {
        sleep_ms(500);
        SyncSem c;
#ifdef USE_UNNAMED
        sync_sem_attach(&c, &sh->storage, NULL);
#else
        sync_sem_attach(&c, NULL, sem_name);
#endif
        sync_sem_post(&c);
        sync_sem_close(&c);
        _exit(0);
    }

    int status;
    sync_sem_wait(&sem);
    double t1 = now_ms();
    waitpid(pid, &status, 0);

    printf("waited %.1f ms\n", t1 - t0);

    int ok = (t1 - t0) >= 400.0;

    sync_sem_close(&sem);
    shm_detach_sized(sh, sizeof(TestShm));
    shm_destroy(shm_name);

    if (ok) { printf("OK: sync works\n"); return 0; }
    fprintf(stderr, "FAIL\n");
    return 1;
}