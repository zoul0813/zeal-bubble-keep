#include <zgdk/physics/platformer.h>
/*
 * Everything that moves: the hero, slimes, bats, imps and their fireballs, the Slime King, bubbles,
 * knocked-out enemies, fruit and power-ups, and score pops.
 * Physics comes from ZGDK; ZGDK owns entities/animation. This file owns game rules.
 */
#include "game.h"

/* bubble phases */
#define PH_SHOT    0   // flying out of the hero's mouth, traps enemies
#define PH_RISE    1   // floating up
#define PH_DRIFT   2   // under the ceiling, drifting to the middle
#define PH_POPPING 3   // queued for the pop chain

/* enemy data bits */
#define EN_LEFT    0x01
#define EN_ANGRY   0x02
#define EN_WINDUP  0x04   // imp: mouth open, about to spit (high byte of data: spit cooldown)

#define SHOT_FRAMES      14
#define SHOT_FRAMES_RAPID 22   // candy: bubbles fly further
#define BUBBLE_LIFE      600
#define TRAPPED_LIFE     420
#define TRAPPED_WARN     120
#define MAX_BUBBLES      10
#define BLOW_COOLDOWN    16
#define BLOW_COOLDOWN_RAPID 9
#define ITEM_EVERY       6     // every 6th knock-out lands as a power-up instead of fruit
#define MAX_FIRES        2
#define BOSS_SPAWN_FRAMES 170  // the King calls a minion this often
#define BOSS_FLASH       24
#define INVULN_FRAMES    150
#define DEATH_FRAMES     100
#define FRUIT_LIFE       480
#define CEILING_Y        34    // bubbles stop just under the top wall
#define DRIFT_X          152   // and gather around the middle

typedef struct {
    uint16_t age;
    uint8_t phase;
    uint8_t combo;
} ext_t;

static gfx_sprite sprite_arena[ARENA_SIZE];
static game_entity_t pool[POOL_SIZE];
static ext_t ext[ARENA_SIZE];
static game_entity_t *hero;
static uint8_t hero_dead;      // frames left in the death animation
static uint8_t hero_invuln;
static uint8_t hero_cooldown;
static uint8_t hero_blow;      // frames left showing the blow frame
static uint8_t hero_left;
static uint8_t bubble_count;
static uint8_t frame;
static uint8_t hero_rapid;     // power-ups, lost with a life
static uint8_t hero_shoe;
static game_entity_t *boss;
static uint8_t fire_n;

/* Indexed by the entity's sprite slot: `e - pool` divides by sizeof(game_entity_t) = 31 (a __divsint call,
 * which showed up in the profile), while gfx_sprite is 8 bytes, so this is a shift. */
#define EXT(e) (&ext[(e)->entity.physics.visual.sprite - sprite_arena])

/*
 * Per-frame lists by type, filled in one pass over the pool. Walking the whole pool
 * for every overlap test was a large part of the frame (entities_next in the profile).
 * Entries can change type during the frame (trapped, popped), so users re-check e->entity.physics.visual.type.
 */
#define MAX_ENEMIES 16
static game_entity_t *enemy_list[MAX_ENEMIES];
static uint8_t enemy_n;
static game_entity_t *bubble_list[MAX_BUBBLES];
static uint8_t bubble_n;
static game_entity_t *other_list[POOL_SIZE];
static uint8_t other_n;

static void spawn_ko(int16_t x, int16_t y, uint8_t kind, uint8_t combo, int8_t dir);
static void hero_die(void);

/* trappable enemies (the Slime King is not) */
static uint8_t is_enemy(game_entity_t *e) {
    return e->entity.physics.visual.type == E_SLIME || e->entity.physics.visual.type == E_BAT || e->entity.physics.visual.type == E_IMP;
}

static uint8_t kind_of(game_entity_t *e) {
    return e->entity.physics.visual.type == E_SLIME ? KIND_SLIME : e->entity.physics.visual.type == E_BAT ? KIND_BAT : KIND_IMP;
}

/* ---------------------------------------------------------------- tuning (12.4 fixed point) */
static const platformer_config_t HERO_PHYS = {
    .gravity = 3, .max_fall = 48, .jump_vel = -76, .jump_cut = -24,
    .accel = 4, .decel = 6, .max_run = 24, .coyote = 5, .buffer = 6, .air_jumps = 0,
};
/*
 * Enemies run their physics every other frame (half of them on even frames, half on odd),
 * so these are 30 Hz values: velocities x2, gravity x4. Same arcs and speeds as 60 Hz with
 * 3 / 40 / -74 / 12, at half the CPU. 7+ enemies didn't fit in a frame at 60 Hz.
 */
static const platformer_config_t SLIME_PHYS = {
    .gravity = 12, .max_fall = 80, .jump_vel = -148, .jump_cut = -148,
    .accel = 24, .decel = 24, .max_run = 24, .coyote = 0, .buffer = 0, .air_jumps = 0,
};
static const platformer_config_t SLIME_ANGRY_PHYS = {
    .gravity = 12, .max_fall = 80, .jump_vel = -148, .jump_cut = -148,
    .accel = 40, .decel = 40, .max_run = 40, .coyote = 0, .buffer = 0, .air_jumps = 0,
};
static const platformer_config_t HERO_SHOE_PHYS = {   // shoe power-up
    .gravity = 3, .max_fall = 48, .jump_vel = -76, .jump_cut = -24,
    .accel = 6, .decel = 8, .max_run = 34, .coyote = 5, .buffer = 6, .air_jumps = 0,
};
static const platformer_config_t KING_PHYS = {   // 30 Hz like the other enemies
    .gravity = 12, .max_fall = 80, .jump_vel = -168, .jump_cut = -168,
    .accel = 16, .decel = 16, .max_run = 20, .coyote = 0, .buffer = 0, .air_jumps = 0,
};
static const platformer_config_t KING_ANGRY_PHYS = {
    .gravity = 12, .max_fall = 80, .jump_vel = -176, .jump_cut = -176,
    .accel = 24, .decel = 24, .max_run = 34, .coyote = 0, .buffer = 0, .air_jumps = 0,
};
static const platformer_config_t FALL_PHYS = {   // fruit: gravity only
    .gravity = 3, .max_fall = 48, .jump_vel = 0, .jump_cut = 0,
    .accel = 0, .decel = 0, .max_run = 0, .coyote = 0, .buffer = 0, .air_jumps = 0,
};
static const platformer_config_t KO_PHYS = {     // knocked-out bodies move every other frame: 30 Hz values
    .gravity = 12, .max_fall = 96, .jump_vel = 0, .jump_cut = 0,
    .accel = 0, .decel = 0, .max_run = 0, .coyote = 0, .buffer = 0, .air_jumps = 0,
};

