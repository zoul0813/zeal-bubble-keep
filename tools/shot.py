#!/usr/bin/env python3
"""Screenshots without a window: run a build headless in zeal-native's console, and at given times read the
round's tiles, the sprite table and the game state from RAM, then draw them with assets/tileset.gif.
Text banners (READY, ROUND CLEAR) are not in RAM and are left out; the HUD row is rebuilt from `game`.
Statics are found through the per-object .sym files of the build dir (link order: main, actors, level).
Usage: tools/shot.py <build-dir> <out-prefix> <seconds> [seconds...] [--enter 3]   (Pillow needed)
  e.g. tools/shot.py build-p9 screenshots/boss 20 40   (an AUTOPILOT build plays by itself after Enter)"""
import argparse, os, re, subprocess
from PIL import Image

import shlex
from emulator import add_arguments, resolve, boot_commands

ap = argparse.ArgumentParser()
ap.add_argument('build'); ap.add_argument('out'); ap.add_argument('times', type=float, nargs='+')
ap.add_argument('--enter', type=float, default=3.0, help='seconds after exec to press Enter (0: never)')
ap.add_argument('--scale', type=int, default=2)
add_arguments(ap)
a = ap.parse_args()
resolve(a, ap)
HERE = os.path.dirname(os.path.abspath(__file__))
binp = os.path.join(a.build, 'out', 'bubblekeep.bin')
glob = {}
for l in open(binp[:-4] + '.map'):
    m = re.match(r'\s+([0-9A-F]{8})\s+(_\w+)\s', l)
    if m: glob[m.group(2)] = int(m.group(1), 16)


def sym(mod):
    d = {}; size = 0
    for l in open(os.path.join(a.build, 'CMakeFiles/bubblekeep.dir/src/%s.c.sym' % mod)):
        m = re.match(r'\s+1 (_\w+)\s+([0-9A-F]{6})', l)
        if m: d[m.group(1)] = int(m.group(2), 16)
        m = re.match(r'\s+1 _DATA\s+size\s+([0-9A-F]+)', l)
        if m: size = int(m.group(1), 16)
    return d, size


lv, _ = sym('level'); ac, ac_size = sym('actors')
level_base = glob['_level_spawn_x'] - lv['_level_spawn_x']
ROOM = level_base + lv['_room_tiles']
ARENA = level_base - ac_size + ac['_sprite_arena']
GAME = glob['_game']

FRAME_T = 10_000_000
rom = a.rom
cmds = boot_commands(os.path.basename(binp))
now = 0.0
events = sorted([(t, 'shot') for t in a.times] + ([(a.enter, 'enter')] if a.enter else []))
for t, what in events:
    if t > now: cmds.append('run %d' % int((t - now) * FRAME_T)); now = t
    if what == 'enter': cmds += ['press ENTER', 'run 500000', 'release ENTER']; now += 0.05
    else:
        for ad, n in ((GAME, 26), (ARENA, 320), (ROOM, 300)):
            for o in range(0, n, 256): cmds.append('rb 0x%x %d' % (ad + o, min(256, n - o)))
cmds.append('quit')
p = subprocess.run([a.emu, '-r', rom, '-H', os.path.abspath(os.path.dirname(binp)), '-o', '-g'] + shlex.split(a.args),
                   input='\n'.join(cmds) + '\n', capture_output=True, text=True, timeout=1800)
vals = []
for line in (p.stdout + p.stderr).split('\n'):
    m = re.search(r'\[DBG\] [0-9a-f]{4}: ((?:[0-9a-f]{2} ?)+)$', line.strip())
    if m: vals += [int(x, 16) for x in m.group(1).split()]

ts = Image.open(os.path.join(HERE, '..', 'assets', 'tileset.gif'))
pal = ts.getpalette()
TW = ts.width // 16


def tile(i):
    return ts.crop(((i % TW) * 16, (i // TW) * 16, (i % TW) * 16 + 16, (i // TW) * 16 + 16))


def five(v):
    return '%05d' % v


per = 26 + 320 + 300
for n, t in enumerate(a.times):
    blob = vals[n * per:(n + 1) * per]
    g, arena, room = blob[:26], blob[26:346], blob[346:]
    img = Image.new('P', (320, 240)); img.putpalette(pal)
    blocks = lambda x, y: 0 <= x < 20 and 1 <= y < 15 and room[y * 20 + x] != 0
    flood = g[18]
    for y in range(15):
        for x in range(20):
            sh = 0
            if not blocks(x, y):
                l, u = blocks(x - 1, y), blocks(x, y - 1)
                sh = 10 if l and u else 8 if l else 9 if u else 11 if blocks(x - 1, y - 1) else 0
            if flood < 15 and y >= flood: sh = 12 if y == flood else 13
            if sh: img.paste(tile(sh), (x * 16, y * 16))
    for y in range(1, 15):
        for x in range(20):
            tid = room[y * 20 + x]
            if tid: tt = tile(tid); img.paste(tt, (x * 16, y * 16), tt.point(lambda v: 255 if v else 0, '1'))
    # HUD, like hud_draw()
    score, hi, lives, rnd = g[0] | g[1] << 8, g[2] | g[3] << 8, g[4], g[5] + 1
    line = list(five(score) + '  ' + str(min(lives, 9)) + ' R%02d ' % rnd + ' ' * 7)
    hp_tiles = []
    if g[25]: line[14:18] = 'DEMO'
    elif g[22]:
        line[13] = 'K'; lit = (g[21] * 6 + g[22] - 1) // g[22]; hp_tiles = [14 if i < lit else 15 for i in range(6)]
    else: line[13:15] = 'HI'; line[15:20] = five(hi)
    for x, ch in enumerate(line):
        if ch != ' ': tt = tile(ord(ch)); img.paste(tt, (x * 16, 0), tt.point(lambda v: 255 if v else 0, '1'))
    tt = tile(16); img.paste(tt, (96, 0), tt.point(lambda v: 255 if v else 0, '1'))
    for i, tid in enumerate(hp_tiles):
        tt = tile(tid); img.paste(tt, ((14 + i) * 16, 0), tt.point(lambda v: 255 if v else 0, '1'))
    # sprites, in table order
    for s in range(40):
        y, x, tid, fl, opt = (arena[s * 8] | arena[s * 8 + 1] << 8, arena[s * 8 + 2] | arena[s * 8 + 3] << 8,
                              arena[s * 8 + 4], arena[s * 8 + 5], arena[s * 8 + 6])
        if y == 0 or y >= 256 or x == 0 or x >= 336: continue
        parts = [tile(tid)] + ([tile(tid + 1)] if opt & 2 else [])
        spr = Image.new('P', (16, 16 * len(parts))); spr.putpalette(pal)
        for i, pt in enumerate(parts): spr.paste(pt, (0, 16 * i))
        if fl & 8: spr = spr.transpose(Image.FLIP_LEFT_RIGHT)
        if fl & 4: spr = spr.transpose(Image.FLIP_TOP_BOTTOM)
        img.paste(spr, (x - 16, y - 16), spr.point(lambda v: 255 if v else 0, '1'))
    out = '%s-%ds.png' % (a.out, t) if len(a.times) > 1 else a.out + '.png'
    img.convert('RGB').resize((320 * a.scale, 240 * a.scale), Image.NEAREST).save(out)
    print(out, 'round', rnd, 'score', score)
