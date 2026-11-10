#define _POSIX_C_SOURCE 200809L

#include "main_common.h"
#include "config.h"
#include "map.h"
#include "term.h"
#include "model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

static const char *SHM_NAME = "/fire_state_standalone";
static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) { (void)sig; g_stop = 1; }

static void sleep_ms_local(long ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

int main(int argc, char **argv) {
    const char *map_path = NULL, *cfg_path = "data/conditions.conf";
    int st_r = 0, st_c = 0, st_given = 0, quiet = 0;
    unsigned int seed = (unsigned)time(NULL);
    long delay_ms = 300;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--map") && i + 1 < argc) map_path = argv[++i];
        else if (!strcmp(argv[i], "--conditions") && i + 1 < argc) cfg_path = argv[++i];
        else if (!strcmp(argv[i], "--station") && i + 2 < argc) {
            st_r = atoi(argv[++i]);
            st_c = atoi(argv[++i]);
            st_given = 1;
        } else if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
            seed = (unsigned)strtoul(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--delay") && i + 1 < argc) {
            delay_ms = strtol(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--quiet")) {
            quiet = 1;
        } else {
            fprintf(stderr, "Неизвестный аргумент: %s\n", argv[i]);
            return 2;
        }
    }

    if (!map_path) { fprintf(stderr, "--map обязателен\n"); return 2; }

    Config cfg;
    if (config_load(cfg_path, &cfg) != 0) return 3;

    MapData map;
    if (map_load(map_path, &map) != 0) return 4;

    if (!st_given && map.station_candidates_count > 0) {
        st_r = map.station_candidates[0].r;
        st_c = map.station_candidates[0].c;
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
    if (fire_app_create(&app, SHM_NAME, &map, &scfg, st_r, st_c, seed, 1) != 0)
        return 5;

    printf("Ожидаю подключения fire-sensors, fire-teams, fire-spread...\n");

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    for (int i = 0; i < 3; i++) sync_sem_wait(&app.ready);
    printf("Все три программы подключились, начинаю симуляцию\n");

    if (!quiet) {
        term_install_signals();
        term_alt_screen_enter();
        if (term_raw_mode() == 0) term_hide_cursor();
        term_clear();
    }
    if (!quiet) render_frame(&app.ctx);
    if (!quiet && delay_ms > 0) sleep_ms_local(delay_ms);

    int result = sim_finished(app.sh);
    int W = app.num_fire;

    while (result == 0 && !g_stop && !term_should_stop()) {
        app.sh->tick++;

        sync_sem_post(&app.sensors_go);
        sync_sem_wait(&app.sensors_done);

        teams_observe(&app.ctx, &app.seed);
        messages_tick(&app.ctx);

        sync_mutex_lock(&app.teams_lock);
        center_tick(&app.ctx);
        sync_mutex_unlock(&app.teams_lock);

        sync_sem_post(&app.teams_go);
        sync_sem_wait(&app.teams_done);

        int total = app.sh->rows * app.sh->cols;
        for (int i = 0; i < total; i++)
            atomic_store(&app.sh->next_state[i], app.sh->state[i]);

        for (int i = 0; i < W; i++) sync_sem_post(&app.fire_go);
        for (int i = 0; i < W; i++) sync_sem_wait(&app.fire_done);

        sim_apply_next_state(app.sh);

        if (!quiet) {
            render_frame(&app.ctx);
            if (delay_ms > 0) sleep_ms_local(delay_ms);
            int k = term_poll_key();
            if (k == 'q') { result = 4; break; }
        }

        result = sim_finished(app.sh);
    }

    if (g_stop || term_should_stop()) result = 4;

    atomic_store(&app.sh->stop, 1);
    sync_sem_post(&app.sensors_go);
    sync_sem_post(&app.teams_go);
    for (int i = 0; i < W; i++) sync_sem_post(&app.fire_go);

    sync_sem_wait(&app.sensors_done);
    sync_sem_wait(&app.teams_done);
    for (int i = 0; i < W; i++) sync_sem_wait(&app.fire_done);

    for (int i = 0; i < 3; i++) sync_sem_wait(&app.done);

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
    int burning = 0, burnt = 0, ext = 0;
    for (int i = 0; i < total; i++) {
        if      (app.sh->state[i] == ST_BURNING)      burning++;
        else if (app.sh->state[i] == ST_BURNT)        burnt++;
        else if (app.sh->state[i] == ST_EXTINGUISHED) ext++;
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
    printf("──────────────────────────\n");

    fire_app_destroy(&app, SHM_NAME);
    map_free(&map);
    return 0;
}