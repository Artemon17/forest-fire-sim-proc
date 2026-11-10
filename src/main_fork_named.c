#define _POSIX_C_SOURCE 200809L

#include "main_common.h"
#include "config.h"
#include "map.h"
#include "term.h"
#include "model.h"
#include "process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

static const char *SHM_NAME = "/fire_state_proc";
static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) { (void)sig; g_stop = 1; }

typedef struct {
    const char *shm_name;
    int         index;
    unsigned int seed_base;
} WorkerArg;

static void sensor_proc(void *arg) {
    WorkerArg *a = (WorkerArg *)arg;
    FireApp app;
    fire_app_attach(&app, a->shm_name);
    if (!app.sh) _exit(2);

    unsigned int rng = a->seed_base ^ (0x9E3779B9u * (unsigned)(a->index + 1));

    while (1) {
        sync_barrier_wait(&app.barrier_sensors);
        if (atomic_load(&app.sh->stop)) break;
        sensors_observe_one(&app.ctx, a->index, &rng);
        sync_barrier_wait(&app.barrier_sensors);
    }
    _exit(0);
}

static void team_proc(void *arg) {
    WorkerArg *a = (WorkerArg *)arg;
    FireApp app;
    fire_app_attach(&app, a->shm_name);
    if (!app.sh) _exit(2);

    while (1) {
        sync_barrier_wait(&app.barrier_teams);
        if (atomic_load(&app.sh->stop)) break;

        sync_mutex_lock(&app.teams_lock);
        teams_move_one(&app.ctx, a->index);
        teams_extinguish_one(&app.ctx, a->index);
        sync_mutex_unlock(&app.teams_lock);

        sync_barrier_wait(&app.barrier_teams);
    }
    _exit(0);
}

static void fire_proc(void *arg) {
    WorkerArg *a = (WorkerArg *)arg;
    FireApp app;
    fire_app_attach(&app, a->shm_name);
    if (!app.sh) _exit(2);

    int total = app.sh->rows * app.sh->cols;
    int W = app.num_fire;
    int slice = (total + W - 1) / W;
    int lo = a->index * slice;
    int hi = lo + slice;
    if (hi > total) hi = total;

    unsigned int rng = a->seed_base ^ (0xA5A5A5A5u * (unsigned)(a->index + 100));

    while (1) {
        sync_barrier_wait(&app.barrier_fire);
        if (atomic_load(&app.sh->stop)) break;
        fire_spread_worker(&app.ctx, lo, hi, &rng);
        sync_barrier_wait(&app.barrier_fire);
    }
    _exit(0);
}

