#define _POSIX_C_SOURCE 200809L

#include "main_common.h"
#include "map.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

enum {
    RESULT_RUNNING = 0,
    RESULT_EXTINGUISHED,
    RESULT_TERRITORY_EXHAUSTED,
    RESULT_TICK_LIMIT,
    RESULT_INTERRUPTED
};

int sim_finished(const SharedState *sh) {
    int burning = 0, flammable_safe = 0;
    int total = sh->rows * sh->cols;
    for (int i = 0; i < total; i++) {
        if (sh->state[i] == ST_BURNING) burning++;
        Terrain t = sh->cells[i];
        if (t != TERRAIN_WATER && t != TERRAIN_FIREBREAK) {
            if (sh->state[i] == ST_SAFE || sh->state[i] == ST_EXTINGUISHED)
                flammable_safe++;
        }
    }
    if (burning == 0) return RESULT_EXTINGUISHED;
    if (flammable_safe == 0) return RESULT_TERRITORY_EXHAUSTED;
    if (!sh->cfg.end_unlimited && sh->tick >= sh->cfg.end_max_ticks)
        return RESULT_TICK_LIMIT;
    return RESULT_RUNNING;
}

const char *sim_result_name(int r) {
    switch (r) {
        case RESULT_RUNNING:             return "RUNNING";
        case RESULT_EXTINGUISHED:        return "EXTINGUISHED";
        case RESULT_TERRITORY_EXHAUSTED: return "TERRITORY_EXHAUSTED";
        case RESULT_TICK_LIMIT:          return "TICK_LIMIT";
        case RESULT_INTERRUPTED:         return "INTERRUPTED";
    }
    return "?";
}

void sim_apply_next_state(SharedState *sh) {
    int total = sh->rows * sh->cols;
    for (int i = 0; i < total; i++) {
        FireState prev = sh->state[i];
        FireState nx = atomic_load(&sh->next_state[i]);
        if (prev == ST_BURNING && nx == ST_BURNT) sh->burn_age[i] = -1;
        else if (prev == ST_BURNING) sh->burn_age[i]++;
        else if (prev != ST_BURNING && nx == ST_BURNING) sh->burn_age[i] = 0;
        sh->state[i] = nx;
    }
}

void sim_close_pending(SharedState *sh) {
    for (int i = 0; i < sh->tasks_count; i++) {
        Task *t = &sh->tasks[i];
        if (t->state == TASK_PENDING || t->state == TASK_ASSIGNED ||
            t->state == TASK_IN_PROGRESS) {
            t->state = TASK_CANCELLED;
            t->completed_tick = sh->tick;
        }
    }
    for (int i = 0; i < sh->teams_count; i++) {
        sh->teams[i].target_task_id = 0;
        sh->teams[i].target_r = -1;
        sh->teams[i].target_c = -1;
        if (sh->teams[i].state != TEAM_IDLE) sh->teams[i].state = TEAM_IDLE;
    }
}

int fire_app_create(FireApp *app, const char *shm_name,
                    const MapData *map, const SharedConfig *cfg,
                    int st_r, int st_c, unsigned int seed, int standalone)
{
    memset(app, 0, sizeof(*app));
    app->standalone = standalone;

    if (map->rows > SHM_MAX_ROWS || map->cols > SHM_MAX_COLS) {
        fprintf(stderr, "Карта больше лимита SHM\n");
        return -1;
    }

    app->sh = shm_create(shm_name);
    if (!app->sh) return -1;

    SharedState *sh = app->sh;
    sh->rows = map->rows;
    sh->cols = map->cols;
    sh->st_r = st_r;
    sh->st_c = st_c;
    sh->seed = seed;
    sh->cfg  = *cfg;
    sh->standalone = standalone;

    int total = sh->rows * sh->cols;
    for (int i = 0; i < total; i++) {
        sh->cells[i] = map->cells[i];
        sh->state[i] = ST_SAFE;
        sh->occupant[i] = OCC_NONE;
        sh->burn_age[i] = -1;
        atomic_store(&sh->next_state[i], ST_SAFE);
    }

    sh->occupant[st_r * sh->cols + st_c] = OCC_STATION;

    int map_sensors = map->sensors_count;
    if (map_sensors > SHM_MAX_SENSORS - 1) map_sensors = SHM_MAX_SENSORS - 1;
    for (int i = 0; i < map_sensors; i++) {
        sh->sensors[i].id = i + 1;
        sh->sensors[i].r = map->sensors[i].r;
        sh->sensors[i].c = map->sensors[i].c;
        sh->occupant[map->sensors[i].r * sh->cols + map->sensors[i].c] = OCC_SENSOR;
    }
    sh->sensors[map_sensors].id = map_sensors + 1;
    sh->sensors[map_sensors].r = st_r;
    sh->sensors[map_sensors].c = st_c;
    sh->sensors_count = map_sensors + 1;

    int teams = cfg->team_count > SHM_MAX_TEAMS ? SHM_MAX_TEAMS : cfg->team_count;
    for (int i = 0; i < teams; i++) {
        sh->teams[i].id = i + 1;
        sh->teams[i].r = st_r;
        sh->teams[i].c = st_c;
        sh->teams[i].state = TEAM_IDLE;
        sh->teams[i].target_r = -1;
        sh->teams[i].target_c = -1;
        sh->teams[i].target_task_id = 0;
        sh->teams[i].progress = 0.0;
        sh->teams[i].last_report_r = -1;
        sh->teams[i].last_report_c = -1;
    }
    sh->teams_count = teams;

    for (int i = 0; i < map->ignition_count; i++) {
        int idx = map->ignition[i].r * sh->cols + map->ignition[i].c;
        sh->state[idx] = ST_BURNING;
        sh->burn_age[idx] = 0;
    }

    sh->next_message_id = 1;
    sh->next_task_id = 1;
    sh->tick = 0;
    atomic_store(&sh->stop, 0);

    log_shared_init(sh);

    app->num_sensors = sh->sensors_count;
    app->num_teams   = sh->teams_count;
    app->num_fire    = 8;
    app->seed        = seed;

    if (standalone) {
        sync_sem_create(&app->sensors_go,   &sh->sensors_go_storage,   "/fire_s_go",   0);
        sync_sem_create(&app->sensors_done, &sh->sensors_done_storage, "/fire_s_done", 0);
        sync_sem_create(&app->teams_go,     &sh->teams_go_storage,     "/fire_t_go",   0);
        sync_sem_create(&app->teams_done,   &sh->teams_done_storage,   "/fire_t_done", 0);
        sync_sem_create(&app->fire_go,      &sh->fire_go_storage,      "/fire_f_go",   0);
        sync_sem_create(&app->fire_done,    &sh->fire_done_storage,    "/fire_f_done", 0);
        sync_sem_create(&app->ready,        &sh->ready_storage,        "/fire_ready",  0);
        sync_sem_create(&app->done,         &sh->done_storage,         "/fire_done",   0);
    } else {
        sync_barrier_create(&app->barrier_sensors, &sh->barrier_sensors, "/fire_bs", app->num_sensors + 1);
        sync_barrier_create(&app->barrier_teams,   &sh->barrier_teams,   "/fire_bt", app->num_teams + 1);
        sync_barrier_create(&app->barrier_fire,    &sh->barrier_fire,    "/fire_bf", app->num_fire + 1);
    }

    sync_mutex_create(&app->teams_lock, &sh->teams_lock_storage, "/fire_teams_mtx");
    sync_mutex_create(&app->log_lock,   &sh->log_lock_storage,   "/fire_log_mtx");

    app->ctx.sh = sh;
    app->ctx.teams_lock = &app->teams_lock;
    app->ctx.log_lock   = &app->log_lock;

    atomic_store(&sh->init_done, 1);
    return 0;
}

