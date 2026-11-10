#define _POSIX_C_SOURCE 200809L

#include "map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* ─── Внутренние состояния парсера ────────────────────── */

enum {
    SEC_NONE = 0,
    SEC_TERRAIN,
    SEC_IGNITION,
    SEC_SENSORS,
    SEC_CANDIDATES,
    SEC_UNKNOWN
};

/* Обрезать пробелы с обоих концов (на месте). */
static char *trim_inplace(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

static int section_from_name(const char *name) {
    if (!strcmp(name, "terrain"))            return SEC_TERRAIN;
    if (!strcmp(name, "ignition"))           return SEC_IGNITION;
    if (!strcmp(name, "sensors"))            return SEC_SENSORS;
    if (!strcmp(name, "station_candidates")) return SEC_CANDIDATES;
    return SEC_UNKNOWN;
}

/* Разобрать "r c" в две координаты. 0 при успехе. */
static int parse_coord(const char *s, int *r, int *c) {
    char *endp = NULL;
    long rr = strtol(s, &endp, 10);
    if (endp == s) return -1;
    while (*endp == ' ' || *endp == '\t') endp++;
    if (*endp == '\0') return -1;
    long cc = strtol(endp, &endp, 10);
    if (*endp != '\0' && *endp != ' ' && *endp != '\t') return -1;
    *r = (int)rr;
    *c = (int)cc;
    return 0;
}

/* Добавить координату в динамический массив. */
static int push_coord(Coord **arr, int *count, int *cap, int r, int c) {
    if (*count >= *cap) {
        int ncap = *cap ? *cap * 2 : 16;
        Coord *na = realloc(*arr, (size_t)ncap * sizeof(Coord));
        if (!na) return -1;
        *arr = na;
        *cap = ncap;
    }
    (*arr)[*count].r = r;
    (*arr)[*count].c = c;
    (*count)++;
    return 0;
}

/* ─── Основная функция ─────────────────────────────────── */

int map_load(const char *path, MapData *out) {
    memset(out, 0, sizeof(*out));

    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "map: не удалось открыть '%s': %s\n", path, strerror(errno));
        return -1;
    }

    char line[2048];
    int  lineno = 0;
    int  errors = 0;
    int  sec = SEC_NONE;
    int  size_known = 0;
    int  terrain_row = 0;
    int  ign_cap = 0, sens_cap = 0, cand_cap = 0;

    while (fgets(line, sizeof(line), f)) {
        lineno++;

        /* убрать \r\n, но не трогать пробелы внутри строки */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        /* ── Внутри terrain строки значимы как есть ── */
        if (sec == SEC_TERRAIN) {
            /* копия для проверки на заголовок секции / комментарий */
            char probe[2048];
            strncpy(probe, line, sizeof(probe) - 1);
            probe[sizeof(probe) - 1] = '\0';
            char *pt = trim_inplace(probe);

            if (*pt == '#' || *pt == '\0') continue;

            size_t plen = strlen(pt);
            if (plen > 0 && pt[plen-1] == ':') {
                pt[plen-1] = '\0';
                char *name = trim_inplace(pt);
                int ns = section_from_name(name);
                if (ns == SEC_UNKNOWN) {
                    fprintf(stderr, "%s:%d: неизвестная секция '%s'\n",
                            path, lineno, name);
                    errors++;
                    continue;
                }
                sec = ns;
                continue;
            }

            /* это строка terrain */
            if (!size_known) {
                fprintf(stderr, "%s:%d: terrain до size\n", path, lineno);
                errors++;
                continue;
            }
            if (terrain_row >= out->rows) {
                fprintf(stderr, "%s:%d: лишняя строка terrain\n", path, lineno);
                errors++;
                continue;
            }
            if (len < (size_t)out->cols) {
                fprintf(stderr,
                        "%s:%d: terrain строка %d короткая (%zu < %d)\n",
                        path, lineno, terrain_row, len, out->cols);
                errors++;
                continue;
            }
            for (int c = 0; c < out->cols; c++) {
                int t = terrain_parse(line[c]);
                if (t < 0) {
                    fprintf(stderr,
                            "%s:%d: неверный символ '%c' в terrain[%d][%d]\n",
                            path, lineno, line[c], terrain_row, c);
                    errors++;
                    continue;
                }
                out->cells[terrain_row * out->cols + c] = (Terrain)t;
            }
            terrain_row++;
            continue;
        }

        /* ── Остальные секции: режем комментарии, тримим ── */
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';

        char *s = trim_inplace(line);
        if (*s == '\0') continue;

        /* заголовок секции? */
        size_t slen = strlen(s);
        if (s[slen-1] == ':') {
            s[slen-1] = '\0';
            char *name = trim_inplace(s);
            int ns = section_from_name(name);
            if (ns == SEC_UNKNOWN) {
                fprintf(stderr, "%s:%d: неизвестная секция '%s'\n",
                        path, lineno, name);
                errors++;
                continue;
            }
            sec = ns;
            continue;
        }

        /* ключ=значение? */
        char *eq = strchr(s, '=');
        if (eq) {
            *eq = '\0';
            char *k = trim_inplace(s);
            char *v = trim_inplace(eq + 1);

            if (!strcmp(k, "size")) {
                int r, c;
                if (parse_coord(v, &r, &c) != 0 || r <= 0 || c <= 0) {
                    fprintf(stderr, "%s:%d: неверный size '%s'\n",
                            path, lineno, v);
                    errors++;
                    continue;
                }
                out->rows = r;
                out->cols = c;
                out->cells = calloc((size_t)r * (size_t)c, sizeof(Terrain));
                if (!out->cells) {
                    fprintf(stderr, "%s:%d: не хватило памяти\n", path, lineno);
                    errors++;
                    size_known = 0;
                    continue;
                }
                size_known = 1;
            } else {
                fprintf(stderr, "%s:%d: неизвестный ключ '%s'\n",
                        path, lineno, k);
                errors++;
            }
            continue;
        }

        /* ── Данные в списковых секциях ── */
        if (sec == SEC_IGNITION || sec == SEC_SENSORS || sec == SEC_CANDIDATES) {
            int r, c;
            if (parse_coord(s, &r, &c) != 0) {
                fprintf(stderr,
                        "%s:%d: ожидались координаты 'r c', получено '%s'\n",
                        path, lineno, s);
                errors++;
                continue;
            }
            if (r < 0 || r >= out->rows || c < 0 || c >= out->cols) {
                fprintf(stderr, "%s:%d: координата (%d,%d) вне карты %dx%d\n",
                        path, lineno, r, c, out->rows, out->cols);
                errors++;
                continue;
            }

            if (sec == SEC_IGNITION) {
                Terrain t = out->cells[r * out->cols + c];
                if (t == TERRAIN_WATER || t == TERRAIN_FIREBREAK) {
                    fprintf(stderr,
                            "%s:%d: очаг (%d,%d) на невоспламеняемой клетке '%c'\n",
                            path, lineno, r, c, terrain_symbol(t));
                    errors++;
                    continue;
                }
                if (push_coord(&out->ignition, &out->ignition_count,
                               &ign_cap, r, c) != 0) {
                    errors++;
                    continue;
                }
            } else if (sec == SEC_SENSORS) {
                if (push_coord(&out->sensors, &out->sensors_count,
                               &sens_cap, r, c) != 0) {
                    errors++;
                    continue;
                }
            } else {
                Terrain t = out->cells[r * out->cols + c];
                if (t == TERRAIN_WATER) {
                    fprintf(stderr, "%s:%d: кандидат станции (%d,%d) на воде\n",
                            path, lineno, r, c);
                    errors++;
                    continue;
                }
                if (push_coord(&out->station_candidates,
                               &out->station_candidates_count,
                               &cand_cap, r, c) != 0) {
                    errors++;
                    continue;
                }
            }
            continue;
        }

        fprintf(stderr, "%s:%d: строка данных вне секции: '%s'\n",
                path, lineno, s);
        errors++;
    }

    fclose(f);

    if (errors > 0) {
        fprintf(stderr, "map: %d ошибок в '%s'\n", errors, path);
        map_free(out);
        return -1;
    }
    if (!size_known) {
        fprintf(stderr, "map: '%s' — не указан size\n", path);
        map_free(out);
        return -1;
    }
    if (terrain_row != out->rows) {
        fprintf(stderr, "map: '%s' — terrain строк %d, ожидалось %d\n",
                path, terrain_row, out->rows);
        map_free(out);
        return -1;
    }

    return 0;
}

