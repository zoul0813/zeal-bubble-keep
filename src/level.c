#include <zgdk/tilemap/flags.h>
/*
 * Tiled rounds: native ZTM tile bytes plus point-object spawns.
 * Shadows and flood animation remain game rendering effects.
 */
#include <core.h>
#include "game.h"
#include "assets.h"
#include "map_data.h"

TILEFLAGS_DEFINE_CACHE(15);

uint8_t level_spawn_x, level_spawn_y;

static uint8_t room_tiles[ROOM_SIZE];
static Tilemap tilemap;
static uint8_t tile_flags[256];
static zar_entry_t rounds_file, spawns_file;

zos_err_t level_init(void) {
    zos_err_t err = zar_entry_from_name(&pack, "rounds.ztm", &rounds_file);
    if (err) return err;
    return zar_entry_from_name(&pack, "rounds.spn", &spawns_file);
}

static void read_round(uint8_t round) {
    map_objects_t objects;
    uint8_t i;
    uint16_t size = ROOM_SIZE;
    if (zar_entry_read_at(&pack, &rounds_file, (uint16_t)round * ROOM_SIZE, room_tiles, &size) != ERR_SUCCESS || size != ROOM_SIZE)
        mem_set(room_tiles, TILE_EMPTY, ROOM_SIZE);
    size = sizeof(objects);
    if (zar_entry_read_at(&pack, &spawns_file, (uint16_t)round * sizeof(objects), &objects, &size) != ERR_SUCCESS ||
        size != sizeof(objects) || objects.count > sizeof(objects.objects) / sizeof(objects.objects[0])) return;
    for (i = 0; i < objects.count; i++) {
        const map_object_t *o = &objects.objects[i];
        if (o->x >= WIDTH || o->y >= HEIGHT) continue;
        switch (o->kind) {
            case MAP_PLAYER: level_spawn_x = o->x; level_spawn_y = o->y; break;
            case MAP_SLIME: actors_spawn_enemy(KIND_SLIME, o->x, o->y); break;
            case MAP_BAT: actors_spawn_enemy(KIND_BAT, o->x, o->y); break;
            case MAP_IMP: actors_spawn_enemy(KIND_IMP, o->x, o->y); break;
            case MAP_BOSS: actors_spawn_boss(o->x, o->y); break;
        }
    }
}

static uint8_t is_block(int8_t x, int8_t y) {
    if (x < 0 || y < 1 || x >= WIDTH || y >= HEIGHT) return 0;
    return room_tiles[(y * WIDTH) + x] != TILE_EMPTY;
}

static void draw_shadows(void) {
    uint8_t line[WIDTH];
    int8_t x, y;
    for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++) {
            uint8_t t = TILE_EMPTY;
            if (!is_block(x, y)) {
                uint8_t left = is_block(x - 1, y);
                uint8_t up = is_block(x, y - 1);
                if (left && up) t = TILE_SHADOW_RB;
                else if (left) t = TILE_SHADOW_R;
                else if (up) t = TILE_SHADOW_B;
                else if (is_block(x - 1, y - 1)) t = TILE_SHADOW_C;
            }
            line[x] = t;
        }
        gfx_tilemap_load(&vctx, line, WIDTH, LEVEL_LAYER, 0, y);
    }
}

void level_load(uint8_t round) {
    uint8_t i;

    for (i = 0; i < THEME_COUNT; i++) {
        tile_flags[TILE_BLOCK0 + i] = TF_SOLID;
        tile_flags[TILE_PLAT0 + i] = TF_ONEWAY;
    }
    tileflags_register(tile_flags);
    // the bottom gap leads out of the room (and back in at the top)
    tileflags_set_outside(TF_NONE);

    level_spawn_x = 2;
    level_spawn_y = 13;
    read_round(round);
    if (tilemap_load(&vctx, &tilemap, room_tiles, WIDTH, HEIGHT, ROOM_LAYER, 0, 0) == ERR_SUCCESS)
        tileflags_refresh();
    draw_shadows();
}

/* HURRY UP floods the keep from the bottom, one row at a time, behind the blocks (layer 0) */
void level_flood_rise(void) {
    uint8_t line[WIDTH];
    uint8_t y = game.flood - 1, x;
    for (x = 0; x < WIDTH; x++) line[x] = TILE_WATER_TOP;
    gfx_tilemap_load(&vctx, line, WIDTH, LEVEL_LAYER, 0, y);
    if (game.flood < HEIGHT) {
        for (x = 0; x < WIDTH; x++) line[x] = TILE_WATER;
        gfx_tilemap_load(&vctx, line, WIDTH, LEVEL_LAYER, 0, game.flood);
    }
    game.flood = y;
}

void level_flood_drain(void) {
    if (game.flood >= HEIGHT) return;
    game.flood = HEIGHT;
    draw_shadows();
}

/* Redraw rows of the current round (ie; after a text banner) without re-running the spawns */
void level_redraw_rows(uint8_t from, uint8_t count) {
    uint8_t y;
    for (y = from; y < from + count; y++) {
        gfx_tilemap_load(&vctx, room_tiles + (y * WIDTH), WIDTH, ROOM_LAYER, 0, y);
    }
}
