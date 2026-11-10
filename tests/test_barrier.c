#define _POSIX_C_SOURCE 200809L

#include "shm.h"
#include "sync_barrier.h"
#include "process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <unistd.h>

#define N_CHILDREN 4

typedef struct {
    SyncBarrierStorage barrier;
    atomic_int         ready;
    atomic_int         failures;
} TestShm;

static void child_main(void *arg) {
    TestShm *sh = (TestShm *)arg;

    SyncBarrier b;
    if (sync_barrier_attach(&b, &sh->barrier, "/fire_test_bar") != 0) _exit(1);

    atomic_fetch_add(&sh->ready, 1);
    sync_barrier_wait(&b);

    if (atomic_load(&sh->ready) != N_CHILDREN)
        atomic_fetch_add(&sh->failures, 1);

    sync_barrier_destroy(&b);
    _exit(0);
}

int main(void) {
    const char *shm_name = "/fire_test_bar_shm";

    TestShm *sh = shm_create_sized(shm_name, sizeof(TestShm));
    if (!sh) return 1;

    atomic_init(&sh->ready, 0);
    atomic_init(&sh->failures, 0);

    SyncBarrier b;
    if (sync_barrier_create(&b, &sh->barrier, "/fire_test_bar", N_CHILDREN) != 0)
        return 1;

    ChildPool pool;
    process_init(&pool);
    for (int i = 0; i < N_CHILDREN; i++)
        process_spawn(&pool, child_main, sh);

    process_wait_all(&pool);

    int fail = atomic_load(&sh->failures);
    printf("ready=%d failures=%d\n", atomic_load(&sh->ready), fail);

    sync_barrier_destroy(&b);
    shm_detach_sized(sh, sizeof(TestShm));
    shm_destroy(shm_name);

    if (fail == 0) { printf("OK: barrier works\n"); return 0; }
    fprintf(stderr, "FAIL\n");
    return 1;
}