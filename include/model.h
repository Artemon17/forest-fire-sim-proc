#ifndef MODEL_H
#define MODEL_H

#include "shm.h"

typedef struct {
    SharedState *sh;
    SyncMutex   *teams_lock;
    SyncMutex   *log_lock;
} Ctx;

void log_shared_init(SharedState *sh);
void log_shared_add(Ctx *ctx, int tick, const char *fmt, ...);
int  log_shared_last_n(Ctx *ctx, int n, LogEvent *out);

int  fire_burn_ticks(const SharedState *sh, int r, int c);
void fire_spread_worker(Ctx *ctx, int idx_lo, int idx_hi, unsigned int *rng);

void sensors_observe_one(Ctx *ctx, int sensor_idx, unsigned int *rng);

void messages_send(Ctx *ctx, int sensor_idx, int r, int c, unsigned int *rng);
void messages_tick(Ctx *ctx);

void center_tick(Ctx *ctx);

void teams_observe(Ctx *ctx, unsigned int *rng);
void teams_move_one(Ctx *ctx, int team_idx);
void teams_extinguish_one(Ctx *ctx, int team_idx);

void render_frame(Ctx *ctx);

const char *wind_dir_name_shared(int d);

#endif