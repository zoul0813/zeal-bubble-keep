/*
 * Bubble Keep: single-screen arena platformer for Zeal 8-bit OS.
 * Blow bubbles to trap slimes and bats, pop them to send them flying into the others.
 */
#include <zos_vfs.h>
#include <stdint.h>
#include <zos_errors.h>
#include <zos_sys.h>
#include <zos_video.h>
#include "game.h"
#include "controls.h"
#include "entities.h"
#include "assets.h"
#include <zgdk/sprites.h>
#include <zgdk/tilemap/scroll.h>
#ifdef DEBUG_FPS
/* game loop counter on the bottom row; compare two screenshots taken a known time apart */
uint16_t fps_frames;
static void fps_tick(void) {
    char text[6];
    uint16_t v = ++fps_frames;
    int8_t i;
    if (v & 63) return;   // tools/hfps.py reads the counter from memory, the print is for eyes only
    for (i = 4; i >= 0; i--) { text[i] = '0' + (v % 10); v /= 10; }
    text[5] = 0;
    text_print(text, 7, HEIGHT - 1);
}
#endif

#define ROUND_INTRO_FRAMES  90
#define ROUND_CLEAR_FRAMES  150
#define HURRY_FRAMES        (45U * 60U)
#define HURRY_SHOW_FRAMES   120
#ifndef START_ROUND
#define START_ROUND         0
#endif
#define GAME_OVER_FRAMES    200   // the game over jingle is 192 frames
#define FLOOD_STEP          240   // after HURRY UP the water rises a row every 4 s
#define FLOOD_TOP           8     // up to row 8: the top half stays dry
#define DEMO_AFTER          900   // title frames before the attract demo
#define DEMO_FRAMES         (40U * 60U)

gfx_context vctx;
game_t game;
entity_pool_t game_entities;
uint16_t input_now;
uint16_t input_pressed;
static uint16_t input_last;
static uint8_t autopilot_on;   // the bot plays: the attract demo, or every game in AUTOPILOT builds
static uint8_t demo_quit;      // a key was pressed during the demo
static uint8_t demo_count;
static uint8_t scroll_y;

static const highscore_t DEFAULT_SCORES[HISCORES_COUNT] = {
    { { 'Z', 'E', 'L' }, 20000 }, { { 'B', 'U', 'B' }, 15000 }, { { 'K', 'E', 'P' }, 12000 },
    { { 'S', 'L', 'M' }, 10000 }, { { 'B', 'A', 'T' }, 8000 },  { { 'Z', 'G', 'D' }, 6000 },
    { { 'Z', 'V', 'B' }, 4000 },  { { 'Z', 'O', 'S' }, 3000 },  { { 'Z', 'D', 'E' }, 2000 },
    { { 'P', 'O', 'P' }, 1000 },
};

static const hiscore_config_t HISCORE_CONFIG = {
    .context = &vctx,
    .width = WIDTH,
    .height = HEIGHT,
    .layer = ROOM_LAYER,
    .empty_tile = TILE_EMPTY,
    .default_scores = DEFAULT_SCORES,
    .path = HISCORE_PATH,
};

static void fatal(zos_err_t err, const char *what) {
    // printf would pull in ~2.6 KB of formatting code, and RAM is tight
    uint16_t len = 0;
    char code[4] = { ' ', '0' + (err / 10) % 10, '0' + err % 10, '\n' };
    ioctl(DEV_STDOUT, CMD_RESET_SCREEN, NULL);
    while (what[len]) len++;
    write(DEV_STDOUT, what, &len);
    len = sizeof(code);
    write(DEV_STDOUT, code, &len);
    exit(err);
}

static void read_input(void) {
    input_now = input_read();
    if (autopilot_on) {
        if (game.demo && input_now) demo_quit = 1;
        input_now = (input_now & IN_START) | autopilot_input();
    }
    input_pressed = input_now & ~input_last;
    input_last = input_now;
}

static void frame_end(void) {
    gfx_wait_vblank(&vctx);
    sprites_render(&vctx);
    if (game.view_y != scroll_y) {
        // screen shake: both layers move with the sprites (entity_view_set in actors.c)
        scroll_y = game.view_y;
        tilemap_scroll(LEVEL_LAYER, 0, scroll_y);
        tilemap_scroll(ROOM_LAYER, 0, scroll_y);
    }
    if (game.hud_dirty) hud_draw();
    gfx_wait_end_vblank(&vctx);
    bgm_tick();
    sfx_tick();
}

static void wait_frames(uint8_t frames) {
    while (frames--) {
        read_input();
        frame_end();
    }
}