/* ---------------------------------------------------------------- animations */
static const uint8_t F_HERO_STAND[] = { SPR_HERO_STAND };
static const uint8_t F_HERO_WALK[] = { SPR_HERO_WALK1, SPR_HERO_STAND, SPR_HERO_WALK2, SPR_HERO_STAND };
static const uint8_t F_HERO_JUMP[] = { SPR_HERO_JUMP };
static const uint8_t F_HERO_BLOW[] = { SPR_HERO_BLOW };
static const uint8_t F_HERO_DEAD[] = { SPR_HERO_DEAD1, SPR_HERO_DEAD2 };
static const uint8_t F_SLIME[] = { SPR_SLIME1, SPR_SLIME2 };
static const uint8_t F_SLIME_ANGRY[] = { SPR_SLIME_ANGRY1, SPR_SLIME_ANGRY2 };
static const uint8_t F_BAT[] = { SPR_BAT1, SPR_BAT2 };
static const uint8_t F_BAT_ANGRY[] = { SPR_BAT_ANGRY1, SPR_BAT_ANGRY2 };
static const uint8_t F_BUBBLE[] = { SPR_BUBBLE1, SPR_BUBBLE2 };
static const uint8_t F_BUBBLE_SLIME[] = { SPR_BUBBLE_SLIME1, SPR_BUBBLE_SLIME2 };
static const uint8_t F_BUBBLE_SLIME_WARN[] = { SPR_BUBBLE_SLIME1, SPR_BUBBLE_ANGRY };
static const uint8_t F_BUBBLE_BAT[] = { SPR_BUBBLE_BAT1, SPR_BUBBLE_BAT2 };
static const uint8_t F_BUBBLE_BAT_WARN[] = { SPR_BUBBLE_BAT1, SPR_BUBBLE_BAT_ANGRY };
static const uint8_t F_SLIME_KO[] = { SPR_SLIME_KO1, SPR_SLIME_KO2 };
static const uint8_t F_BAT_KO[] = { SPR_BAT_KO1, SPR_BAT_KO2 };
static const uint8_t F_IMP[] = { SPR_IMP1, SPR_IMP2 };
static const uint8_t F_IMP_ANGRY[] = { SPR_IMP_ANGRY1, SPR_IMP_ANGRY2 };
static const uint8_t F_IMP_SPIT[] = { SPR_IMP_SPIT };
static const uint8_t F_BUBBLE_IMP[] = { SPR_BUBBLE_IMP1, SPR_BUBBLE_IMP2 };
static const uint8_t F_BUBBLE_IMP_WARN[] = { SPR_BUBBLE_IMP1, SPR_BUBBLE_IMP_ANGRY };
static const uint8_t F_IMP_KO[] = { SPR_IMP_KO1, SPR_IMP_KO2 };
static const uint8_t F_FIRE[] = { SPR_FIRE1, SPR_FIRE2 };
static const uint8_t F_KING[] = { SPR_KING1, SPR_KING2 };
static const uint8_t F_KING_HURT[] = { SPR_KING_HURT, SPR_KING1 };
static const uint8_t F_POP[] = { SPR_POP, SPR_POP2 };
static const uint8_t F_FRUIT[] = { SPR_CHERRY, SPR_BANANA, SPR_MELON, SPR_GEM, SPR_CANDY, SPR_SHOE, SPR_STAR };
static const uint8_t F_PTS[] = {
    SPR_PTS_10, SPR_PTS_50, SPR_PTS_100, SPR_PTS_200, SPR_PTS_400,
    SPR_PTS_800, SPR_PTS_1600, SPR_PTS_3200, SPR_PTS_6400, SPR_PTS_1UP,
};

