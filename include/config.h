#ifndef CONFIG_H
#define CONFIG_H

/* ─── Направление ветра ───────────────────────────────── */
typedef enum {
    WIND_N, WIND_NE, WIND_E, WIND_SE,
    WIND_S, WIND_SW, WIND_W, WIND_NW,
    WIND_COUNT
} WindDir;

/* ─── Все настройки симуляции ─────────────────────────── */
typedef struct {
    /* ── физика огня ── */
    double fire_base_prob_grass;
    double fire_base_prob_tree;
    double fire_burn_time_factor;
    double fire_wind_factor_along;
    double fire_wind_factor_against;
    double fire_wind_factor_side;
    int    fire_spread_8dir;
    int    fire_reburn_burnt;

    /* ── ветер ── */
    WindDir wind_direction;
    int     wind_power;

    /* ── датчики ── */
    int sensor_radius;
    int sensor_resend_period;

    /* ── сообщения ── */
    int    msg_delay_min;
    int    msg_delay_max;
    double msg_loss_prob;
    double msg_dup_prob;

    /* ── пожарные группы ── */
    int    team_count;
    int    team_speed;
    int    team_extinguish_time;
    double team_efficiency;
    int    team_return_to_station;
    int    team_can_cross_water;
    int    team_can_cross_firebreak;

    /* ── условия завершения ── */
    int end_max_ticks;
    int end_unlimited;
} Config;

/* ─── API ─────────────────────────────────────────────── */

/* Прочитать файл конфига. Возвращает 0 при успехе, -1 при ошибке. */
int config_load(const char *path, Config *out);

/* Заполнить Config значениями по умолчанию. */
void config_defaults(Config *out);

/* Напечатать конфиг (для отладки). */
void config_dump(const Config *c);

/* Строка направления ветра ("N", "NE", …, "NW"). */
const char *wind_dir_name(WindDir d);

#endif