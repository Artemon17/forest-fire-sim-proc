#define _POSIX_C_SOURCE 200809L

#include "main_common.h"
#include "model.h"

#include <signal.h>
#include <unistd.h>
#include <stdio.h>

static volatile sig_atomic_t g_stop = 0;
static void on_signal(int s) { (void)s; g_stop = 1; }

int main(void) {
    const char *SHM_NAME = "/fire_state_standalone";

    struct sigaction sa = {0};
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    FireApp app;
    fire_app_attach(&app, SHM_NAME);
    if (!app.sh) { fprintf(stderr, "SHM не найдена\n"); return 1; }

    sync_sem_post(&app.ready);

    int total = app.sh->rows * app.sh->cols;
    int W = app.num_fire;
    int slice = (total + W - 1) / W;

    unsigned int rng[8];
    for (int i = 0; i < W; i++) rng[i] = app.seed ^ (0xA5A5A5A5u * (unsigned)(i + 1));

    while (1) {
        for (int w = 0; w < W; w++) {
            sync_sem_wait(&app.fire_go);
            if (atomic_load(&app.sh->stop) || g_stop) {
                sync_sem_post(&app.fire_done);
                continue;
            }
            int lo = w * slice;
            int hi = lo + slice;
            if (hi > total) hi = total;
            if (lo < total) fire_spread_worker(&app.ctx, lo, hi, &rng[w]);
            sync_sem_post(&app.fire_done);
        }
        if (atomic_load(&app.sh->stop) || g_stop) break;
    }

    sync_sem_post(&app.done);
    fire_app_detach(&app);
    return 0;
}