static const anim_def A_HERO_STAND = { F_HERO_STAND, 1, 1, ANIM_LOOP };
static const anim_def A_HERO_WALK = { F_HERO_WALK, 4, 6, ANIM_LOOP };
static const anim_def A_HERO_JUMP = { F_HERO_JUMP, 1, 1, ANIM_LOOP };
static const anim_def A_HERO_BLOW = { F_HERO_BLOW, 1, 1, ANIM_LOOP };
static const anim_def A_HERO_DEAD = { F_HERO_DEAD, 2, 6, ANIM_LOOP };
static const anim_def A_SLIME = { F_SLIME, 2, 12, ANIM_LOOP };
static const anim_def A_SLIME_ANGRY = { F_SLIME_ANGRY, 2, 6, ANIM_LOOP };
static const anim_def A_BAT = { F_BAT, 2, 10, ANIM_LOOP };
static const anim_def A_BAT_ANGRY = { F_BAT_ANGRY, 2, 5, ANIM_LOOP };
static const anim_def A_BUBBLE = { F_BUBBLE, 2, 16, ANIM_LOOP };
static const anim_def A_BUBBLE_SLIME = { F_BUBBLE_SLIME, 2, 16, ANIM_LOOP };
static const anim_def A_BUBBLE_SLIME_WARN = { F_BUBBLE_SLIME_WARN, 2, 6, ANIM_LOOP };
static const anim_def A_BUBBLE_BAT = { F_BUBBLE_BAT, 2, 16, ANIM_LOOP };
static const anim_def A_BUBBLE_BAT_WARN = { F_BUBBLE_BAT_WARN, 2, 6, ANIM_LOOP };
static const anim_def A_SLIME_KO = { F_SLIME_KO, 2, 4, ANIM_LOOP };
static const anim_def A_BAT_KO = { F_BAT_KO, 2, 4, ANIM_LOOP };
static const anim_def A_IMP = { F_IMP, 2, 10, ANIM_LOOP };
static const anim_def A_IMP_ANGRY = { F_IMP_ANGRY, 2, 5, ANIM_LOOP };
static const anim_def A_IMP_SPIT = { F_IMP_SPIT, 1, 1, ANIM_LOOP };
static const anim_def A_BUBBLE_IMP = { F_BUBBLE_IMP, 2, 16, ANIM_LOOP };
static const anim_def A_BUBBLE_IMP_WARN = { F_BUBBLE_IMP_WARN, 2, 6, ANIM_LOOP };
static const anim_def A_IMP_KO = { F_IMP_KO, 2, 4, ANIM_LOOP };
static const anim_def A_FIRE = { F_FIRE, 2, 4, ANIM_LOOP };
static const anim_def A_KING = { F_KING, 2, 14, ANIM_LOOP };
static const anim_def A_KING_HURT = { F_KING_HURT, 2, 3, ANIM_LOOP };
static const anim_def A_POP = { F_POP, 2, 4, ANIM_LOOP };
/* one-frame defs for the fruit, power-up and score tiles */
static const anim_def A_FRUIT[7] = {
    { &F_FRUIT[0], 1, 1, ANIM_LOOP }, { &F_FRUIT[1], 1, 1, ANIM_LOOP },
    { &F_FRUIT[2], 1, 1, ANIM_LOOP }, { &F_FRUIT[3], 1, 1, ANIM_LOOP },
    { &F_FRUIT[4], 1, 1, ANIM_LOOP }, { &F_FRUIT[5], 1, 1, ANIM_LOOP }, { &F_FRUIT[6], 1, 1, ANIM_LOOP },
};
static const anim_def A_PTS[10] = {
    { &F_PTS[0], 1, 1, ANIM_LOOP }, { &F_PTS[1], 1, 1, ANIM_LOOP }, { &F_PTS[2], 1, 1, ANIM_LOOP },
    { &F_PTS[3], 1, 1, ANIM_LOOP }, { &F_PTS[4], 1, 1, ANIM_LOOP }, { &F_PTS[5], 1, 1, ANIM_LOOP },
    { &F_PTS[6], 1, 1, ANIM_LOOP }, { &F_PTS[7], 1, 1, ANIM_LOOP }, { &F_PTS[8], 1, 1, ANIM_LOOP },
    { &F_PTS[9], 1, 1, ANIM_LOOP },
};
#define PTS_10   0
#define PTS_50   1
#define PTS_100  2   // 100 << combo for combo 0..6
#define PTS_3200 7
#define PTS_1UP  9

static const uint16_t FRUIT_POINTS[4] = { 100, 200, 400, 800 };   // power-ups score 800 too

/* ---------------------------------------------------------------- helpers */
/* body_overlap with a quick reject: every body here is at most 16x16, so bodies 16 px or more apart
 * (256 in 12.4) on an axis can't touch. Most pairs fail this without the 4 shifts. */
static uint8_t hit(const body_t *a, const body_t *b) {
    if ((uint16_t)(a->x - b->x + 255) > 510) return 0;
    if ((uint16_t)(a->y - b->y + 255) > 510) return 0;
    return body_overlap(a, b);
}

/* for pairs with the Slime King (24 px tall) */
static uint8_t hit_tall(const body_t *a, const body_t *b) {
    if ((uint16_t)(a->x - b->x + 255) > 510) return 0;
    if ((uint16_t)(a->y - b->y + 511) > 1022) return 0;
    return body_overlap(a, b);
}

static int16_t px(game_entity_t *e) { return PHYSICS_FP_INT(e->entity.physics.body.x); }
static int16_t py(game_entity_t *e) { return PHYSICS_FP_INT(e->entity.physics.body.y); }

static uint8_t is_angry(game_entity_t *e) {
    return (e->data & EN_ANGRY) || game.hurry || game.loop;
}

static void wrap(game_entity_t *e) {
    room_exit exit = room_exit_for(&e->entity.physics.body);
    if (exit != ROOM_EXIT_NONE) {
        room_wrap_body(&e->entity.physics.body, exit);
        platformer_reset(&e->platformer);
    }
}

static void spawn_label(int16_t x, int16_t y, uint8_t pts_index) {
    game_entity_t *p = physics_entities_spawn(&game_entities, E_PTS, x, y, 16, 16);
    if (p == NULL) return;
    animated_physics_entity_play(&p->entity, &A_PTS[pts_index]);
    p->timer = 50;
}

static void spawn_points(int16_t x, int16_t y, uint8_t pts_index, uint16_t points) {
    score_add(points);
    spawn_label(x, y, pts_index);
}

static void shake(uint8_t frames) {
    if (game.shake < frames) game.shake = frames;
}

static uint8_t combo_index(uint8_t combo) {
    return combo > 6 ? PTS_100 + 6 : PTS_100 + combo;
}

static uint16_t combo_points(uint8_t combo) {
    return (uint16_t)100 << (combo > 6 ? 6 : combo);
}

/* ---------------------------------------------------------------- spawning */
void actors_init(void) {
    body_set_touch_mask(0);   // no hazards or ladders in Bubble Keep
    sprites_register_arena(sprite_arena, ARENA_SIZE);
    sprite_entities_init(&game_entities, pool, POOL_SIZE, sizeof(*pool));
}

static void enemy_anim(game_entity_t *e) {
    uint8_t angry = is_angry(e);
    if (e->entity.physics.visual.type == E_SLIME) animated_physics_entity_play(&e->entity, angry ? &A_SLIME_ANGRY : &A_SLIME);
    else if (e->entity.physics.visual.type == E_BAT) animated_physics_entity_play(&e->entity, angry ? &A_BAT_ANGRY : &A_BAT);
    else if (e->data & EN_WINDUP) animated_physics_entity_play(&e->entity, &A_IMP_SPIT);
    else animated_physics_entity_play(&e->entity, angry ? &A_IMP_ANGRY : &A_IMP);
}

static const uint8_t KIND_TYPE[4] = { E_SLIME, E_SLIME, E_BAT, E_IMP };

