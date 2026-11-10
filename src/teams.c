#define _POSIX_C_SOURCE 200809L

#include "model.h"

#include <stdlib.h>
#include <string.h>

#define EXT_RADIUS 2

static const int DR[8] = { -1, -1,  0, +1, +1, +1,  0, -1 };
static const int DC[8] = {  0, +1, +1, +1,  0, -1, -1, -1 };

static int abs_i(int x) { return x < 0 ? -x : x; }

static int team_can_stand(const SharedState *sh, int r, int c) {
    int R = sh->rows, C = sh->cols;
    if (r < 0 || r >= R || c < 0 || c >= C) return 0;
    int idx = r * C + c;
    Terrain t = sh->cells[idx];
    if (t == TERRAIN_WATER && !sh->cfg.team_can_cross_water) return 0;
    if (t == TERRAIN_FIREBREAK && !sh->cfg.team_can_cross_firebreak) return 0;
    if (sh->state[idx] == ST_BURNING) return 0;
    return 1;
}

static int bfs_next_step(const SharedState *sh,
                         int sr, int sc, int tr, int tc,
                         int *out_r, int *out_c)
{
    int R = sh->rows, C = sh->cols;
    int n = R * C;
    if (sr == tr && sc == tc) return 0;

    int *prev  = malloc((size_t)n * sizeof(int));
    int *queue = malloc((size_t)n * sizeof(int));
    if (!prev || !queue) { free(prev); free(queue); return 0; }

    for (int i = 0; i < n; i++) prev[i] = -1;

    int head = 0, tail = 0;
    int start = sr * C + sc;
    int goal  = tr * C + tc;

    queue[tail++] = start;
    prev[start] = start;

    int found = 0;
    while (head < tail) {
        int cur = queue[head++];
        if (cur == goal) { found = 1; break; }
        int cr = cur / C, cc = cur % C;
        for (int d = 0; d < 8; d++) {
            int nr = cr + DR[d], nc = cc + DC[d];
            if (nr < 0 || nr >= R || nc < 0 || nc >= C) continue;
            int nidx = nr * C + nc;
            if (prev[nidx] != -1) continue;
            if (nidx == goal) {
                prev[nidx] = cur; queue[tail++] = nidx; continue;
            }
            if (!team_can_stand(sh, nr, nc)) continue;
            if (sh->occupant[nidx] == OCC_TEAM) continue;
            prev[nidx] = cur;
            queue[tail++] = nidx;
        }
    }

    if (!found) { free(prev); free(queue); return 0; }

    int cur = goal;
    while (prev[cur] != start && prev[cur] != cur) cur = prev[cur];
    *out_r = cur / C;
    *out_c = cur % C;
    free(prev);
    free(queue);
    return 1;
}

static void team_mark_leave(SharedState *sh, FireTeam *t) {
    int idx = t->r * sh->cols + t->c;
    if (sh->occupant[idx] == OCC_TEAM && sh->team_id[idx] == t->id) {
        sh->occupant[idx] = OCC_NONE;
        sh->team_id[idx]  = 0;
    }
}

static void team_mark_enter(SharedState *sh, FireTeam *t) {
    int idx = t->r * sh->cols + t->c;
    sh->occupant[idx] = OCC_TEAM;
    sh->team_id[idx]  = t->id;
}

static Task *find_task(SharedState *sh, int id) {
    for (int i = 0; i < sh->tasks_count; i++)
        if (sh->tasks[i].id == id) return &sh->tasks[i];
    return NULL;
}

static int find_nearby_fire(const SharedState *sh, int r, int c,
                            int *out_r, int *out_c)
{
    int R = sh->rows, C = sh->cols;
    int best_d = 1 << 30;
    int found = 0;
    for (int dr = -EXT_RADIUS; dr <= EXT_RADIUS; dr++) {
        for (int dc = -EXT_RADIUS; dc <= EXT_RADIUS; dc++) {
            if (dr == 0 && dc == 0) continue;
            int nr = r + dr, nc = c + dc;
            if (nr < 0 || nr >= R || nc < 0 || nc >= C) continue;
            if (sh->state[nr * C + nc] != ST_BURNING) continue;
            int d = abs_i(dr) + abs_i(dc);
            if (d < best_d) { best_d = d; *out_r = nr; *out_c = nc; found = 1; }
        }
    }
    return found;
}

