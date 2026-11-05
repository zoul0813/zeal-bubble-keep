#include <stdint.h>
#include <zos_errors.h>
#include <zvb_gfx.h>
/* only the ZGDK headers in use: <zgdk.h> also pulls rect.h, ascii.h and music.h, whose
 * non-inline static functions SDCC copies into every file that includes them */
#include <zgdk/types.h>
#include <zgdk/input.h>
#include <zgdk/sprites.h>
#include <zgdk/tilemap.h>
#include <zgdk/hiscore.h>
#include <zgdk/utils/rand.h>
#include <zgdk/hud/tilemap.h>
#include <zgdk/sound/sounds.h>
#include <zgdk/sound/tracker.h>
#include <zgdk/physics/body.h>
#include <zgdk/tilemap/flags.h>
#include "room.h"
#include <zgdk/sound/sfx.h>
#include <zgdk/input.h>
#include "gfx.h"
#include "entities.h"

#ifndef GAME_H
#define GAME_H

#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  240
#define WIDTH          20
#define HEIGHT         15

#define LEVEL_LAYER    0   // block shadows
#define ROOM_LAYER     1   // blocks (collision) + HUD on row 0
#define HUD_ROW        0

#define POOL_SIZE      40
#define ARENA_SIZE     POOL_SIZE

/* entity types */
#define E_HERO    0
#define E_SLIME   1
#define E_BAT     2
#define E_BUBBLE  3
#define E_KO      4   // knocked-out enemy flying through the room
#define E_FRUIT   5
#define E_PTS     6   // floating score
#define E_POP     7   // bubble burst effect
#define E_IMP     8   // walks, spits fireballs along its row
#define E_FIRE    9   // an imp's fireball
#define E_BOSS   10   // the Slime King (rounds 10, 20, 30)

/* enemy kinds kept inside a bubble or a KO corpse (game_entity_t.data low byte) */
#define KIND_NONE  0
#define KIND_SLIME 1
#define KIND_BAT   2
#define KIND_IMP   3

/* fruit data: 0-3 fruit, then the power-ups */
#define ITEM_CANDY 4   // rapid bubbles that fly further
#define ITEM_SHOE  5   // run faster
#define ITEM_STAR  6   // knocks out every enemy on screen

/* input */
#define IN_LEFT   BUTTON_LEFT
#define IN_RIGHT  BUTTON_RIGHT
#define IN_JUMP   BUTTON_B                // Z / Space, SNES B
#define IN_BLOW   (BUTTON_Y | BUTTON_A)   // X or A key, SNES Y or A
#define IN_START  BUTTON_START            // Enter, SNES Start
#define IN_QUIT   BUTTON_SELECT           // Right Shift, SNES Select

#ifndef HISCORE_PATH
#ifdef EMULATOR
#define HISCORE_PATH "H:/bkeep.zhs"   // HostFS: the game folder
#else
#define HISCORE_PATH "B:/bkeep.zhs"

#endif

#endif

#define ROUND_COUNT 30
#define LIVES_START 3
#define LIVES_MAX   9
#define EXTEND_FIRST 10000U   // extra life at 10000, then every 20000
#define EXTEND_EVERY 20000U

typedef struct {
    uint16_t score;
    uint16_t hi;
    uint8_t lives;
    uint8_t round;      // 0 based
    uint8_t loop;       // times all rounds were cleared, makes enemies faster
    uint8_t enemies;    // enemies still alive (walking, bubbled or flying KO)
    uint16_t round_timer;
    uint8_t hurry;
    uint8_t clear_timer;
    uint8_t hud_dirty;
    uint16_t extend_at;  // next extra life
    uint8_t extended;    // set by score_add(), actors show the 1UP
    uint8_t misses;      // lives lost this round (NO MISS bonus)
    uint8_t died;        // set when the hero dies, main drains the flood
    uint8_t flood;       // top water row, HEIGHT = no water
    uint8_t shake;       // frames of screen shake left
    uint8_t view_y;      // this frame's shake offset (sprites and both layers)
    uint8_t boss_hp;
    uint8_t boss_max;    // 0 = no boss this round
    uint8_t ko_count;    // knock-outs until the next power-up
    uint8_t item_next;   // which power-up comes next
    uint8_t demo;        // attract mode: the bot plays, no hiscore
} game_t;

extern gfx_context vctx;
extern game_t game;
extern uint16_t input_now;
extern uint16_t input_pressed;

/* rounds.c */

/* level.c */
zos_err_t level_init(void);   // after zar_file_open(PACK_FILE, &pack)
void level_load(uint8_t round);
void level_redraw_rows(uint8_t from, uint8_t count);
void level_flood_rise(void);    // water one row higher (game.flood)
void level_flood_drain(void);   // no water, shadows back
extern uint8_t level_spawn_x, level_spawn_y;

/* actors.c */
void actors_init(void);
void actors_round_start(void);
void actors_spawn_enemy(uint8_t kind, uint8_t tx, uint8_t ty);
void actors_update(void);
uint8_t actors_hero_dead(void);
void actors_make_all_angry(void);
void actors_cheat_clear(void);
void actors_spawn_boss(uint8_t tx, uint8_t ty);

/* hud.c */
void hud_draw(void);
/* Character glyphs mapped to icon tiles by hud_init(). */
#define HUD_LIFE_ICON '@'
#define HUD_HEALTH_FULL '%'
#define HUD_HEALTH_EMPTY '&'

void hud_init(void);
void text_print(const char *str, uint8_t x, uint8_t y);
void text_center(const char *str, uint8_t y);
void text_clear(void);
void score_add(uint16_t points);
void text_number(uint16_t value, uint8_t digits, uint8_t x, uint8_t y);

/* sfx.c (on ZGDK sound/sfx: sfx_tick() comes from there) */
void sfx_setup(void);
void sfx_jump(void);
void sfx_blow(void);
void sfx_pop(void);
void sfx_kill(uint8_t combo);
void sfx_fruit(void);
void sfx_die(void);
void sfx_bounce(void);
void sfx_trap(void);
void sfx_escape(void);
void sfx_spit(void);
void sfx_power(void);
void sfx_extend(void);
void sfx_boss_hit(void);
void sfx_boom(void);

/* autopilot.c: the bot (AUTOPILOT builds play whole games with it, normal builds use it for the attract demo) */
uint16_t autopilot_input(void);
void autopilot_reset(void);
game_entity_t *actors_hero(void);

/* music.c */
#define BGM_TITLE    0
#define BGM_KEEP     1
#define BGM_READY    2
#define BGM_CLEAR    3
#define BGM_GAMEOVER 4
#define BGM_NONE     0xFF
zos_err_t bgm_init(void);
void *bgm_scratch(uint16_t *size);   // the pattern buffers, free until the first bgm_play()
void bgm_play(uint8_t song);
void bgm_stop(void);
void bgm_pause(void);
void bgm_resume(void);
void bgm_hurry(void);
uint8_t bgm_busy(void);
void bgm_tick(void);


#endif