static game_entity_t *spawn_enemy_px(uint8_t kind, int16_t x, int16_t y, uint8_t angry) {
    game_entity_t *e = physics_entities_spawn(&game_entities, KIND_TYPE[kind], x, y, 16, 16);
    if (e == NULL) return NULL;
    e->data = angry ? EN_ANGRY : 0;
    if (rand8() & 1) e->data |= EN_LEFT;
    e->timer = 30 + (rand8() & 63);
    if (kind == KIND_BAT) {
        e->entity.physics.body.vx = (e->data & EN_LEFT) ? -16 : 16;
        e->entity.physics.body.vy = (rand8() & 1) ? -16 : 16;
    }
    enemy_anim(e);
    return e;
}

void actors_spawn_enemy(uint8_t kind, uint8_t tx, uint8_t ty) {
    spawn_enemy_px(kind, tx * 16, ty * 16, 0);
}

/* 'K' in a round: the Slime King. One 16x32 sprite, crown above the 14x24 hitbox. */
void actors_spawn_boss(uint8_t tx, uint8_t ty) {
    game_entity_t *k = physics_entities_spawn(&game_entities, E_BOSS, tx * 16 + 1, ty * 16 - 8, 14, 24);
    if (k == NULL) return;
    k->entity.physics.visual.tall = 1;
    k->entity.physics.visual.ox = -1;
    k->entity.physics.visual.oy = -6;
    k->data = EN_LEFT;
    k->timer = 40;
    EXT(k)->age = 0;
    EXT(k)->combo = 0;   // hurt flash
    animated_physics_entity_play(&k->entity, &A_KING);
    boss = k;
    game.boss_max = 12 + 4 * (game.round / 10) + 4 * game.loop;
    game.boss_hp = game.boss_max;
}

static void hero_spawn(void) {
    hero->entity.physics.body.vx = 0;
    hero->entity.physics.body.vy = 0;
    platformer_set_pos(&hero->entity.physics.body, &hero->platformer, level_spawn_x * 16, level_spawn_y * 16);
    hero->entity.physics.visual.hidden = 0;
    hero_dead = 0;
    hero_invuln = INVULN_FRAMES;
    hero_left = 0;
    hero->entity.physics.visual.flip = 0;
    animated_physics_entity_play(&hero->entity, &A_HERO_STAND);
}

/* Called after level_load() spawned the enemies */
void actors_round_start(void) {
    if (!game.boss_max) boss = NULL;
    hero = physics_entities_spawn(&game_entities, E_HERO, level_spawn_x * 16, level_spawn_y * 16, 14, 16);
    hero->entity.physics.visual.ox = -1;
    hero_spawn();
    hero_cooldown = 0;
    hero_blow = 0;
}

game_entity_t *actors_hero(void) {
    return hero;
}

uint8_t actors_hero_dead(void) {
    return hero_dead;
}

void actors_make_all_angry(void) {
    game_entity_t *e;
    ENTITY_POOL_FOR_EACH(&game_entities, e, entity.physics.visual.type) {
        if (is_enemy(e)) enemy_anim(e);
    }
}

/* ---------------------------------------------------------------- knock-outs */
static void knock(game_entity_t *e, uint8_t combo, int8_t dir) {
    spawn_ko(px(e), py(e), kind_of(e), combo, dir);
    spawn_points(px(e), py(e) - 8, combo_index(combo), combo_points(combo));
    entity_pool_free(&game_entities, e);
}

static void burst(game_entity_t *b) {
    b->entity.physics.visual.type = E_POP;
    b->entity.physics.body.vx = 0;
    b->entity.physics.body.vy = 0;
    b->timer = 8;
    animated_physics_entity_play(&b->entity, &A_POP);
}

/* Star power-up and the King's fall: every enemy and trapped bubble on screen, as one chain */
static void knock_all(uint8_t combo) {
    game_entity_t *e;
    uint8_t i, start = combo;
    int8_t dir = 1;
    for (i = 0; i < enemy_n; i++) {
        e = enemy_list[i];
        if (!is_enemy(e)) continue;
        knock(e, combo, dir);
        if (combo < 6) combo++;
        dir = -dir;
    }
    for (i = 0; i < bubble_n; i++) {
        e = bubble_list[i];
        if (e->entity.physics.visual.type != E_BUBBLE || !(e->data & 0xFF)) continue;
        spawn_ko(px(e), py(e), e->data & 0xFF, combo, dir);
        spawn_points(px(e), py(e) - 8, combo_index(combo), combo_points(combo));
        burst(e);
        if (combo < 6) combo++;
        dir = -dir;
    }
    if (combo != start) sfx_kill(combo - start);
    shake(20);
}

static void boss_defeat(void) {
    int16_t x = px(boss), y = py(boss);
    int8_t i;
    knock_all(1);
    for (i = -1; i <= 2; i++) {
        // the King leaves a pile of gems
        int16_t gx = x + i * 18 - 9;
        game_entity_t *g;
        if (gx < 16) gx = 16;
        if (gx > SCREEN_WIDTH - 32) gx = SCREEN_WIDTH - 32;
        g = physics_entities_spawn(&game_entities, E_FRUIT, gx, y, 16, 16);
        if (g == NULL) break;
        g->data = 3;
        EXT(g)->age = 0;
        EXT(g)->phase = 0;
        animated_physics_entity_play(&g->entity, &A_FRUIT[3]);
    }
    spawn_points(x, y - 8, PTS_3200, 3200);
    boss->entity.physics.visual.tall = 0;
    burst(boss);
    boss = NULL;
    sfx_boom();
    shake(40);
}

static void boss_hurt(uint8_t damage) {
    ext_t *x = EXT(boss);
    if (x->combo) return;   // still flashing from the last hit
    x->combo = BOSS_FLASH;
    game.hud_dirty = 1;
    if (game.boss_hp > damage) {
        game.boss_hp -= damage;
        sfx_boss_hit();
        shake(6);
        return;
    }
    game.boss_hp = 0;
    boss_defeat();
}

#ifdef CHEATS
/* Q key / SNES L: knock out every enemy (testing round flow) */
void actors_cheat_clear(void) {
    knock_all(0);
    if (boss) { EXT(boss)->combo = 0; boss_hurt(6); }
}
#endif