static void push_task(Ctx *ctx, const Task *t) {
    SharedState *sh = ctx->sh;
    if (sh->tasks_count >= SHM_MAX_TASKS) return;
    sh->tasks[sh->tasks_count++] = *t;
}

void teams_move_one(Ctx *ctx, int team_idx) {
    SharedState *sh = ctx->sh;
    FireTeam *t = &sh->teams[team_idx];
    if (t->state != TEAM_MOVING && t->state != TEAM_RETURNING) return;

    int goal_r, goal_c;
    if (t->state == TEAM_RETURNING) {
        goal_r = sh->st_r; goal_c = sh->st_c;
    } else {
        goal_r = t->target_r; goal_c = t->target_c;
    }

    if (t->state == TEAM_MOVING) {
        int tidx = goal_r * sh->cols + goal_c;
        if (sh->state[tidx] != ST_BURNING) {
            Task *task = find_task(sh, t->target_task_id);
            if (task) {
                task->state = (sh->state[tidx] == ST_BURNT)
                              ? TASK_CANCELLED : TASK_DONE;
                task->completed_tick = sh->tick;
                log_shared_add(ctx, sh->tick,
                        "Задача #%d закрыта: (%d,%d), группа #%d разворачивается",
                        task->id, task->r + 1, task->c + 1, t->id);
            }
            t->target_task_id = 0;
            t->target_r = -1;
            t->target_c = -1;
            t->progress = 0.0;

            int nr, nc;
            if (find_nearby_fire(sh, t->r, t->c, &nr, &nc)) {
                Task nt;
                nt.id = sh->next_task_id++;
                nt.r = nr; nt.c = nc;
                nt.state = TASK_IN_PROGRESS;
                nt.assigned_team_id = t->id;
                nt.created_tick = sh->tick;
                nt.completed_tick = 0;
                push_task(ctx, &nt);
                t->target_task_id = nt.id;
                t->target_r = nr; t->target_c = nc;
                t->state = TEAM_MOVING;
                log_shared_add(ctx, sh->tick,
                        "Группа #%d берёт новую цель (%d,%d)",
                        t->id, nr + 1, nc + 1);
            } else {
                if (sh->cfg.team_return_to_station) t->state = TEAM_RETURNING;
                else                                t->state = TEAM_IDLE;
            }
            return;
        }
    }

    if (t->r == goal_r && t->c == goal_c) {
        if (t->state == TEAM_RETURNING) {
            t->state = TEAM_IDLE;
            log_shared_add(ctx, sh->tick,
                    "Группа #%d вернулась на станцию", t->id);
        }
        return;
    }

    if (t->state == TEAM_MOVING) {
        int adj = abs_i(t->r - goal_r) <= 1 && abs_i(t->c - goal_c) <= 1;
        int tb = sh->state[goal_r * sh->cols + goal_c] == ST_BURNING;
        if (adj && tb) return;
    }

    int steps = sh->cfg.team_speed;
    if (steps < 1) steps = 1;
    for (int s = 0; s < steps; s++) {
        if (t->r == goal_r && t->c == goal_c) break;
        int adj = abs_i(t->r - goal_r) <= 1 && abs_i(t->c - goal_c) <= 1;
        int tb = sh->state[goal_r * sh->cols + goal_c] == ST_BURNING;
        if (t->state == TEAM_MOVING && adj && tb) break;

        int nr, nc;
        if (!bfs_next_step(sh, t->r, t->c, goal_r, goal_c, &nr, &nc)) break;

        team_mark_leave(sh, t);
        t->r = nr; t->c = nc;
        team_mark_enter(sh, t);
        log_shared_add(ctx, sh->tick,
                "Группа #%d: -> (%d,%d)", t->id, t->r + 1, t->c + 1);
    }
}

