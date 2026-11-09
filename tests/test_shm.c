#define _POSIX_C_SOURCE 200809L

#include "shm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    const char *name = "/fire_test";

    SharedState *sh = shm_create(name);
    if (!sh) return 1;

    sh->rows = 5;
    sh->cols = 7;
    sh->tick = 0;
    sh->st_r = 2;
    sh->st_c = 3;

    printf("parent: created SHM, pid=%d\n", getpid());

    pid_t pid = fork();
    if (pid == 0) {
        SharedState *child = shm_attach(name);
        if (!child) _exit(2);

        printf("child: attached, pid=%d, tick=%d\n", getpid(), child->tick);

        child->tick = 42;
        child->state[0] = ST_BURNING;
        child->teams_count = 3;

        shm_detach(child);
        _exit(0);
    }

    int status;
    waitpid(pid, &status, 0);

    printf("parent: after child exit\n");
    printf("  rows=%d cols=%d tick=%d\n",
           sh->rows, sh->cols, sh->tick);
    printf("  state[0]=%d teams_count=%d\n",
           sh->state[0], sh->teams_count);

    int ok = (sh->tick == 42
              && sh->state[0] == ST_BURNING
              && sh->teams_count == 3
              && sh->rows == 5);

    shm_detach(sh);
    shm_destroy(name);

    if (!ok) {
        fprintf(stderr, "FAIL: данные не видны между процессами\n");
        return 1;
    }
    printf("OK: SHM works\n");
    return 0;
}