/* ---------------------------------------------------------------- bubbles */
static void blow_bubble(void) {
    int16_t x = px(hero) + (hero_left ? -10 : 10);
    game_entity_t *b;
    if (bubble_count >= MAX_BUBBLES) return;
    b = physics_entities_spawn(&game_entities, E_BUBBLE, x, py(hero), 16, 16);
    if (b == NULL) return;
    bubble_count++;
    b->data = KIND_NONE;
    b->entity.physics.body.vx = hero_left ? -64 : 64;
    EXT(b)->age = 0;
    EXT(b)->phase = PH_SHOT;
    animated_physics_entity_play(&b->entity, &A_BUBBLE);
    sfx_blow();
}

static void bubble_trap(game_entity_t *b, game_entity_t *enemy) {
    uint8_t angry = (enemy->data & EN_ANGRY) != 0;
    uint8_t kind = kind_of(enemy);
    b->data = kind | (angry ? 0x100 : 0);
    animated_physics_entity_play(&b->entity, kind == KIND_SLIME ? &A_BUBBLE_SLIME : kind == KIND_BAT ? &A_BUBBLE_BAT : &A_BUBBLE_IMP);
    entity_pool_free(&game_entities, enemy);
    EXT(b)->phase = PH_RISE;
    EXT(b)->age = 0;
    b->entity.physics.body.vx = 0;
    score_add(10);
    sfx_trap();
}

static void spawn_ko(int16_t x, int16_t y, uint8_t kind, uint8_t combo, int8_t dir) {
    game_entity_t *k = physics_entities_spawn(&game_entities, E_KO, x, y, 16, 16);
    if (k == NULL) return;
    k->data = kind;
    k->entity.physics.body.vx = dir < 0 ? -72 : 72;   // 30 Hz, see KO_PHYS
    k->entity.physics.body.vy = -144;
    EXT(k)->age = combo & 1;   // bodies from one chain move on alternate frames, not all on the same one
    EXT(k)->phase = 0;   // 1 once it has hit the King
    EXT(k)->combo = combo;
    animated_physics_entity_play(&k->entity, kind == KIND_SLIME ? &A_SLIME_KO : kind == KIND_BAT ? &A_BAT_KO : &A_IMP_KO);
}

/* Pop `first` and every bubble touching a popped one, Bubble Bobble style */
static void pop_chain(game_entity_t *first) {
    game_entity_t *b, *o;
    uint8_t combo = 0, found, i, j;
    int8_t dir = px(first) < px(hero) ? -1 : 1;

    EXT(first)->phase = PH_POPPING;
    do {
        found = 0;
        for (i = 0; i < bubble_n; i++) {
            b = bubble_list[i];
            if (b->entity.physics.visual.type != E_BUBBLE || EXT(b)->phase != PH_POPPING) continue;
            // queue the neighbours
            for (j = 0; j < bubble_n; j++) {
                o = bubble_list[j];
                if (o->entity.physics.visual.type != E_BUBBLE || EXT(o)->phase == PH_POPPING) continue;
                if (px(o) - px(b) < 20 && px(b) - px(o) < 20 && py(o) - py(b) < 20 && py(b) - py(o) < 20) {
                    EXT(o)->phase = PH_POPPING;
                    found = 1;
                }
            }
            // and burst this one
            if (b->data & 0xFF) {
                spawn_ko(px(b), py(b), b->data & 0xFF, combo, dir);
                spawn_points(px(b), py(b) - 8, combo_index(combo), combo_points(combo));
                combo++;
                dir = -dir;
            } else {
                score_add(10);
            }
            burst(b);
        }
    } while (found);
    if (combo) sfx_kill(combo - 1);
    else sfx_pop();
    if (combo > 1) shake(4 + 4 * combo);
}

static void release_enemy(game_entity_t *b) {
    uint8_t kind = b->data & 0xFF;
    int16_t x = px(b), y = py(b);
    burst(b);
    if (y > 13 * 16) y = 13 * 16;
    spawn_enemy_px(kind, x, y, 1);
    sfx_escape();
}

static void update_bubble(game_entity_t *b) {
    ext_t *x = EXT(b);
    uint8_t kind = b->data & 0xFF;
    game_entity_t *e;
    uint8_t i;

    x->age++;
    switch (x->phase) {
        case PH_SHOT:
            body_step(&b->entity.physics.body);
            // trap the first enemy we touch
            for (i = 0; i < enemy_n; i++) {
                e = enemy_list[i];
                if (is_enemy(e) && hit(&b->entity.physics.body, &e->entity.physics.body)) {
                    bubble_trap(b, e);
                    return;
                }
            }
            // the King can't be trapped: the bubble bursts on him and stings a little
            if (boss && hit_tall(&b->entity.physics.body, &boss->entity.physics.body)) {
                burst(b);
                boss_hurt(1);
                return;
            }
            if (x->age >= (hero_rapid ? SHOT_FRAMES_RAPID : SHOT_FRAMES) || (b->entity.physics.body.state & BODY_HIT_WALL)) {
                x->phase = PH_RISE;
                b->entity.physics.body.vx = 0;
            }
            break;
        case PH_RISE:
            // no tile collision once floating: bubbles pass through platforms and stop at CEILING_Y.
            // Floating bubbles move every other frame (double steps) to save CPU on busy screens.
            if (x->age & 1) break;
            b->entity.physics.body.y -= 16;
            if (py(b) <= CEILING_Y) {
                b->entity.physics.body.y = PHYSICS_FP(CEILING_Y);
                x->phase = PH_DRIFT;
            }
            break;
        case PH_DRIFT:
            if (x->age & 1) break;
            if (px(b) < DRIFT_X - 4) b->entity.physics.body.x += 12;
            else if (px(b) > DRIFT_X + 4) b->entity.physics.body.x -= 12;
            b->entity.physics.body.y = PHYSICS_FP(CEILING_Y) + ((x->age & 32) ? 16 : 0);   // bob
            break;
    }

    if (kind) {
        if (x->age == TRAPPED_LIFE - TRAPPED_WARN)
            animated_physics_entity_play(&b->entity, kind == KIND_SLIME ? &A_BUBBLE_SLIME_WARN :
                                kind == KIND_BAT ? &A_BUBBLE_BAT_WARN : &A_BUBBLE_IMP_WARN);
        if (x->age >= TRAPPED_LIFE) release_enemy(b);
    } else if (x->age >= BUBBLE_LIFE) {
        burst(b);
    }
}

