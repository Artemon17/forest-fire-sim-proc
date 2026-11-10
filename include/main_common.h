#ifndef MAIN_COMMON_H
#define MAIN_COMMON_H

#include "config.h"
#include "map.h"
#include "shm.h"
#include "model.h"
#include "sync_barrier.h"
#include "sync_mutex.h"

typedef struct {
    SharedState *sh;
    Ctx ctx;

    SyncBarrier barrier_sensors, barrier_teams, barrier_fire;

    SyncSem sensors_go, sensors_done;
    SyncSem teams_go, teams_done;
    SyncSem fire_go, fire_done;
    SyncSem ready, done;

    SyncMutex teams_lock, log_lock;

    int num_sensors, num_teams, num_fire;
    unsigned int seed;
    int standalone;
} FireApp;

int  fire_app_create(FireApp *app, const char *shm_name,
                     const MapData *map, const SharedConfig *cfg,
                     int st_r, int st_c, unsigned int seed, int standalone);
void fire_app_attach(FireApp *app, const char *shm_name);
void fire_app_detach(FireApp *app);
void fire_app_destroy(FireApp *app, const char *shm_name);

int sim_finished(const SharedState *sh);
const char *sim_result_name(int r);
void sim_apply_next_state(SharedState *sh);
void sim_close_pending(SharedState *sh);

int  standalone_run_main(int argc, char **argv);
int  standalone_run_sensors(int argc, char **argv);
int  standalone_run_teams(int argc, char **argv);
int  standalone_run_spread(int argc, char **argv);

#endif