static int parse_args(int argc, char **argv,
                      const char **map_path, const char **cfg_path,
                      int *st_r, int *st_c, int *st_given,
                      unsigned int *seed, long *delay_ms, int *quiet)
{
    *map_path = NULL;
    *cfg_path = "data/conditions.conf";
    *st_given = 0;
    *st_r = 0; *st_c = 0;
    *seed = (unsigned)time(NULL);
    *delay_ms = 300;
    *quiet = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--map") && i + 1 < argc)
            *map_path = argv[++i];
        else if (!strcmp(argv[i], "--conditions") && i + 1 < argc)
            *cfg_path = argv[++i];
        else if (!strcmp(argv[i], "--station") && i + 2 < argc) {
            *st_r = atoi(argv[++i]);
            *st_c = atoi(argv[++i]);
            *st_given = 1;
        }
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc)
            *seed = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--delay") && i + 1 < argc)
            *delay_ms = strtol(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--quiet"))
            *quiet = 1;
        else {
            fprintf(stderr, "Неизвестный аргумент: %s\n", argv[i]);
            return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *map_path, *cfg_path;
    int st_r, st_c, st_given, quiet;
    unsigned int seed;
    long delay_ms;

    if (parse_args(argc, argv, &map_path, &cfg_path,
                   &st_r, &st_c, &st_given, &seed, &delay_ms, &quiet) != 0)
        return 2;

    if (!map_path) {
        fprintf(stderr, "Укажите --map\n");
        return 2;
    }

    Config cfg;
    if (config_load(cfg_path, &cfg) != 0) return 3;

    MapData map;
    if (map_load(map_path, &map) != 0) return 4;

    if (!st_given && map.station_candidates_count > 0) {
        st_r = map.station_candidates[0].r;
        st_c = map.station_candidates[0].c;
    }
    if (st_r < 0 || st_r >= map.rows || st_c < 0 || st_c >= map.cols) {
        fprintf(stderr, "Станция вне карты\n");
        return 5;
    }

    SharedConfig scfg;
    memset(&scfg, 0, sizeof(scfg));
    scfg.fire_base_prob_grass     = cfg.fire_base_prob_grass;
    scfg.fire_base_prob_tree      = cfg.fire_base_prob_tree;
    scfg.fire_burn_time_factor    = cfg.fire_burn_time_factor;
    scfg.fire_wind_factor_along   = cfg.fire_wind_factor_along;
    scfg.fire_wind_factor_against = cfg.fire_wind_factor_against;
    scfg.fire_wind_factor_side    = cfg.fire_wind_factor_side;
    scfg.fire_spread_8dir         = cfg.fire_spread_8dir;
    scfg.fire_reburn_burnt        = cfg.fire_reburn_burnt;
    scfg.wind_direction           = (int)cfg.wind_direction;
    scfg.wind_power               = cfg.wind_power;
    scfg.sensor_radius            = cfg.sensor_radius;
    scfg.sensor_resend_period     = cfg.sensor_resend_period;
    scfg.msg_delay_min            = cfg.msg_delay_min;
    scfg.msg_delay_max            = cfg.msg_delay_max;
    scfg.msg_loss_prob            = cfg.msg_loss_prob;
    scfg.msg_dup_prob             = cfg.msg_dup_prob;
    scfg.team_count               = cfg.team_count;
    scfg.team_speed               = cfg.team_speed;
    scfg.team_extinguish_time     = cfg.team_extinguish_time;
    scfg.team_efficiency          = cfg.team_efficiency;
    scfg.team_return_to_station   = cfg.team_return_to_station;
    scfg.team_can_cross_water     = cfg.team_can_cross_water;
    scfg.team_can_cross_firebreak = cfg.team_can_cross_firebreak;
    scfg.end_max_ticks            = cfg.end_max_ticks;
    scfg.end_unlimited            = cfg.end_unlimited;

    FireApp app;
    if (fire_app_create(&app, SHM_NAME, &map, &scfg, st_r, st_c, seed, 0) != 0) {
        fprintf(stderr, "fire_app_create failed\n");
        return 5;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    ChildPool pool;
    process_init(&pool);

    WorkerArg *args_sensor = calloc((size_t)app.num_sensors, sizeof(WorkerArg));
    WorkerArg *args_team   = calloc((size_t)app.num_teams,   sizeof(WorkerArg));
    WorkerArg *args_fire   = calloc((size_t)app.num_fire,    sizeof(WorkerArg));

    for (int i = 0; i < app.num_sensors; i++) {
        args_sensor[i].shm_name  = SHM_NAME;
        args_sensor[i].index     = i;
        args_sensor[i].seed_base = seed;
        process_spawn(&pool, sensor_proc, &args_sensor[i]);
    }
    for (int i = 0; i < app.num_teams; i++) {
        args_team[i].shm_name  = SHM_NAME;
        args_team[i].index     = i;
        args_team[i].seed_base = seed;
        process_spawn(&pool, team_proc, &args_team[i]);
    }
    for (int i = 0; i < app.num_fire; i++) {
        args_fire[i].shm_name  = SHM_NAME;
        args_fire[i].index     = i;
        args_fire[i].seed_base = seed;
        process_spawn(&pool, fire_proc, &args_fire[i]);
    }

    if (!quiet) {
        term_install_signals();
        term_alt_screen_enter();
        if (term_raw_mode() == 0) term_hide_cursor();
        term_clear();
    }

    if (!quiet) render_frame(&app.ctx);
    if (!quiet && delay_ms > 0) term_sleep_ms(delay_ms);

    int result = sim_finished(app.sh);
    while (result == 0 && !g_stop && !term_should_stop()) {
        app.sh->tick++;

        sync_barrier_wait(&app.barrier_sensors);
        sync_barrier_wait(&app.barrier_sensors);

        teams_observe(&app.ctx, &app.seed);

        messages_tick(&app.ctx);

        sync_mutex_lock(&app.teams_lock);
        center_tick(&app.ctx);
        sync_mutex_unlock(&app.teams_lock);

        sync_barrier_wait(&app.barrier_teams);
        sync_barrier_wait(&app.barrier_teams);

        int total = app.sh->rows * app.sh->cols;
        for (int i = 0; i < total; i++)
            atomic_store(&app.sh->next_state[i], app.sh->state[i]);

        sync_barrier_wait(&app.barrier_fire);
        sync_barrier_wait(&app.barrier_fire);

        sim_apply_next_state(app.sh);

        if (!quiet) {
            render_frame(&app.ctx);
            if (delay_ms > 0) term_sleep_ms(delay_ms);
            int k = term_poll_key();
            if (k == 'q') { result = 4; break; }
        }

        result = sim_finished(app.sh);
    }

    if (g_stop || term_should_stop()) result = 4;

    atomic_store(&app.sh->stop, 1);

    if (pool.count > 0) {
        for (int i = 0; i < app.num_sensors; i++) sync_sem_post(&app.barrier_sensors.release);
        for (int i = 0; i < app.num_teams;   i++) sync_sem_post(&app.barrier_teams.release);
        for (int i = 0; i < app.num_fire;    i++) sync_sem_post(&app.barrier_fire.release);
    }

    process_terminate_all(&pool);
    process_wait_all(&pool);

    if (!quiet) {
        term_reset_color();
        term_show_cursor();
        term_restore_mode();
        term_alt_screen_leave();
        term_clear();
        render_frame(&app.ctx);
        printf("\n");
    }

    sim_close_pending(app.sh);

    int total = app.sh->rows * app.sh->cols;
    int burning = 0, burnt = 0, ext = 0, flammable_total = 0, untouched = 0;
    for (int i = 0; i < total; i++) {
        Terrain t = app.sh->cells[i];
        if (t != TERRAIN_WATER && t != TERRAIN_FIREBREAK) flammable_total++;
        if      (app.sh->state[i] == ST_BURNING)      burning++;
        else if (app.sh->state[i] == ST_BURNT)        burnt++;
        else if (app.sh->state[i] == ST_EXTINGUISHED) ext++;
        else if (app.sh->state[i] == ST_SAFE) {
            if (t != TERRAIN_WATER && t != TERRAIN_FIREBREAK) untouched++;
        }
    }

    printf("\n────────── ИТОГ ──────────\n");
    printf("Карта:      %s\n", map_path);
    printf("Условия:    %s\n", cfg_path);
    printf("Станция:    (%d,%d)\n", st_r + 1, st_c + 1);
    printf("Результат:  %s\n", sim_result_name(result));
    printf("Тактов:     %d\n", app.sh->tick);
    printf("seed:       %u\n", seed);
    printf("Выгорело:   %d\n", burnt);
    printf("Потухло:    %d\n", ext);
    printf("Горит:      %d\n", burning);
    printf("Нетронуто:  %d\n", untouched);
    printf("──────────────────────────\n");

    fire_app_destroy(&app, SHM_NAME);
    map_free(&map);

    free(args_sensor);
    free(args_team);
    free(args_fire);

    return 0;
}