/* ---------------------------------------------------------------- enemies */
static int8_t facing(game_entity_t *e) {
    return (e->data & EN_LEFT) ? DIRECTION_LEFT : DIRECTION_RIGHT;
}

static void face_hero(game_entity_t *e) {
    if (px(hero) < px(e)) e->data |= EN_LEFT;
    else e->data &= ~EN_LEFT;
}

/* walkers (slimes, imps, the King): platformer physics, turn at walls, screen wrap */
static void walk(game_entity_t *e, const platformer_config_t *phys, int8_t dir, uint8_t jump) {
    platformer_update(&e->entity.physics.body, phys, &e->platformer, dir, 1, jump);
    platformer_move(&e->entity.physics.body, phys);
    if (e->entity.physics.body.state & BODY_HIT_WALL) e->data ^= EN_LEFT;
    e->entity.physics.visual.flip = (e->data & EN_LEFT) ? SPRITE_FLIP_X : 0;
    wrap(e);
}

static void update_slime(game_entity_t *e) {
    uint8_t jump = 0;
    uint8_t angry = is_angry(e);

    if (e->timer) e->timer--;
    if (e->timer == 0 && (e->entity.physics.body.state & BODY_ON_GROUND)) {
        int16_t dy = py(hero) - py(e);
        e->timer = (angry ? 10 : 20) + (rand8() & 31);   // 30 Hz ticks
        if (dy < -24 && (rand8() & 1)) jump = 1;
        else if (dy > -8 && dy < 8 && (rand8() & 1)) face_hero(e);   // same floor: turn toward the hero
    }
    walk(e, angry ? &SLIME_ANGRY_PHYS : &SLIME_PHYS, facing(e), jump);
    enemy_anim(e);
}

static void update_bat(game_entity_t *e) {
    int16_t vx = e->entity.physics.body.vx, vy = e->entity.physics.body.vy;
    int16_t speed = is_angry(e) ? 52 : 32;   // per 30 Hz step
    if (vx < 0) vx = -speed; else vx = speed;
    if (vy < 0) vy = -speed; else vy = speed;
    e->entity.physics.body.vx = vx;
    e->entity.physics.body.vy = vy;
    body_step(&e->entity.physics.body);
    if (e->entity.physics.body.state & BODY_HIT_WALL) vx = -vx;
    if (e->entity.physics.body.state & (BODY_ON_GROUND | BODY_HIT_CEIL)) vy = -vy;
    e->entity.physics.body.vx = vx;
    e->entity.physics.body.vy = vy;
    e->entity.physics.visual.flip = vx < 0 ? SPRITE_FLIP_X : 0;
    wrap(e);
    enemy_anim(e);
}

static void spawn_fire(game_entity_t *imp, uint8_t angry) {
    uint8_t left = imp->data & EN_LEFT;
    int16_t speed = angry ? 44 : 34;   // per frame, 12.4
    game_entity_t *f = physics_entities_spawn(&game_entities, E_FIRE, px(imp) + (left ? -3 : 9), py(imp) + 4, 10, 8);
    if (f == NULL) return;
    f->entity.physics.visual.ox = -3;
    f->entity.physics.visual.oy = -4;
    f->entity.physics.body.vx = left ? -speed : speed;
    f->entity.physics.visual.flip = left ? SPRITE_FLIP_X : 0;
    f->timer = 160;
    animated_physics_entity_play(&f->entity, &A_FIRE);
    fire_n++;
    sfx_spit();
}

/* Imp: walks like a slime but never jumps. When the hero is on its row in front of it, it stops,
 * opens its mouth (the warning) and spits a fireball along the row. */
static void update_imp(game_entity_t *e) {
    uint8_t cool = e->data >> 8;
    uint8_t angry = is_angry(e);
    int8_t dir = facing(e);
    int16_t dx = px(hero) - px(e), dy = py(hero) - py(e);

    e->data &= 0xFF;
    if (cool) cool--;
    if (e->data & EN_WINDUP) {
        dir = DIRECTION_NONE;
        if (--e->timer == 0) {
            e->data &= ~EN_WINDUP;
            spawn_fire(e, angry);
            cool = angry ? 40 : 70;   // 30 Hz ticks
            e->timer = 12;
        }
    } else {
        if (e->timer) e->timer--;
        if (!cool && !hero_dead && fire_n < MAX_FIRES && (e->entity.physics.body.state & BODY_ON_GROUND) &&
            dy > -10 && dy < 10 && dx > -160 && dx < 160 && (dx < 0) == (dir < 0)) {
            e->data |= EN_WINDUP;
            e->timer = angry ? 6 : 9;   // the open mouth shows for 12-18 frames
            dir = DIRECTION_NONE;
        } else if (e->timer == 0 && (e->entity.physics.body.state & BODY_ON_GROUND)) {
            e->timer = (angry ? 10 : 20) + (rand8() & 31);
            if ((dy > -8 && dy < 8) || (rand8() & 3) == 0) {
                face_hero(e);
                dir = facing(e);
            }
        }
    }
    e->data |= (uint16_t)cool << 8;
    walk(e, angry ? &SLIME_ANGRY_PHYS : &SLIME_PHYS, dir, 0);
    enemy_anim(e);
}

static void update_fire(game_entity_t *f) {
    fire_n++;
    body_step(&f->entity.physics.body);
    if ((f->entity.physics.body.state & BODY_HIT_WALL) || --f->timer == 0) {
        burst(f);
        return;
    }
    if (!hero_dead && !hero_invuln && hit(&hero->entity.physics.body, &f->entity.physics.body)) hero_die();
}

