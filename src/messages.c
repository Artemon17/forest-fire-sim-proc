#define _POSIX_C_SOURCE 200809L

#include "model.h"

#include <stdlib.h>
#include <string.h>

static double rnd01(unsigned int *rng) {
    return (double)rand_r(rng) / ((double)RAND_MAX + 1.0);
}

static int rnd_range(unsigned int *rng, int lo, int hi) {
    if (hi < lo) { int t = lo; lo = hi; hi = t; }
    return lo + rand_r(rng) % (hi - lo + 1);
}

static int seen_has(const SharedState *sh, int id) {
    for (int i = 0; i < sh->seen_count; i++)
        if (sh->seen_ids[i] == id) return 1;
    return 0;
}

static int seen_add(SharedState *sh, int id) {
    if (sh->seen_count >= SHM_MAX_SEEN_IDS) return -1;
    sh->seen_ids[sh->seen_count++] = id;
    return 0;
}

void messages_send(Ctx *ctx, int sensor_idx, int r, int c, unsigned int *rng) {
    SharedState *sh = ctx->sh;
    const Sensor *s = &sh->sensors[sensor_idx];

    if (rnd01(rng) < sh->cfg.msg_loss_prob) {
        log_shared_add(ctx, sh->tick,
                "Датчик #%d: сообщение потеряно в канале", s->id);
        return;
    }

    int delay = rnd_range(rng, sh->cfg.msg_delay_min, sh->cfg.msg_delay_max);

    sync_mutex_lock(ctx->teams_lock);
    Message m;
    m.id = sh->next_message_id++;
    m.sensor_id     = s->id;
    m.origin_r      = r;
    m.origin_c      = c;
    m.created_tick  = sh->tick;
    m.delivery_tick = sh->tick + delay;
    m.is_duplicate  = 0;
    if (sh->in_flight_count < SHM_MAX_MESSAGES)
        sh->in_flight[sh->in_flight_count++] = m;
    sync_mutex_unlock(ctx->teams_lock);

    log_shared_add(ctx, sh->tick,
            "Датчик #%d шлёт сообщение #%d о (%d,%d), задержка %d",
            s->id, m.id, r + 1, c + 1, delay);

    if (rnd01(rng) < sh->cfg.msg_dup_prob) {
        sync_mutex_lock(ctx->teams_lock);
        Message dup = m;
        dup.is_duplicate = 1;
        if (sh->in_flight_count < SHM_MAX_MESSAGES)
            sh->in_flight[sh->in_flight_count++] = dup;
        sync_mutex_unlock(ctx->teams_lock);
        log_shared_add(ctx, sh->tick,
                "Сообщение #%d продублировано", m.id);
    }
}

void messages_tick(Ctx *ctx) {
    SharedState *sh = ctx->sh;
    int i = 0;
    while (i < sh->in_flight_count) {
        Message *m = &sh->in_flight[i];
        if (m->delivery_tick > sh->tick) { i++; continue; }

        Message copy = *m;

        if (seen_has(sh, copy.id)) {
            log_shared_add(ctx, sh->tick,
                    "Дубликат сообщения #%d отброшен", copy.id);
        } else {
            seen_add(sh, copy.id);
            if (sh->inbox_count < SHM_MAX_MESSAGES)
                sh->inbox[sh->inbox_count++] = copy;
            log_shared_add(ctx, sh->tick,
                    "Сообщение #%d доставлено в центр: (%d,%d)",
                    copy.id, copy.origin_r + 1, copy.origin_c + 1);
        }

        for (int j = i; j < sh->in_flight_count - 1; j++)
            sh->in_flight[j] = sh->in_flight[j + 1];
        sh->in_flight_count--;
    }
}