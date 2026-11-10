#define _POSIX_C_SOURCE 200809L

#include "model.h"

void sensors_observe_one(Ctx *ctx, int sensor_idx, unsigned int *rng) {
    SharedState *sh = ctx->sh;
    const Sensor *s = &sh->sensors[sensor_idx];
    int rad = sh->cfg.sensor_radius;
    int R = sh->rows, C = sh->cols;
    int n_cells = R * C;

    unsigned char *reported = sh->sensor_reported +
                              (size_t)sensor_idx * (size_t)n_cells;

    for (int r = s->r - rad; r <= s->r + rad; r++) {
        if (r < 0 || r >= R) continue;
        for (int c = s->c - rad; c <= s->c + rad; c++) {
            if (c < 0 || c >= C) continue;
            int idx = r * C + c;
            if (sh->state[idx] != ST_BURNING) continue;
            if (reported[idx]) continue;

            reported[idx] = 1;
            messages_send(ctx, sensor_idx, r, c, rng);
        }
    }
}