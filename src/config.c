#define _POSIX_C_SOURCE 200809L

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* ─── Значения по умолчанию ───────────────────────────── */

void config_defaults(Config *c) {
    memset(c, 0, sizeof(*c));

    /* физика огня */
    c->fire_base_prob_grass     = 0.20;
    c->fire_base_prob_tree      = 0.35;
    c->fire_burn_time_factor    = 1.5;
    c->fire_wind_factor_along   = 1.8;
    c->fire_wind_factor_against = 0.4;
    c->fire_wind_factor_side    = 1.0;
    c->fire_spread_8dir         = 1;
    c->fire_reburn_burnt        = 0;

    /* ветер */
    c->wind_direction = WIND_SE;
    c->wind_power     = 2;

    /* датчики */
    c->sensor_radius        = 2;
    c->sensor_resend_period = 5;

    /* сообщения */
    c->msg_delay_min = 1;
    c->msg_delay_max = 3;
    c->msg_loss_prob = 0.10;
    c->msg_dup_prob  = 0.05;

    /* группы */
    c->team_count               = 3;
    c->team_speed               = 1;
    c->team_extinguish_time     = 4;
    c->team_efficiency          = 1.0;
    c->team_return_to_station   = 1;
    c->team_can_cross_water     = 0;
    c->team_can_cross_firebreak = 1;

    /* завершение */
    c->end_max_ticks = 500;
    c->end_unlimited = 0;
}

/* ─── Вспомогательные функции ─────────────────────────── */

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

static int split_kv(char *line, char **key, char **value) {
    char *eq = strchr(line, '=');
    if (!eq) return -1;
    *eq = '\0';
    *key   = trim(line);
    *value = trim(eq + 1);
    return 0;
}

static int parse_bool(const char *s) {
    if (!strcmp(s, "true")  || !strcmp(s, "1") || !strcmp(s, "yes")) return 1;
    if (!strcmp(s, "false") || !strcmp(s, "0") || !strcmp(s, "no"))  return 0;
    return -1;
}

static WindDir parse_wind_dir(const char *s) {
    if (!strcmp(s, "N"))  return WIND_N;
    if (!strcmp(s, "NE")) return WIND_NE;
    if (!strcmp(s, "E"))  return WIND_E;
    if (!strcmp(s, "SE")) return WIND_SE;
    if (!strcmp(s, "S"))  return WIND_S;
    if (!strcmp(s, "SW")) return WIND_SW;
    if (!strcmp(s, "W"))  return WIND_W;
    if (!strcmp(s, "NW")) return WIND_NW;
    return WIND_COUNT;
}

/* ─── Применить одну пару ключ=значение ───────────────── */

static int apply_kv(Config *c, const char *key, const char *val,
                    const char *file, int lineno)
{
    #define FAIL(fmt, ...) do { \
        fprintf(stderr, "%s:%d: " fmt "\n", file, lineno, ##__VA_ARGS__); \
        return -1; \
    } while (0)

    /* ── строковые ── */
    if (!strcmp(key, "wind.direction")) {
        WindDir d = parse_wind_dir(val);
        if (d == WIND_COUNT) FAIL("неверное направление ветра: '%s'", val);
        c->wind_direction = d;
        return 0;
    }

    /* ── булевы ── */
    if (!strcmp(key, "fire.spread_8dir") ||
        !strcmp(key, "fire.reburn_burnt") ||
        !strcmp(key, "team.return_to_station") ||
        !strcmp(key, "team.can_cross_water") ||
        !strcmp(key, "team.can_cross_firebreak") ||
        !strcmp(key, "end.unlimited"))
    {
        int b = parse_bool(val);
        if (b < 0) FAIL("ожидалось true/false, получено '%s'", val);

        if      (!strcmp(key, "fire.spread_8dir"))          c->fire_spread_8dir = b;
        else if (!strcmp(key, "fire.reburn_burnt"))         c->fire_reburn_burnt = b;
        else if (!strcmp(key, "team.return_to_station"))    c->team_return_to_station = b;
        else if (!strcmp(key, "team.can_cross_water"))      c->team_can_cross_water = b;
        else if (!strcmp(key, "team.can_cross_firebreak"))  c->team_can_cross_firebreak = b;
        else                                                c->end_unlimited = b;
        return 0;
    }

    /* ── целые ── */
    if (!strcmp(key, "wind.power") ||
        !strcmp(key, "sensor.radius") ||
        !strcmp(key, "sensor.resend_period") ||
        !strcmp(key, "msg.delay_min") ||
        !strcmp(key, "msg.delay_max") ||
        !strcmp(key, "team.count") ||
        !strcmp(key, "team.speed") ||
        !strcmp(key, "team.extinguish_time") ||
        !strcmp(key, "end.max_ticks"))
    {
        char *endp = NULL;
        long v = strtol(val, &endp, 10);
        if (*val == '\0' || *endp != '\0')
            FAIL("ожидалось целое, получено '%s'", val);

        if      (!strcmp(key, "wind.power"))              c->wind_power = (int)v;
        else if (!strcmp(key, "sensor.radius"))           c->sensor_radius = (int)v;
        else if (!strcmp(key, "sensor.resend_period"))    c->sensor_resend_period = (int)v;
        else if (!strcmp(key, "msg.delay_min"))           c->msg_delay_min = (int)v;
        else if (!strcmp(key, "msg.delay_max"))           c->msg_delay_max = (int)v;
        else if (!strcmp(key, "team.count"))              c->team_count = (int)v;
        else if (!strcmp(key, "team.speed"))              c->team_speed = (int)v;
        else if (!strcmp(key, "team.extinguish_time"))    c->team_extinguish_time = (int)v;
        else                                              c->end_max_ticks = (int)v;
        return 0;
    }

    /* ── вещественные ── */
    if (!strcmp(key, "fire.base_prob_grass") ||
        !strcmp(key, "fire.base_prob_tree") ||
        !strcmp(key, "fire.burn_time_factor") ||
        !strcmp(key, "fire.wind_factor_along") ||
        !strcmp(key, "fire.wind_factor_against") ||
        !strcmp(key, "fire.wind_factor_side") ||
        !strcmp(key, "msg.loss_prob") ||
        !strcmp(key, "msg.dup_prob") ||
        !strcmp(key, "team.efficiency"))
    {
        char *endp = NULL;
        double v = strtod(val, &endp);
        if (*val == '\0' || *endp != '\0')
            FAIL("ожидалось число, получено '%s'", val);

        if      (!strcmp(key, "fire.base_prob_grass"))      c->fire_base_prob_grass = v;
        else if (!strcmp(key, "fire.base_prob_tree"))       c->fire_base_prob_tree = v;
        else if (!strcmp(key, "fire.burn_time_factor"))     c->fire_burn_time_factor = v;
        else if (!strcmp(key, "fire.wind_factor_along"))    c->fire_wind_factor_along = v;
        else if (!strcmp(key, "fire.wind_factor_against"))  c->fire_wind_factor_against = v;
        else if (!strcmp(key, "fire.wind_factor_side"))     c->fire_wind_factor_side = v;
        else if (!strcmp(key, "msg.loss_prob"))             c->msg_loss_prob = v;
        else if (!strcmp(key, "msg.dup_prob"))              c->msg_dup_prob = v;
        else                                                c->team_efficiency = v;
        return 0;
    }

    FAIL("неизвестный ключ '%s'", key);
    #undef FAIL
}

