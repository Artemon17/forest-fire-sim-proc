#define _POSIX_C_SOURCE 200809L

#include "model.h"

#include <string.h>

static int abs_i(int x) { return x < 0 ? -x : x; }

static int has_active_task(const SharedState *sh, int r, int c) {
    for (int i = 0; i < sh->tasks_count; i++) {
        const Task *t = &sh->tasks[i];
        if (t->r != r || t->c != c) continue;
        if (t->state == TASK_PENDING ||
            t->state == TASK_ASSIGNED ||
            t->state == TASK_IN_PROGRESS) return 1;
    }
    return 0;
}

static void center_consume_inbox(Ctx *ctx) {
    SharedState *sh = ctx->sh;

    for (int i = 0; i < sh->inbox_count; i++) {
        Message *m = &sh->inbox[i];
        int r = m->origin_r, c = m->origin_c;

        if (sh->state[r * sh->cols + c] != ST_BURNING) {
            log_shared_add(ctx, sh->tick,
                    "Центр: (%d,%d) уже не горит, сообщение #%d отброшено",
                    r + 1, c + 1, m->id);
            continue;
        }

        if (has_active_task(sh, r, c)) {
            log_shared_add(ctx, sh->tick,
                    "Центр: (%d,%d) уже в работе, сообщение #%d отброшено",
                    r + 1, c + 1, m->id);
            continue;
        }

        if (sh->tasks_count >= SHM_MAX_TASKS) continue;

        Task t;
        t.id               = sh->next_task_id++;
        t.r                = r;
        t.c                = c;
        t.state            = TASK_PENDING;
        t.assigned_team_id = 0;
        t.created_tick     = sh->tick;
        t.completed_tick   = 0;
        sh->tasks[sh->tasks_count++] = t;

        log_shared_add(ctx, sh->tick,
                "Центр: создана задача #%d на очаг (%d,%d)",
                t.id, r + 1, c + 1);
    }

    sh->inbox_count = 0;
}

static void center_assign_teams(Ctx *ctx) {
    SharedState *sh = ctx->sh;

    for (int i = 0; i < sh->tasks_count; i++) {
        Task *t = &sh->tasks[i];
        if (t->state != TASK_PENDING) continue;

        int idx = t->r * sh->cols + t->c;
        if (sh->state[idx] != ST_BURNING) {
            t->state = TASK_CANCELLED;
            t->completed_tick = sh->tick;
            log_shared_add(ctx, sh->tick,
                    "Центр: задача #%d отменена — (%d,%d) не горит",
                    t->id, t->r + 1, t->c + 1);
            continue;
        }

        int best_j = -1, best_d = 1 << 30;
        for (int j = 0; j < sh->teams_count; j++) {
            FireTeam *tm = &sh->teams[j];
            if (tm->target_task_id != 0) continue;
            int d = abs_i(tm->r - t->r) + abs_i(tm->c - t->c);
            if (d < best_d) { best_d = d; best_j = j; }
        }

        if (best_j < 0) continue;

        FireTeam *tm = &sh->teams[best_j];
        tm->target_task_id = t->id;
        tm->target_r       = t->r;
        tm->target_c       = t->c;
        tm->state          = TEAM_MOVING;

        t->state            = TASK_ASSIGNED;
        t->assigned_team_id = tm->id;

        log_shared_add(ctx, sh->tick,
                "Центр: задача #%d -> группа #%d (расстояние %d)",
                t->id, tm->id, best_d);
    }
}

void center_tick(Ctx *ctx) {
    center_consume_inbox(ctx);
    center_assign_teams(ctx);
}