void fire_app_attach(FireApp *app, const char *shm_name) {
    memset(app, 0, sizeof(*app));

    SharedState *sh = NULL;
    for (int attempt = 0; attempt < 500 && !sh; attempt++) {
        sh = shm_attach(shm_name);
        if (!sh) {
            struct timespec ts = {0, 20 * 1000000L};
            nanosleep(&ts, NULL);
        }
    }
    if (!sh) return;

    for (int attempt = 0; attempt < 500 && !atomic_load(&sh->init_done); attempt++) {
        struct timespec ts = {0, 10 * 1000000L};
        nanosleep(&ts, NULL);
    }
    if (!atomic_load(&sh->init_done)) return;

    app->sh = sh;
    app->standalone = sh->standalone;

    if (sh->standalone) {
        sync_sem_attach(&app->sensors_go,   &sh->sensors_go_storage,   "/fire_s_go");
        sync_sem_attach(&app->sensors_done, &sh->sensors_done_storage, "/fire_s_done");
        sync_sem_attach(&app->teams_go,     &sh->teams_go_storage,     "/fire_t_go");
        sync_sem_attach(&app->teams_done,   &sh->teams_done_storage,   "/fire_t_done");
        sync_sem_attach(&app->fire_go,      &sh->fire_go_storage,      "/fire_f_go");
        sync_sem_attach(&app->fire_done,    &sh->fire_done_storage,    "/fire_f_done");
        sync_sem_attach(&app->ready,        &sh->ready_storage,        "/fire_ready");
        sync_sem_attach(&app->done,         &sh->done_storage,         "/fire_done");
    } else {
        sync_barrier_attach(&app->barrier_sensors, &sh->barrier_sensors, "/fire_bs");
        sync_barrier_attach(&app->barrier_teams,   &sh->barrier_teams,   "/fire_bt");
        sync_barrier_attach(&app->barrier_fire,    &sh->barrier_fire,    "/fire_bf");
    }

    sync_mutex_attach(&app->teams_lock, &sh->teams_lock_storage, "/fire_teams_mtx");
    sync_mutex_attach(&app->log_lock,   &sh->log_lock_storage,   "/fire_log_mtx");

    app->num_sensors = sh->sensors_count;
    app->num_teams   = sh->teams_count;
    app->num_fire    = 8;
    app->seed        = sh->seed;

    app->ctx.sh = sh;
    app->ctx.teams_lock = &app->teams_lock;
    app->ctx.log_lock   = &app->log_lock;
}

void fire_app_detach(FireApp *app) {
    if (app->standalone) {
        sync_sem_close(&app->sensors_go);
        sync_sem_close(&app->sensors_done);
        sync_sem_close(&app->teams_go);
        sync_sem_close(&app->teams_done);
        sync_sem_close(&app->fire_go);
        sync_sem_close(&app->fire_done);
        sync_sem_close(&app->ready);
        sync_sem_close(&app->done);
    } else {
        sync_barrier_destroy(&app->barrier_sensors);
        sync_barrier_destroy(&app->barrier_teams);
        sync_barrier_destroy(&app->barrier_fire);
    }
    sync_mutex_destroy(&app->teams_lock);
    sync_mutex_destroy(&app->log_lock);

    if (app->sh) { shm_detach(app->sh); app->sh = NULL; }
}

void fire_app_destroy(FireApp *app, const char *shm_name) {
    fire_app_detach(app);
    shm_destroy(shm_name);
}