/* ─── Загрузка файла ──────────────────────────────────── */

int config_load(const char *path, Config *out) {
    config_defaults(out);

    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "config: не удалось открыть '%s': %s\n",
                path, strerror(errno));
        return -1;
    }

    char line[512];
    int  lineno = 0;
    int  errors = 0;

    while (fgets(line, sizeof(line), f)) {
        lineno++;

        /* отрезать комментарий */
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';

        char *s = trim(line);
        if (*s == '\0') continue;

        char *key, *val;
        if (split_kv(s, &key, &val) != 0) {
            fprintf(stderr, "%s:%d: строка без '=': '%s'\n",
                    path, lineno, s);
            errors++;
            continue;
        }

        if (apply_kv(out, key, val, path, lineno) != 0)
            errors++;
    }

    fclose(f);

    if (errors > 0) {
        fprintf(stderr, "config: %d ошибок в '%s'\n", errors, path);
        return -1;
    }
    return 0;
}

/* ─── Печать ──────────────────────────────────────────── */

const char *wind_dir_name(WindDir d) {
    static const char *names[] = {"N","NE","E","SE","S","SW","W","NW"};
    if (d < 0 || d >= WIND_COUNT) return "?";
    return names[d];
}

void config_dump(const Config *c) {
    printf("Config:\n");
    printf("  fire.base_prob_grass     = %.3f\n", c->fire_base_prob_grass);
    printf("  fire.base_prob_tree      = %.3f\n", c->fire_base_prob_tree);
    printf("  fire.burn_time_factor    = %.3f\n", c->fire_burn_time_factor);
    printf("  fire.wind_factor_along   = %.3f\n", c->fire_wind_factor_along);
    printf("  fire.wind_factor_against = %.3f\n", c->fire_wind_factor_against);
    printf("  fire.wind_factor_side    = %.3f\n", c->fire_wind_factor_side);
    printf("  fire.spread_8dir         = %d\n",   c->fire_spread_8dir);
    printf("  fire.reburn_burnt        = %d\n",   c->fire_reburn_burnt);

    printf("  wind.direction           = %s\n",   wind_dir_name(c->wind_direction));
    printf("  wind.power               = %d\n",   c->wind_power);

    printf("  sensor.radius            = %d\n",   c->sensor_radius);
    printf("  sensor.resend_period     = %d\n",   c->sensor_resend_period);

    printf("  msg.delay_min            = %d\n",   c->msg_delay_min);
    printf("  msg.delay_max            = %d\n",   c->msg_delay_max);
    printf("  msg.loss_prob            = %.3f\n", c->msg_loss_prob);
    printf("  msg.dup_prob             = %.3f\n", c->msg_dup_prob);

    printf("  team.count               = %d\n",   c->team_count);
    printf("  team.speed               = %d\n",   c->team_speed);
    printf("  team.extinguish_time     = %d\n",   c->team_extinguish_time);
    printf("  team.efficiency          = %.3f\n", c->team_efficiency);
    printf("  team.return_to_station   = %d\n",   c->team_return_to_station);
    printf("  team.can_cross_water     = %d\n",   c->team_can_cross_water);
    printf("  team.can_cross_firebreak = %d\n",   c->team_can_cross_firebreak);

    printf("  end.max_ticks            = %d\n",   c->end_max_ticks);
    printf("  end.unlimited            = %d\n",   c->end_unlimited);
}