void teams_extinguish_one(Ctx *ctx, int team_idx) {
    SharedState *sh = ctx->sh;
    FireTeam *t = &sh->teams[team_idx];
    if (t->state != TEAM_MOVING && t->state != TEAM_EXTINGUISHING) return;

    Task *task = find_task(sh, t->target_task_id);

    if (task) {
        int idx = t->target_r * sh->cols + t->target_c;
        if (sh->state[idx] != ST_BURNING) {
            task->state = (sh->state[idx] == ST_BURNT)
                          ? TASK_CANCELLED : TASK_DONE;
            task->completed_tick = sh->tick;
            t->target_task_id = 0;
            t->target_r = -1;
            t->target_c = -1;
            t->progress = 0.0;
            task = NULL;

            int nr, nc;
            if (find_nearby_fire(sh, t->r, t->c, &nr, &nc)) {
                Task nt;
                nt.id = sh->next_task_id++;
                nt.r = nr; nt.c = nc;
                nt.state = TASK_IN_PROGRESS;
                nt.assigned_team_id = t->id;
                nt.created_tick = sh->tick;
                nt.completed_tick = 0;
                push_task(ctx, &nt);
                t->target_task_id = nt.id;
                t->target_r = nr; t->target_c = nc;
                t->state = TEAM_MOVING;
                log_shared_add(ctx, sh->tick,
                        "Группа #%d берёт следующую цель (%d,%d)",
                        t->id, nr + 1, nc + 1);
            } else {
                if (sh->cfg.team_return_to_station) t->state = TEAM_RETURNING;
                else                                t->state = TEAM_IDLE;
            }
            return;
        }
    }

    if (!task) return;

    int idx = t->target_r * sh->cols + t->target_c;
    int adj = abs_i(t->r - t->target_r) <= 1 &&
              abs_i(t->c - t->target_c) <= 1;
    if (!adj) return;

    if (t->state == TEAM_MOVING) {
        t->state = TEAM_EXTINGUISHING;
        task->state = TASK_IN_PROGRESS;
        log_shared_add(ctx, sh->tick,
                "Группа #%d тушит (%d,%d)",
                t->id, t->target_r + 1, t->target_c + 1);
    }

    t->progress += sh->cfg.team_efficiency;
    log_shared_add(ctx, sh->tick,
            "Группа #%d тушит (%d,%d): %.1f / %d",
            t->id, t->target_r + 1, t->target_c + 1,
            t->progress, sh->cfg.team_extinguish_time);

    if (t->progress >= (double)sh->cfg.team_extinguish_time) {
        sh->state[idx] = ST_EXTINGUISHED;
        sh->burn_age[idx] = -1;

        task->state = TASK_DONE;
        task->completed_tick = sh->tick;
        log_shared_add(ctx, sh->tick,
                "Группа #%d потушила (%d,%d)",
                t->id, t->target_r + 1, t->target_c + 1);

        t->target_task_id = 0;
        t->target_r = -1;
        t->target_c = -1;
        t->progress = 0.0;

        int nr, nc;
        if (find_nearby_fire(sh, t->r, t->c, &nr, &nc)) {
            Task nt;
            nt.id = sh->next_task_id++;
            nt.r = nr; nt.c = nc;
            nt.state = TASK_IN_PROGRESS;
            nt.assigned_team_id = t->id;
            nt.created_tick = sh->tick;
            nt.completed_tick = 0;
            push_task(ctx, &nt);
            t->target_task_id = nt.id;
            t->target_r = nr; t->target_c = nc;
            t->state = TEAM_MOVING;
            log_shared_add(ctx, sh->tick,
                    "Группа #%d берёт следующую цель (%d,%d)",
                    t->id, nr + 1, nc + 1);
        } else {
            if (sh->cfg.team_return_to_station) t->state = TEAM_RETURNING;
            else                                t->state = TEAM_IDLE;
        }
    }
}

void teams_observe(Ctx *ctx, unsigned int *rng) {
    SharedState *sh = ctx->sh;
    int R = sh->rows, C = sh->cols;
    const int RAD = 2;
    for (int i = 0; i < sh->teams_count; i++) {
        FireTeam *t = &sh->teams[i];
        int reported = 0;
        for (int dr = -RAD; dr <= RAD && !reported; dr++) {
            for (int dc = -RAD; dc <= RAD; dc++) {
                if (dr == 0 && dc == 0) continue;
                int nr = t->r + dr, nc = t->c + dc;
                if (nr < 0 || nr >= R || nc < 0 || nc >= C) continue;
                if (sh->state[nr * C + nc] != ST_BURNING) continue;
                if (t->last_report_r == nr && t->last_report_c == nc) continue;
                messages_send(ctx, (int)(sh->sensors_count - 1), nr, nc, rng);
                t->last_report_r = nr;
                t->last_report_c = nc;
                reported = 1;
                break;
            }
        }
    }
}