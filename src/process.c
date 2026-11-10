#define _POSIX_C_SOURCE 200809L

#include "process.h"

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

void process_init(ChildPool *p) {
    p->count = 0;
}

int process_spawn(ChildPool *p, void (*child_main)(void *), void *arg) {
    if (p->count >= PROCESS_MAX_CHILDREN) return -1;
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        signal(SIGINT, SIG_IGN);
        signal(SIGTERM, SIG_DFL);
        if (child_main) child_main(arg);
        _exit(0);
    }
    p->pids[p->count++] = pid;
    return 0;
}

void process_wait_all(ChildPool *p) {
    for (int i = 0; i < p->count; i++) {
        int status;
        while (waitpid(p->pids[i], &status, 0) < 0 && errno == EINTR) { }
    }
    p->count = 0;
}

void process_terminate_all(ChildPool *p) {
    for (int i = 0; i < p->count; i++)
        kill(p->pids[i], SIGTERM);
}