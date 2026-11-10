#ifndef MAP_H
#define MAP_H

/* Типы растительности/покрытия. */
typedef enum {
    TERRAIN_GRASS = 0,   /* " или пробел в файле */
    TERRAIN_TREE,        /* t */
    TERRAIN_WATER,       /* w */
    TERRAIN_FIREBREAK    /* _ */
} Terrain;

typedef struct {
    int r, c;
} Coord;

typedef struct {
    int rows, cols;
    Terrain *cells;                    /* rows*cols, row-major */

    Coord *ignition;
    int    ignition_count;

    Coord *sensors;
    int    sensors_count;

    Coord *station_candidates;
    int    station_candidates_count;
} MapData;

/* Загрузить карту из файла. 0 при успехе. */
int  map_load(const char *path, MapData *out);

/* Освободить всю память и обнулить структуру. */
void map_free(MapData *m);

/* Символ для отображения клетки. */
char terrain_symbol(Terrain t);

/* Разобрать символ: 0..3 = Terrain, -1 при ошибке. */
int  terrain_parse(char ch);

/* Печать карты и списков (для отладки). */
void map_dump(const MapData *m);

#endif