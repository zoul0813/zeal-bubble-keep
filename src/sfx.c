/*
 * Sound effects: tables for ZGDK's sequencer (zgdk/sound/sfx.h) on voice 3.
 * Voices 0-2 belong to the ZMT music (music.c), which never writes voice 3.
 * Priorities keep a jump from cutting a combo (sfx_play_prio).
 */
#include "game.h"

#define SFX_VOLUME VOL_100

static const sfx_seg S_JUMP[]   = { SFX_SEG(WAV_SQUARE, 330, 700, 8), SFX_END };
static const sfx_seg S_BOUNCE[] = { SFX_SEG(WAV_SQUARE, 520, 1040, 6), SFX_END };
static const sfx_seg S_BLOW[]   = { SFX_SEG(WAV_NOISE, 6000, 6000, 2), SFX_SEG(WAV_TRIANGLE, 700, 380, 6), SFX_END };
static const sfx_seg S_TRAP[]   = { SFX_SEG(WAV_TRIANGLE, 500, 900, 6), SFX_SEG(WAV_TRIANGLE, 1200, 1200, 4), SFX_END };
static const sfx_seg S_POP[]    = { SFX_SEG(WAV_NOISE, 3000, 1500, 4), SFX_END };
/* the knock-out arpeggio climbs a minor third with every link of a chain */
static const sfx_seg S_KILL0[]  = { SFX_SEG(WAV_SQUARE, 523, 523, 3), SFX_SEG(WAV_SQUARE, 659, 659, 3),
                                    SFX_SEG(WAV_SQUARE, 784, 784, 3), SFX_SEG(WAV_SQUARE, 1047, 1200, 6), SFX_END };
static const sfx_seg S_KILL1[]  = { SFX_SEG(WAV_SQUARE, 622, 622, 3), SFX_SEG(WAV_SQUARE, 784, 784, 3),
                                    SFX_SEG(WAV_SQUARE, 932, 932, 3), SFX_SEG(WAV_SQUARE, 1245, 1427, 6), SFX_END };
static const sfx_seg S_KILL2[]  = { SFX_SEG(WAV_SQUARE, 740, 740, 3), SFX_SEG(WAV_SQUARE, 932, 932, 3),
                                    SFX_SEG(WAV_SQUARE, 1109, 1109, 3), SFX_SEG(WAV_SQUARE, 1480, 1697, 6), SFX_END };
static const sfx_seg S_KILL3[]  = { SFX_SEG(WAV_SQUARE, 880, 880, 3), SFX_SEG(WAV_SQUARE, 1109, 1109, 3),
                                    SFX_SEG(WAV_SQUARE, 1319, 1319, 3), SFX_SEG(WAV_SQUARE, 1760, 2018, 6), SFX_END };
static const sfx_seg S_KILL4[]  = { SFX_SEG(WAV_SQUARE, 1047, 1047, 3), SFX_SEG(WAV_SQUARE, 1319, 1319, 3),
                                    SFX_SEG(WAV_SQUARE, 1568, 1568, 3), SFX_SEG(WAV_SQUARE, 2093, 2400, 8), SFX_END };
static const sfx_seg *const S_KILL[5] = { S_KILL0, S_KILL1, S_KILL2, S_KILL3, S_KILL4 };
static const sfx_seg S_SPIT[]   = { SFX_SEG(WAV_NOISE, 1200, 400, 6), SFX_END };
static const sfx_seg S_POWER[]  = { SFX_SEG(WAV_SQUARE, 392, 784, 6), SFX_SEG(WAV_SQUARE, 523, 1047, 6), SFX_SEG(WAV_SQUARE, 659, 1319, 8), SFX_END };
static const sfx_seg S_EXTEND[] = { SFX_SEG(WAV_TRIANGLE, 784, 784, 5), SFX_SEG(WAV_TRIANGLE, 1047, 1047, 5), SFX_SEG(WAV_TRIANGLE, 784, 784, 5),
                                    SFX_SEG(WAV_TRIANGLE, 1047, 1047, 5), SFX_SEG(WAV_TRIANGLE, 1568, 1568, 12), SFX_END };
static const sfx_seg S_BOSS_HIT[] = { SFX_SEG(WAV_SAWTOOTH, 220, 110, 6), SFX_SEG(WAV_NOISE, 900, 300, 4), SFX_END };
static const sfx_seg S_BOOM[]   = { SFX_SEG(WAV_NOISE, 2000, 200, 30), SFX_SEG(WAV_SQUARE, 1047, 2093, 10), SFX_END };
static const sfx_seg S_FRUIT[]  = { SFX_SEG(WAV_TRIANGLE, 1047, 1047, 3), SFX_SEG(WAV_TRIANGLE, 1568, 1568, 3),
                                    SFX_SEG(WAV_TRIANGLE, 2093, 2093, 5), SFX_END };
static const sfx_seg S_ESCAPE[] = { SFX_SEG(WAV_SAWTOOTH, 180, 420, 10), SFX_END };
static const sfx_seg S_DIE[]    = { SFX_SEG(WAV_SQUARE, 880, 440, 20), SFX_SEG(WAV_SQUARE, 440, 110, 30),
                                    SFX_SEG(WAV_NOISE, 800, 200, 12), SFX_END };

void sfx_setup(void) {
    sfx_init(VOICE3, SFX_VOLUME);
}

void sfx_jump(void)   { sfx_play_prio(S_JUMP, 0); }
void sfx_bounce(void) { sfx_play_prio(S_BOUNCE, 0); }
void sfx_blow(void)   { sfx_play_prio(S_BLOW, 0); }
void sfx_pop(void)    { sfx_play_prio(S_POP, 1); }
void sfx_trap(void)   { sfx_play_prio(S_TRAP, 1); }
void sfx_escape(void) { sfx_play_prio(S_ESCAPE, 1); }
void sfx_kill(uint8_t combo) { sfx_play_prio(S_KILL[combo > 4 ? 4 : combo], 2); }
void sfx_spit(void)   { sfx_play_prio(S_SPIT, 1); }
void sfx_power(void)  { sfx_play_prio(S_POWER, 2); }
void sfx_extend(void) { sfx_play_prio(S_EXTEND, 3); }
void sfx_boss_hit(void) { sfx_play_prio(S_BOSS_HIT, 2); }
void sfx_boom(void)   { sfx_play_prio(S_BOOM, 3); }
void sfx_fruit(void)  { sfx_play_prio(S_FRUIT, 2); }
void sfx_die(void)    { sfx_play_prio(S_DIE, 3); }
