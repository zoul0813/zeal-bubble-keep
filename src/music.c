/*
 * Background music: ZMT tracks (tools/make_music.py) played with ZGDK's tracker.
 *
 * Only one track is loaded at a time. A pattern_t takes 772 bytes of RAM, so all tracks share
 * three pattern buffers, and a track is read from bubblekeep.zar into zmt_buf and parsed by
 * zmt_rom_load() when it changes. Until the first track plays, the pattern buffers are the
 * tileset's load buffer (bgm_scratch()). ZGDK's device loader is private (zmt_file_load() only
 * takes a path); a public zmt_dev_load(track, dev) would stream the track from the archive instead.
 * The tracks only use voices 0-2; voice 3 is left to sfx.c.
 */
#include "game.h"
#include "assets.h"

#define MAX_PATTERNS 3
#define HURRY_TEMPO  16   // frames per quarter note during HURRY UP (the theme is 24)

#define ZMT_MAX 512   // the largest track, keep.zmt, is 457 bytes

typedef struct {
    const char *name;   // entry in bubblekeep.zar
    uint8_t loop;
} song_t;

static const song_t SONGS[] = {
    [BGM_TITLE]    = { "title.zmt", 1 },
    [BGM_KEEP]     = { "keep.zmt", 1 },
    [BGM_READY]    = { "ready.zmt", 0 },
    [BGM_CLEAR]    = { "clear.zmt", 0 },
    [BGM_GAMEOVER] = { "gameover.zmt", 0 },
};

static uint8_t zmt_buf[ZMT_MAX];
static pattern_t patterns[MAX_PATTERNS];

static track_t track;

/* zmt_sound_off() mutes every voice, the sound effect voice too */
static void sound_off(void) {
    zmt_sound_off();
    sfx_restore();
}
static uint8_t current = BGM_NONE;
static uint8_t playing;

void *bgm_scratch(uint16_t *size) {
    *size = sizeof(patterns);
    return patterns;
}

zos_err_t bgm_init(void) {
    uint8_t i;
    for (i = 0; i < MAX_PATTERNS; i++) track.patterns[i] = &patterns[i];
    zmt_reset(VOL_75);
    return ERR_SUCCESS;
}

void bgm_play(uint8_t song) {
    const song_t *s = &SONGS[song];
    sound_off();
    if (song != current) {
        // loading reinitialises every pattern; it only happens between screens
        uint16_t size = sizeof(zmt_buf);
        if (zar_entry_load(&pack, s->name, zmt_buf, &size) != ERR_SUCCESS ||
            zmt_rom_load(&track, zmt_buf, size) != ERR_SUCCESS) {
            current = BGM_NONE;
            playing = 0;
            return;
        }
        current = song;
    }
    zmt_track_reset(&track, 1);
    playing = 1;
}

void bgm_stop(void) {
    playing = 0;
    sound_off();
}

/* Pause keeps the playhead; bgm_resume() carries on from the next step */
void bgm_pause(void) {
    if (!playing) return;
    playing = 2;
    sound_off();
}

void bgm_resume(void) {
    if (playing == 2) playing = 1;
}

void bgm_hurry(void) {
    track.current_tempo = HURRY_TEMPO;
}

uint8_t bgm_busy(void) {
    return playing == 1;
}

/* Once per frame */
void bgm_tick(void) {
    if (playing != 1) return;
    // a one-shot ends when the last step has played and the playhead wraps to step 0
    if (zmt_tick(&track, 1) == 0 && !SONGS[current].loop) bgm_stop();
}
