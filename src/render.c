#define _POSIX_C_SOURCE 200809L

#include "model.h"
#include "term.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

const char *wind_dir_name_shared(int d) {
    static const char *names[] = {"N","NE","E","SE","S","SW","W","NW"};
    if (d < 0 || d > 7) return "?";
    return names[d];
}

static const char *color_for(char sym) {
    switch (sym) {
        case 'F': return C_FIRE;
        case '#': return C_BURNT;
        case 'w': return C_WATER;
        case '_': return C_FIREBRK;
        case '.': return C_SENSOR;
        case 'S': return C_STATION;
        case 't': return C_FOREST;
        case '"': return C_GRASS;
        default:
            if (sym >= '1' && sym <= '9') return C_TEAM;
            return C_RESET;
    }
}

static char cell_symbol(const SharedState *sh, int r, int c) {
    int idx = r * sh->cols + c;
    if (r == sh->st_r && c == sh->st_c) return 'S';

    if (sh->occupant[idx] == OCC_TEAM) {
        int tid = sh->team_id[idx];
        return (tid >= 1 && tid <= 9) ? (char)('0' + tid) : 'T';
    }
    if (sh->state[idx] == ST_BURNING) return 'F';
    if (sh->state[idx] == ST_BURNT)   return '#';
    if (sh->occupant[idx] == OCC_SENSOR) return '.';

    switch (sh->cells[idx]) {
        case TERRAIN_WATER:     return 'w';
        case TERRAIN_FIREBREAK: return '_';
        case TERRAIN_TREE:      return 't';
        default:                return '"';
    }
}

void render_frame(Ctx *ctx) {
    SharedState *sh = ctx->sh;
    static char buf[1 << 16];
    size_t p = 0;

    #define APPEND(...) do { \
        int _n = snprintf(buf + p, sizeof(buf) - p, __VA_ARGS__); \
        if (_n > 0) p += (size_t)_n; \
        if (p >= sizeof(buf)) p = sizeof(buf) - 1; \
    } while (0)

    #define ENDL() do { APPEND("\033[K\r\n"); } while (0)

    APPEND("\033[H\033[2J");

    int burning = 0, burnt = 0, ext = 0;
    int total = sh->rows * sh->cols;
    for (int i = 0; i < total; i++) {
        if      (sh->state[i] == ST_BURNING)      burning++;
        else if (sh->state[i] == ST_BURNT)        burnt++;
        else if (sh->state[i] == ST_EXTINGUISHED) ext++;
    }

    APPEND("Такт %d | Ветер %s %d | Огонь: %d | Потухло: %d | Выгорело: %d | В пути: %d | В инбоксе: %d",
           sh->tick,
           wind_dir_name_shared(sh->cfg.wind_direction),
           sh->cfg.wind_power,
           burning, ext, burnt,
           sh->in_flight_count, sh->inbox_count);
    ENDL();

    int pend = 0, active = 0, done = 0, cancelled = 0;
    for (int i = 0; i < sh->tasks_count; i++) {
        switch (sh->tasks[i].state) {
            case TASK_PENDING:     pend++;      break;
            case TASK_ASSIGNED:
            case TASK_IN_PROGRESS: active++;    break;
            case TASK_DONE:        done++;      break;
            case TASK_CANCELLED:   cancelled++; break;
        }
    }
    int busy = 0;
    for (int i = 0; i < sh->teams_count; i++)
        if (sh->teams[i].target_task_id != 0) busy++;

    APPEND("Задачи: ждут %d, в работе %d, готово %d, отменено %d | Группы: занято %d из %d",
           pend, active, done, cancelled, busy, sh->teams_count);
    ENDL();

    APPEND("─────");
    for (int c = 0; c < sh->cols * 2; c++) APPEND("─");
    ENDL();

    APPEND("    ");
    for (int c = 1; c <= sh->cols; c++) APPEND("%2d", c);
    ENDL();

    for (int r = 0; r < sh->rows; r++) {
        APPEND("%2d |", r + 1);
        for (int c = 0; c < sh->cols; c++) {
            char sym = cell_symbol(sh, r, c);
            const char *col = color_for(sym);
            APPEND("%s%c%s ", col, sym, C_RESET);
        }
        ENDL();
    }

    APPEND("─────");
    for (int c = 0; c < sh->cols * 2; c++) APPEND("─");
    ENDL();

    APPEND("События:");
    ENDL();

    LogEvent last[5];
    int log_n = log_shared_last_n(ctx, 5, last);
    if (log_n == 0) {
        APPEND("  (пока тихо)");
        ENDL();
    } else {
        for (int i = 0; i < log_n; i++) {
            APPEND("  [%d] %s", last[i].tick, last[i].text);
            ENDL();
        }
    }

    #undef APPEND
    #undef ENDL

    ssize_t off = 0;
    while (off < (ssize_t)p) {
        ssize_t k = write(STDOUT_FILENO, buf + off, (size_t)p - (size_t)off);
        if (k < 0) break;
        off += k;
    }
}