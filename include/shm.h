#ifndef SHM_H
#define SHM_H

#include <stdatomic.h>
#include <stddef.h>

#define SHM_MAX_ROWS        64
#define SHM_MAX_COLS        64
#define SHM_MAX_CELLS       (SHM_MAX_ROWS * SHM_MAX_COLS)
#define SHM_MAX_SENSORS     32
#define SHM_MAX_TEAMS       16
#define SHM_MAX_TASKS       4096
#define SHM_MAX_MESSAGES    2048
#define SHM_MAX_SEEN_IDS    (SHM_MAX_MESSAGES * 4)

/* ─── Типы модели (без указателей) ───────────────────────── */

typedef enum { ST_SAFE = 0, ST_BURNING, ST_EXTINGUISHED, ST_BURNT } FireState;
typedef enum { OCC_NONE = 0, OCC_SENSOR, OCC_STATION, OCC_TEAM } Occupant;
typedef enum {
    TEAM_IDLE = 0, TEAM_MOVING, TEAM_EXTINGUISHING, TEAM_RETURNING
} TeamState;
typedef enum {
    TASK_PENDING = 0, TASK_ASSIGNED, TASK_IN_PROGRESS,
    TASK_DONE, TASK_CANCELLED
} TaskState;
typedef enum {
    TERRAIN_GRASS = 0, TERRAIN_TREE, TERRAIN_WATER, TERRAIN_FIREBREAK
} Terrain;

typedef struct { int id; int r, c; } Sensor;

typedef struct {
    int       id;
    int       r, c;
    TeamState state;
    int       target_r, target_c;
    int       target_task_id;
    double    progress;
    int       last_report_r, last_report_c;
} FireTeam;

typedef struct {
    int       id;
    int       r, c;
    TaskState state;
    int       assigned_team_id;
    int       created_tick, completed_tick;
} Task;

typedef struct {
    int id, sensor_id;
    int origin_r, origin_c;
    int created_tick, delivery_tick;
    int is_duplicate;
} Message;

typedef struct {
    double fire_base_prob_grass, fire_base_prob_tree;
    double fire_burn_time_factor;
    double fire_wind_factor_along, fire_wind_factor_against, fire_wind_factor_side;
    int    fire_spread_8dir, fire_reburn_burnt;

    int    wind_direction, wind_power;

    int    sensor_radius, sensor_resend_period;

    int    msg_delay_min, msg_delay_max;
    double msg_loss_prob, msg_dup_prob;

    int    team_count, team_speed, team_extinguish_time;
    double team_efficiency;
    int    team_return_to_station;
    int    team_can_cross_water, team_can_cross_firebreak;

    int    end_max_ticks, end_unlimited;
} SharedConfig;

typedef struct {
    int rows, cols;
    int st_r, st_c;
    unsigned int seed;

    Terrain  cells[SHM_MAX_CELLS];

    FireState state[SHM_MAX_CELLS];
    Occupant  occupant[SHM_MAX_CELLS];
    int       team_id[SHM_MAX_CELLS];
    int       burn_age[SHM_MAX_CELLS];
    _Atomic FireState next_state[SHM_MAX_CELLS];

    Sensor   sensors[SHM_MAX_SENSORS];
    int      sensors_count;
    FireTeam teams[SHM_MAX_TEAMS];
    int      teams_count;

    Task     tasks[SHM_MAX_TASKS];
    int      tasks_count;
    int      next_task_id;

    Message  in_flight[SHM_MAX_MESSAGES];
    int      in_flight_count;
    Message  inbox[SHM_MAX_MESSAGES];
    int      inbox_count;
    int      next_message_id;

    int      seen_ids[SHM_MAX_SEEN_IDS];
    int      seen_count;

    unsigned char sensor_reported[SHM_MAX_SENSORS * SHM_MAX_CELLS];

    SharedConfig cfg;

    int        tick;
    atomic_int stop;
} SharedState;

SharedState *shm_create(const char *name);   /* main */
SharedState *shm_attach(const char *name);   /* дети */
void         shm_detach(SharedState *sh);
void         shm_destroy(const char *name);  /* main при выходе */

#endif