/* ---------------------------------------------------------------- the Slime King */
static void update_boss(game_entity_t *k) {
    ext_t *x = EXT(k);
    uint8_t jump = 0;
    uint8_t angry = game.boss_hp <= (game.boss_max >> 1);

    if (x->combo) {
        x->combo--;
        if (x->combo == 0 || x->combo == BOSS_FLASH - 1) animated_physics_entity_play(&k->entity, x->combo ? &A_KING_HURT : &A_KING);
    }
    if (!hero_dead && !hero_invuln && hit_tall(&hero->entity.physics.body, &k->entity.physics.body)) hero_die();

    // calls a minion now and then (the round's minion kind: slimes, bats, then imps)
    if (++x->age >= (angry ? BOSS_SPAWN_FRAMES - 50 : BOSS_SPAWN_FRAMES)) {
        x->age = 0;
        if (game.enemies < 4) {   // last frame's count: the King plus 3 helpers, walking or bubbled
            uint8_t r = game.round / 10;
            game_entity_t *m = spawn_enemy_px(r == 0 ? KIND_SLIME : r == 1 ? KIND_BAT : KIND_IMP, px(k), py(k) + 8, 0);
            if (m && m->entity.physics.visual.type != E_BAT) m->entity.physics.body.vy = -120;
        }
    }

    if (frame & 1) return;   // moves at 30 Hz like the other enemies
    if (k->timer) k->timer--;
    if (k->timer == 0 && (k->entity.physics.body.state & BODY_ON_GROUND)) {
        k->timer = (angry ? 12 : 24) + (rand8() & 31);
        // jump when the hero stands higher (the King is 8 px taller), now and then anyway
        if (py(hero) - py(k) < -16 || (rand8() & 15) == 0) jump = 1;
        if (angry || (rand8() & 1)) face_hero(k);   // only half the time until he's hurt
    }
    // he stands still for half a second after calling a minion (age restarts at the call)
    walk(k, angry ? &KING_ANGRY_PHYS : &KING_PHYS, x->age < 30 ? DIRECTION_NONE : facing(k), jump);
}

/* ---------------------------------------------------------------- knocked out, fruit, effects */
static void update_ko(game_entity_t *k) {
    ext_t *x = EXT(k);
    int16_t vx = k->entity.physics.body.vx;
    game_entity_t *e;
    uint8_t i;

    x->age++;
    if (x->age & 1) {
        platformer_move(&k->entity.physics.body, &KO_PHYS);
        k->entity.physics.body.vx = (k->entity.physics.body.state & BODY_HIT_WALL) ? -vx : vx;
        wrap(k);
    }

    // the flying body knocks out any enemy it hits, and the combo keeps climbing
    for (i = 0; i < enemy_n; i++) {
        e = enemy_list[i];
        if (is_enemy(e) && hit(&k->entity.physics.body, &e->entity.physics.body)) {
            uint8_t combo = x->combo + 1;
            x->combo = combo;
            knock(e, combo, vx < 0 ? 1 : -1);
            sfx_kill(combo);
            shake(combo > 4 ? 24 : 8 + 4 * combo);
        }
    }
    // and is the only thing that really hurts the King
    if (boss && !x->phase && hit_tall(&k->entity.physics.body, &boss->entity.physics.body)) {
        x->phase = 1;
        k->entity.physics.body.vx = -vx;
        boss_hurt(3);
    }

    if ((x->age & 1) && (k->entity.physics.body.state & BODY_ON_GROUND)) {
        if (x->age > 50) {
            uint8_t fruit = x->combo > 3 ? 3 : x->combo;
            if (--game.ko_count == 0) {
                // every few knock-outs: a power-up, skipping the ones the hero already has
                game.ko_count = ITEM_EVERY;
                for (i = 0; i < 3; i++) {
                    fruit = ITEM_CANDY + game.item_next;
                    if (++game.item_next == 3) game.item_next = 0;
                    if (!(fruit == ITEM_CANDY && hero_rapid) && !(fruit == ITEM_SHOE && hero_shoe)) break;
                    fruit = ITEM_STAR;
                }
            }
            k->entity.physics.visual.type = E_FRUIT;
            k->entity.physics.body.vx = 0;
            k->data = fruit;
            x->age = 0;
            x->phase = 0;
            animated_physics_entity_play(&k->entity, &A_FRUIT[fruit]);
        } else {
            k->entity.physics.body.vy = -80;
        }
    }
}

static void update_fruit(game_entity_t *f) {
    ext_t *x = EXT(f);
    x->age++;
    if (!x->phase) {
        // falls until it lands, then stays put (platforms never move)
        platformer_move(&f->entity.physics.body, &FALL_PHYS);
        wrap(f);
        if (f->entity.physics.body.state & BODY_ON_GROUND) x->phase = 1;
    }
    f->entity.physics.visual.hidden = (x->age > FRUIT_LIFE - 120) && (x->age & 4);
    if (!hero_dead && hit(&hero->entity.physics.body, &f->entity.physics.body)) {
        uint8_t item = f->data;
        if (item < ITEM_CANDY) {
            spawn_points(px(f), py(f) - 8, PTS_100 + item, FRUIT_POINTS[item]);
            sfx_fruit();
        } else {
            spawn_points(px(f), py(f) - 8, PTS_100 + 3, 800);
            sfx_power();
            if (item == ITEM_CANDY) hero_rapid = 1;
            else if (item == ITEM_SHOE) hero_shoe = 1;
            else {
                knock_all(0);
                if (boss) { EXT(boss)->combo = 0; boss_hurt(6); }
            }
        }
        entity_pool_free(&game_entities, f);
        return;
    }
    if (x->age >= FRUIT_LIFE) entity_pool_free(&game_entities, f);
}

/* ---------------------------------------------------------------- hero */
static void hero_die(void) {
    hero_dead = DEATH_FRAMES;
    hero_rapid = 0;
    hero_shoe = 0;
    game.died = 1;
    hero->entity.physics.body.vx = 0;
    hero->entity.physics.body.vy = 0;
    animated_physics_entity_play(&hero->entity, &A_HERO_DEAD);
    sfx_die();
}

