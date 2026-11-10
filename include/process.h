#ifndef PROCESS_H
#define PROCESS_H

#include <sys/types.h>
#include <signal.h>

#define PROCESS_MAX_CHILDREN 64

typedef struct {
    pid_t pids[PROCESS_MAX_CHILDREN];
    int   count;
} ChildPool;

void process_init(ChildPool *p);
int  process_spawn(ChildPool *p, void (*child_main)(void *), void *arg);
void process_wait_all(ChildPool *p);
void process_terminate_all(ChildPool *p);

#endif