void map_free(MapData *m) {
    if (!m) return;
    free(m->cells);
    free(m->ignition);
    free(m->sensors);
    free(m->station_candidates);
    memset(m, 0, sizeof(*m));
}

char terrain_symbol(Terrain t) {
    switch (t) {
        case TERRAIN_GRASS:     return '"';
        case TERRAIN_TREE:      return 't';
        case TERRAIN_WATER:     return 'w';
        case TERRAIN_FIREBREAK: return '_';
    }
    return '?';
}

int terrain_parse(char ch) {
    switch (ch) {
        case ' ': case '"': return TERRAIN_GRASS;
        case 't':           return TERRAIN_TREE;
        case 'w':           return TERRAIN_WATER;
        case '_':           return TERRAIN_FIREBREAK;
        default:            return -1;
    }
}

void map_dump(const MapData *m) {
    printf("Map: %dx%d\n", m->rows, m->cols);
    printf("Terrain:\n");
    for (int r = 0; r < m->rows; r++) {
        printf("  ");
        for (int c = 0; c < m->cols; c++)
            printf("%c", terrain_symbol(m->cells[r * m->cols + c]));
        printf("\n");
    }
    printf("Ignition (%d):\n", m->ignition_count);
    for (int i = 0; i < m->ignition_count; i++)
        printf("  (%d,%d)\n", m->ignition[i].r, m->ignition[i].c);
    printf("Sensors (%d):\n", m->sensors_count);
    for (int i = 0; i < m->sensors_count; i++)
        printf("  (%d,%d)\n", m->sensors[i].r, m->sensors[i].c);
    printf("Station candidates (%d):\n", m->station_candidates_count);
    for (int i = 0; i < m->station_candidates_count; i++)
        printf("  (%d,%d)\n",
               m->station_candidates[i].r, m->station_candidates[i].c);
}