static void init(void) {
    zos_err_t err;

    err = input_init(1);
    if (err) fatal(err, "input");

    gfx_enable_screen(0);
    err = gfx_initialize(ZVB_CTRL_VID_MODE_GFX_320_8BIT, &vctx);
    if (err) fatal(err, "gfx");
    err = sprites_clear(&vctx);   // a previous program may have left sprites in the table
    if (err) fatal(err, "sprites");
    err = zar_file_open(PACK_FILE, &pack);   // bubblekeep.zar, next to the .bin
    if (err) fatal(err, PACK_FILE);
    err = level_init();
    if (err) fatal(err, "rounds");
    err = load_palette(&vctx);
    if (err) fatal(err, "palette");
    gfx_tileset_options options = { .compression = TILESET_COMP_LZ };
    err = load_tileset(&vctx, &options);
    if (err) fatal(err, "tileset");
    hud_init();
    tilemap_fill(&vctx, LEVEL_LAYER, TILE_EMPTY, 0, 0, 80, 40);
    tilemap_fill(&vctx, ROOM_LAYER, TILE_EMPTY, 0, 0, 80, 40);

    actors_init();
    sound_init_hardware();       // ZGDK SFX shares the sound hardware; this resets every voice, so it goes first
    err = bgm_init();
    if (err) fatal(err, "music");
    sfx_setup();
    hiscore_init(&HISCORE_CONFIG);
    rand8_seed(0x5EED);

    gfx_enable_screen(1);
}

static void deinit(void) {
    bgm_stop();
    sound_deinit();
    zar_file_close(&pack);
    (void)sprites_clear(&vctx);   // best-effort cleanup before returning to ZOS
    ioctl(DEV_STDOUT, CMD_RESET_SCREEN, NULL);
}

/* Title screen, alternating with the hiscore table. Returns 0 to quit, 1 to play, 2 for the attract demo. */
static uint8_t title(void) {
    uint16_t t = 0;
    uint8_t page = 0xFF;
    uint16_t seed = 0;

    entity_pool_clear(&game_entities);
    game.view_y = 0;
    game.shake = 0;
    entity_view_set(0, 0);
    physics_entities_sync(&game_entities);
    tilemap_fill(&vctx, LEVEL_LAYER, TILE_EMPTY, 0, 0, WIDTH, HEIGHT);
    game.hud_dirty = 0;
    input_flush();
    controls_menu_begin();
    bgm_play(BGM_TITLE);

    while (1) {
        uint8_t want = (t / 300) & 1;
        if (want != page) {
            page = want;
            if (page == 0) {
                text_clear();
                text_center("BUBBLE KEEP", 2);
                text_center("TRAP THEM IN BUBBLES", 5);
                text_center("POP THEM INTO", 6);
                text_center("EACH OTHER", 7);
                text_center(CONTROL_PROMPT("Z JUMP  X BLOW", PAD_B " JUMP  " PAD_A " BLOW"), 10);
                text_center(PROMPT_START, 12);
            } else {
                hiscore_show();
                text_center("HIGH SCORES", 1);
                text_center(PROMPT_START, 14);
            }
        }
        read_input();
        if (controls_menu_update()) page = 0xFF;
        seed++;
        if (input_pressed & IN_START) break;
        if (input_pressed & IN_QUIT) return 0;
        frame_end();
#ifndef AUTOPILOT
        if (++t == DEMO_AFTER) {
            rand8_seed(seed | 1);
            return 2;
        }
#else
        t++;
#endif
    }
    rand8_seed(seed | 1);
    return 1;
}

static void round_start(void) {
    char text[] = "ROUND 00";
    uint8_t round = game.round + 1;

    entity_pool_clear(&game_entities);
    text_clear();
    game.flood = HEIGHT;
    game.misses = 0;
    game.died = 0;
    game.shake = 0;
    game.boss_max = 0;   // a 'K' in the round sets it
    level_load(game.round);
    actors_round_start();
    game.round_timer = 0;
    game.hurry = 0;
    game.clear_timer = 0;
    game.hud_dirty = 1;
    actors_update();   // place sprites once, then hold them still for the intro

    text[6] = '0' + (round / 10);
    text[7] = '0' + (round % 10);
    text_center(text, 7);
    text_center(game.boss_max ? "THE SLIME KING!" : "READY", 8);
    bgm_play(BGM_READY);
    wait_frames(ROUND_INTRO_FRAMES);
    bgm_play(BGM_KEEP);
    // restore the two rows the banner covered
    level_redraw_rows(7, 2);
    game.hud_dirty = 1;
}

