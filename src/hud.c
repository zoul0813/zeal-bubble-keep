/*
 * HUD text and icons use this game's shared ASCII_MAP.
 */
#include <core.h>
#include "game.h"
#include <zgdk/ascii.h>

void hud_init(void) {
    // This game's font occupies tiles 32-95; unsupported characters are blank.
    mem_set(ASCII_MAP, 32, sizeof(ASCII_MAP));
    ascii_map(' ', 64, 32);
    ascii_map(HUD_LIFE_ICON, 1, TILE_LIFE);
    ascii_map(HUD_HEALTH_FULL, 1, TILE_HP);
    ascii_map(HUD_HEALTH_EMPTY, 1, TILE_HP_OFF);
}

void text_print(const char *str, uint8_t x, uint8_t y) {
    uint8_t len = 0;
    while (str[len]) len++;
    hud_tilemap_nput_s_layer(&vctx, str, len, ROOM_LAYER, x, y);
}

void text_center(const char *str, uint8_t y) {
    uint8_t len = 0;
    while (str[len]) len++;
    hud_tilemap_nput_s_layer(&vctx, str, len, ROOM_LAYER, (WIDTH - len) >> 1, y);
}

void text_clear(void) {
    tilemap_fill(&vctx, ROOM_LAYER, TILE_EMPTY, 0, 0, WIDTH, HEIGHT);
}

/* 5 digit, zero padded. zgdk_itoa takes an int, so scores above 32767 would print negative.
 * Subtracting powers of ten: % and / call __moduint/__divuint, ~4% of a frame when the score changes. */
static const uint16_t POW10[4] = { 10000, 1000, 100, 10 };
static void format5(uint16_t value, char *out) {
    uint8_t i;
    for (i = 0; i < 4; i++) {
        char c = '0';
        while (value >= POW10[i]) { value -= POW10[i]; c++; }
        out[i] = c;
    }
    out[4] = '0' + (uint8_t)value;
    out[5] = 0;
}

/* `digits` (1-5) right-aligned digits of `value`, zero padded */
void text_number(uint16_t value, uint8_t digits, uint8_t x, uint8_t y) {
    char text[6];
    format5(value, text);
    hud_tilemap_nput_s_layer(&vctx, text + 5 - digits, digits, ROOM_LAYER, x, y);
}

void score_add(uint16_t points) {
    // hiscore_t.score is 16-bit, so the whole game caps at 65535
    if ((uint16_t)(game.score + points) < game.score) game.score = 0xFFFF;
    else game.score += points;
    if (game.score > game.hi && !game.demo) game.hi = game.score;
    if (game.score >= game.extend_at) {
        // extra life; past the last one that fits in 16 bits there are no more
        game.extend_at = (game.extend_at > 0xFFFF - EXTEND_EVERY) ? 0xFFFF : game.extend_at + EXTEND_EVERY;
        if (game.lives < LIVES_MAX) game.lives++;
        game.extended = 1;
    }
    game.hud_dirty = 1;
}

void hud_draw(void) {
    // "SSSSS @L R01  HHHHH": score, lives, round, hiscore
    char line[WIDTH + 1];
    uint8_t i;
    uint8_t round = game.round + 1;
    for (i = 0; i < WIDTH; i++) line[i] = ' ';
    line[WIDTH] = 0;
    format5(game.score, line);
    line[5] = ' ';
    line[6] = HUD_LIFE_ICON;
    line[7] = '0' + (game.lives > 9 ? 9 : game.lives);
    line[9] = 'R';
    line[10] = '0' + (round / 10);
    line[11] = '0' + (round % 10);
    if (game.demo) {
        line[14] = 'D'; line[15] = 'E'; line[16] = 'M'; line[17] = 'O';
        hud_tilemap_nput_s_layer(&vctx, line, WIDTH, ROOM_LAYER, 0, HUD_ROW);
    } else if (game.boss_max) {
        // boss round: the Slime King's health instead of the top score
        uint8_t lit = (uint8_t)(((uint16_t)game.boss_hp * 6 + game.boss_max - 1) / game.boss_max);
        line[13] = 'K';
        for (i = 0; i < 6; i++) line[14 + i] = i < lit ? HUD_HEALTH_FULL : HUD_HEALTH_EMPTY;
        hud_tilemap_nput_s_layer(&vctx, line, WIDTH, ROOM_LAYER, 0, HUD_ROW);
    } else {
        line[13] = 'H';
        line[14] = 'I';
        format5(game.hi, &line[15]);
        hud_tilemap_nput_s_layer(&vctx, line, WIDTH, ROOM_LAYER, 0, HUD_ROW);
    }
    game.hud_dirty = 0;
}
