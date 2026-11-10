#define _POSIX_C_SOURCE 200809L

#include "shm.h"
#include "sync_mutex.h"
#include "process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define N_CHILDREN 4
#define N_ITERS    10000

typedef struct {
    SyncMutexStorage mutex;
    int              counter;
} TestShm;

static void child_main(void *arg) {
    TestShm *sh = (TestShm *)arg;

    SyncMutex m;
    if (sync_mutex_attach(&m, &sh->mutex, "/fire_test_mtx") != 0) _exit(1);

    for (int i = 0; i < N_ITERS; i++) {
        sync_mutex_lock(&m);
        sh->counter++;
        sync_mutex_unlock(&m);
    }

    sync_mutex_destroy(&m);
    _exit(0);
}

int main(void) {
    const char *shm_name = "/fire_test_mtx_shm";

    TestShm *sh = shm_create_sized(shm_name, sizeof(TestShm));
    if (!sh) return 1;
    sh->counter = 0;

    SyncMutex m;
    if (sync_mutex_create(&m, &sh->mutex, "/fire_test_mtx") != 0) return 1;

    ChildPool pool;
    process_init(&pool);
    for (int i = 0; i < N_CHILDREN; i++)
        process_spawn(&pool, child_main, sh);

    process_wait_all(&pool);

    int expected = N_CHILDREN * N_ITERS;
    int actual = sh->counter;
    printf("counter=%d expected=%d\n", actual, expected);

    sync_mutex_destroy(&m);
    shm_detach_sized(sh, sizeof(TestShm));
    shm_destroy(shm_name);

    if (actual == expected) { printf("OK: mutex works\n"); return 0; }
    fprintf(stderr, "FAIL\n");
    return 1;
}