static void update_hero(void) {
    int8_t dir = DIRECTION_NONE;
    game_entity_t *e;
    uint8_t i;

    if (hero_dead) {
        if (game.lives == 0) return;
        hero->entity.physics.visual.hidden = 0;
        if (--hero_dead == 0) {
#ifndef AUTOPILOT
            if (game.lives) game.lives--;
#else
            // bot builds measure the whole game: endless lives (the attract demo plays by the rules)
            if (game.lives && game.demo) game.lives--;
#endif
            game.hud_dirty = 1;
            if (game.lives) hero_spawn();
            else {
                hero_dead = 1;   // stay dead, main sees game over
                hero->entity.physics.visual.hidden = 1;
            }
        }
        return;
    }

    if (input_now & IN_LEFT) { dir = DIRECTION_LEFT; hero_left = 1; }
    if (input_now & IN_RIGHT) { dir = DIRECTION_RIGHT; hero_left = 0; }
    {
        const platformer_config_t *phys = hero_shoe ? &HERO_SHOE_PHYS : &HERO_PHYS;
        if (platformer_update(&hero->entity.physics.body, phys, &hero->platformer, dir, input_now & IN_JUMP, input_pressed & IN_JUMP)) sfx_jump();
        platformer_move(&hero->entity.physics.body, phys);
    }
    wrap(hero);
    // the flood: feet more than 6 px under the surface
    if (game.flood < HEIGHT && py(hero) > (int16_t)game.flood * 16 - 10) {
        hero_die();
        return;
    }

    if (hero_cooldown) hero_cooldown--;
    if ((input_pressed & IN_BLOW) && hero_cooldown == 0) {
        blow_bubble();
        hero_cooldown = hero_rapid ? BLOW_COOLDOWN_RAPID : BLOW_COOLDOWN;
        hero_blow = 10;
    }

    if (hero_blow) { hero_blow--; animated_physics_entity_play(&hero->entity, &A_HERO_BLOW); }
    else if (!(hero->entity.physics.body.state & BODY_ON_GROUND)) animated_physics_entity_play(&hero->entity, &A_HERO_JUMP);
    else if (hero->entity.physics.body.vx) animated_physics_entity_play(&hero->entity, &A_HERO_WALK);
    else animated_physics_entity_play(&hero->entity, &A_HERO_STAND);
    hero->entity.physics.visual.flip = hero_left ? SPRITE_FLIP_X : 0;

    if (hero_invuln) {
        hero_invuln--;
        hero->entity.physics.visual.hidden = (hero_invuln & 4) != 0;
    } else {
        hero->entity.physics.visual.hidden = 0;
    }

    for (i = 0; i < bubble_n; i++) {
        e = bubble_list[i];
        if (e->entity.physics.visual.type != E_BUBBLE || EXT(e)->phase == PH_SHOT || !hit(&hero->entity.physics.body, &e->entity.physics.body)) continue;
        // falling onto a bubble with jump held bounces off it
        if (hero->entity.physics.body.vy > 0 && py(hero) + 8 < py(e) && (input_now & IN_JUMP)) {
            hero->entity.physics.body.vy = HERO_PHYS.jump_vel;
            sfx_bounce();
        } else {
            pop_chain(e);
        }
    }
    if (hero_invuln) return;
    for (i = 0; i < enemy_n; i++) {
        e = enemy_list[i];
        if (is_enemy(e) && hit(&hero->entity.physics.body, &e->entity.physics.body)) {
            hero_die();
            return;
        }
    }
}

/* ---------------------------------------------------------------- frame */
void actors_update(void) {
    game_entity_t *e;
    uint8_t enemies = 0, i;

    // sort the pool into per-type lists (spawns during this frame are picked up next frame)
    enemy_n = bubble_n = other_n = fire_n = 0;
    ENTITY_POOL_FOR_EACH(&game_entities, e, entity.physics.visual.type) {
        if (is_enemy(e)) { if (enemy_n < MAX_ENEMIES) enemy_list[enemy_n++] = e; }
        else if (e->entity.physics.visual.type == E_BUBBLE) { if (bubble_n < MAX_BUBBLES) bubble_list[bubble_n++] = e; }
        else if (e->entity.physics.visual.type != E_HERO) other_list[other_n++] = e;
    }

    update_hero();
    hero->entity.physics.visual.sprite->tile = anim_tick(&hero->entity.anim);

    frame++;
    for (i = 0; i < enemy_n; i++) {
        e = enemy_list[i];
        if (!is_enemy(e)) continue;
        enemies++;
        e->entity.physics.visual.sprite->tile = anim_tick(&e->entity.anim);
        // alternate halves: each enemy moves on every other frame (30 Hz tuning above)
        if ((i ^ frame) & 1) continue;
        if (e->entity.physics.visual.type == E_SLIME) update_slime(e);
        else if (e->entity.physics.visual.type == E_BAT) update_bat(e);
        else update_imp(e);
    }
    for (i = 0; i < bubble_n; i++) {
        e = bubble_list[i];
        if (e->entity.physics.visual.type != E_BUBBLE) continue;
        update_bubble(e);
        if (e->entity.physics.visual.type == E_BUBBLE && (e->data & 0xFF)) enemies++;
        e->entity.physics.visual.sprite->tile = anim_tick(&e->entity.anim);
    }
    for (i = 0; i < other_n; i++) {
        e = other_list[i];
        switch (e->entity.physics.visual.type) {
            case E_KO:
                update_ko(e);
                if (e->entity.physics.visual.type == E_KO) enemies++;
                break;
            case E_FRUIT:  update_fruit(e); break;
            case E_FIRE:   update_fire(e); break;
            case E_BOSS:
                update_boss(e);
                if (e->entity.physics.visual.type == E_BOSS) enemies++;
                break;
            case E_PTS:
                e->entity.physics.body.y -= 8;
                if (--e->timer == 0) entity_pool_free(&game_entities, e);
                break;
            case E_POP:
                if (--e->timer == 0) entity_pool_free(&game_entities, e);
                break;
        }
        if (e->entity.physics.visual.type != ENTITY_FREE) e->entity.physics.visual.sprite->tile = anim_tick(&e->entity.anim);
    }
    bubble_count = bubble_n;
    game.enemies = enemies;
    if (game.extended) {
        game.extended = 0;
        spawn_label(px(hero), py(hero) - 12, PTS_1UP);
        sfx_extend();
    }
    // screen shake: sprites move with the layers (main scrolls them in the vblank)
    game.view_y = 0;
    if (game.shake) game.view_y = (--game.shake & 2) ? 3 : 0;
    entity_view_set(0, game.view_y);
    physics_entities_sync(&game_entities);
}
