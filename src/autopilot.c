/*
 * A simple bot that plays the real game: the title screen's attract demo, and whole games in
 * AUTOPILOT builds (-DAUTOPILOT=ON, for videos and frame-rate runs). It returns the buttons a player
 * would hold this frame; everything else (physics, rules, sound) is the normal game.
 */
#include "game.h"

#define BLOW_RANGE  60    // a shot bubble flies about 56 px
#define SAME_ROW    12

static uint16_t t;
static uint16_t last_in;
static uint16_t u;
static uint16_t last_blow;
static int16_t last_x;
static uint8_t still;      // frames without moving while trying to
static int8_t wander = 1;
static uint8_t jump_hold;  // frames left holding jump

static int16_t abs16(int16_t v) { return v < 0 ? -v : v; }

void autopilot_reset(void) {
    t = u = last_in = last_blow = 0;
    still = jump_hold = 0;
}

uint16_t autopilot_input(void) {
    game_entity_t *e, *hero = NULL, *enemy = NULL, *bubble = NULL, *threat = NULL, *target = NULL, *fruit = NULL, *flee = NULL;
    int16_t hx, hy, dx, dy, best = 0x7FFF, best_b = 0x7FFF, best_f = 0x7FFF, d;
    uint16_t in = 0;
    int8_t dir = 0;

    // decide on every other frame and hold the buttons in between, so the bot's own CPU time
    // (it scans the whole pool) doesn't inflate the frame-rate measurements much
    if (++t & 1) return last_in;
    u = t >> 1;   // decision counter
    hero = actors_hero();
    if (hero == NULL || hero->entity.physics.visual.hidden || actors_hero_dead()) return last_in = 0;
    hx = PHYSICS_FP_INT(hero->entity.physics.body.x);
    hy = PHYSICS_FP_INT(hero->entity.physics.body.y);

    ENTITY_POOL_FOR_EACH(&game_entities, e, entity.physics.visual.type) {
        if (e->entity.physics.visual.type == E_FIRE) {
            // a fireball coming along our row: jump it
            dx = PHYSICS_FP_INT(e->entity.physics.body.x) - hx;
            if (abs16(PHYSICS_FP_INT(e->entity.physics.body.y) - hy - 4) < 12 && abs16(dx) < 56 && (dx < 0) == (e->entity.physics.body.vx > 0) && jump_hold == 0)
                jump_hold = 12;
        } else if (e->entity.physics.visual.type == E_SLIME || e->entity.physics.visual.type == E_BAT || e->entity.physics.visual.type == E_IMP || e->entity.physics.visual.type == E_BOSS) {
            dx = PHYSICS_FP_INT(e->entity.physics.body.x) - hx;
            dy = PHYSICS_FP_INT(e->entity.physics.body.y) - hy;
            if (e->entity.physics.visual.type == E_BOSS) {
                // keep away from the King, blow at him from a distance
                dy += 8;
                if (abs16(dx) < 44 && abs16(dy) < 36) { flee = e; continue; }
            }
            d = abs16(dx) + 2 * abs16(dy);
            if (d < best) { best = d; enemy = e; }
            // on our row and in range: blow at it before anything else
            if (abs16(dy) < SAME_ROW && abs16(dx) < BLOW_RANGE) target = e;
            // close but not on our row, where a bubble can't reach it: get out of the way
            else if (abs16(dx) < 24 && abs16(dy) < 28) threat = e;
        } else if (e->entity.physics.visual.type == E_BUBBLE && (e->data & 0xFF)) {
            // a bubble with an enemy inside
            d = abs16(PHYSICS_FP_INT(e->entity.physics.body.x) - hx) + abs16(PHYSICS_FP_INT(e->entity.physics.body.y) - hy);
            if (d < best_b) { best_b = d; bubble = e; }
        } else if (e->entity.physics.visual.type == E_FRUIT) {
            d = abs16(PHYSICS_FP_INT(e->entity.physics.body.x) - hx) + 2 * abs16(PHYSICS_FP_INT(e->entity.physics.body.y) - hy);
            if (d < best_f) { best_f = d; fruit = e; }
        }
    }

    if (flee) {
        // run from the King; hop over him when cornered
        dx = PHYSICS_FP_INT(flee->entity.physics.body.x) - hx;
        dir = dx < 0 ? 1 : -1;
        if (abs16(dx) < 24 && jump_hold == 0) jump_hold = 12;
        if ((u & 7) == 0) in |= IN_BLOW;
    } else if (target) {
        dx = PHYSICS_FP_INT(target->entity.physics.body.x) - hx;
        if ((uint16_t)(u - last_blow) < 8) {
            // the bubble is reloading: back off if it's close
            if (abs16(dx) < 28) dir = dx < 0 ? 1 : -1;
        } else if ((u & 1) == 0) {
            dir = dx < 0 ? -1 : 1;   // face it
        } else {
            in |= IN_BLOW;
            last_blow = u;
        }
    } else if (threat) {
        dir = PHYSICS_FP_INT(threat->entity.physics.body.x) < hx ? 1 : -1;
    // pop trapped bubbles that are close, or all of them once nothing is left to trap
    } else if (bubble && (enemy == NULL || best_b < 80 || best_b < best)) {
        dx = PHYSICS_FP_INT(bubble->entity.physics.body.x) - hx;
        dy = PHYSICS_FP_INT(bubble->entity.physics.body.y) - hy;
        if (abs16(dx) > 6) dir = dx < 0 ? -1 : 1;
        if (dy < -8 && abs16(dx) < 40 && jump_hold == 0) jump_hold = 9;
    } else if (fruit && (enemy == NULL || best_f < 64 || best > 120)) {
        // collect fruit on the way
        dx = PHYSICS_FP_INT(fruit->entity.physics.body.x) - hx;
        dy = PHYSICS_FP_INT(fruit->entity.physics.body.y) - hy;
        if (abs16(dx) > 4) dir = dx < 0 ? -1 : 1;
        if (dy < -SAME_ROW && abs16(dx) < 32 && jump_hold == 0) jump_hold = 10;
    } else if (enemy) {
        dx = PHYSICS_FP_INT(enemy->entity.physics.body.x) - hx;
        dy = PHYSICS_FP_INT(enemy->entity.physics.body.y) - hy;
        if (abs16(dy) < SAME_ROW && abs16(dx) < BLOW_RANGE) {
            // face it and blow
            if ((u & 7) < 2) dir = dx < 0 ? -1 : 1;
            if ((u & 7) == 3) in |= IN_BLOW;
        } else if (dy < -SAME_ROW) {
            // above: head for it and jump up through the platforms
            dir = dx < 0 ? -1 : 1;
            if (jump_hold == 0 && (u & 31) == 0) jump_hold = 10;
            if ((u & 15) == 5 && abs16(dx) < BLOW_RANGE) in |= IN_BLOW;
        } else {
            // level or below: walk to it, dropping off ledges
            dir = abs16(dx) > 4 ? (dx < 0 ? -1 : 1) : wander;
            if ((u & 15) == 9 && abs16(dx) < BLOW_RANGE && abs16(dy) < 24) in |= IN_BLOW;
        }
    } else {
        dir = wander;
    }
    // the flood is coming up under us: climb
    if (game.flood < HEIGHT && hy > (int16_t)game.flood * 16 - 44 && jump_hold == 0 && (u & 7) == 0) jump_hold = 10;

    // walls: if we stopped moving, turn around and hop
    if (dir != 0 && hx == last_x) {
        if (++still > 30) {
            wander = -wander;
            dir = wander;
            jump_hold = 10;
            still = 0;
        }
    } else {
        still = 0;
    }
    last_x = hx;

    if (dir < 0) in |= IN_LEFT;
    else if (dir > 0) in |= IN_RIGHT;
    if (jump_hold) {
        jump_hold--;
        if (jump_hold > 1) in |= IN_JUMP;   // release for a few frames so the next jump registers
    }
    return last_in = in;
}