static void pause(void) {
    text_center("PAUSED", 7);
    bgm_pause();
    input_flush();
    input_last = 0;
    do {
        read_input();
        frame_end();
    } while (!(input_pressed & IN_START));
    level_redraw_rows(7, 1);
    bgm_resume();
}

/* Round cleared: time bonus, plus 500 when no life was lost in the round (scores are 16-bit: keep bonuses modest) */
static void clear_bonus(void) {
    uint16_t secs = game.round_timer / 60;
    uint16_t bonus = secs < 20 ? 1000 : secs < 30 ? 500 : secs < 45 ? 200 : 0;
    text_center("ROUND CLEAR!", 5);
    text_print("TIME", 5, 7);
    text_number(secs > 99 ? 99 : secs, 2, 13, 7);
    if (game.misses == 0) {
        bonus += 500;
        text_center("NO MISS!", 8);
    }
    text_print("BONUS", 5, 9);
    text_number(bonus, 5, 10, 9);
    if (bonus) score_add(bonus);
}

static const uint8_t DEMO_ROUNDS[3] = { 0, 9, 5 };   // first steps, the Slime King, imp alley

static void play(uint8_t demo) {
    uint8_t hurry_show = 0;
    uint8_t over_timer = 0;
    uint16_t flood_timer = 0;
    uint16_t demo_timer = 0;

    game.score = 0;
    game.lives = LIVES_START;
    game.round = START_ROUND;
    game.loop = 0;
    game.extend_at = EXTEND_FIRST;
    game.ko_count = 6;
    game.item_next = 0;
    game.demo = demo;
    demo_quit = 0;
    if (demo) game.round = DEMO_ROUNDS[demo_count++ % 3];
#ifdef AUTOPILOT
    autopilot_on = 1;
#else
    autopilot_on = demo;
#endif
    autopilot_reset();
    {
        uint16_t top = hiscore_get(0)->score;
        game.hi = top ? top : DEFAULT_SCORES[0].score;
    }
    round_start();

    while (1) {
    
        read_input();
        if ((input_pressed & IN_START) && !game.demo) pause();
#ifdef CHEATS
        if (input_pressed & BUTTON_L) actors_cheat_clear();
#endif

        actors_update();
    

        game.round_timer++;
        if (game.round_timer == HURRY_FRAMES && game.enemies) {
            game.hurry = 1;
            actors_make_all_angry();
            bgm_hurry();
            text_center("HURRY UP!", 7);
            hurry_show = HURRY_SHOW_FRAMES;
        }
        if (hurry_show && --hurry_show == 0) {
            level_redraw_rows(7, 1);
        }
        // the flood: rises while enemies are left, drains when the hero loses a life
        if (game.died) {
            game.died = 0;
            game.misses++;
            level_flood_drain();
            flood_timer = 0;
        }
        if (game.hurry && game.enemies && !actors_hero_dead() && game.flood > FLOOD_TOP && ++flood_timer >= FLOOD_STEP) {
            flood_timer = 0;
            level_flood_rise();
        }
        if (game.demo && (demo_quit || ++demo_timer >= DEMO_FRAMES)) break;

        if (actors_hero_dead() && game.lives == 0) {
            if (over_timer == 0) {
                text_center("GAME OVER", 7);
                bgm_play(BGM_GAMEOVER);
            }
            if (++over_timer > GAME_OVER_FRAMES) break;
        } else if (game.enemies == 0) {
            if (game.clear_timer == 0) {
                bgm_play(BGM_CLEAR);
                clear_bonus();
            }
            if (++game.clear_timer >= ROUND_CLEAR_FRAMES) {
                game.round++;
                if (game.round >= ROUND_COUNT) {
                    game.round = 0;
                    game.loop++;
                }
                round_start();
            }
        }

#ifdef DEBUG_FPS
        fps_tick();
#endif
        frame_end();
    }

    entity_pool_clear(&game_entities);
    game.view_y = 0;
    game.shake = 0;
    entity_view_set(0, 0);
    physics_entities_sync(&game_entities);
    tilemap_fill(&vctx, LEVEL_LAYER, TILE_EMPTY, 0, 0, WIDTH, HEIGHT);   // hiscore_show only clears its own layer
    bgm_stop();
    autopilot_on = 0;
    frame_end();
    if (game.demo) {
        game.demo = 0;
        return;
    }
    input_flush();
    hiscore_add(game.score);
}

int main(void) {
    uint8_t mode;
    init();
    while ((mode = title())) play(mode == 2);
    deinit();
    return 0;
}
