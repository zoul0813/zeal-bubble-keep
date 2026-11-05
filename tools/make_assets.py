#!/usr/bin/env python3
"""Generate the Bubble Keep tileset (assets/tileset.gif) and its tile index header (src/gfx.h).

Everything is drawn here as text pixel art so the art stays diffable. One 256-tile, 8-bit
tileset is shared by both layers and all sprites. Palette index 0 is transparent for layer 1
and sprites, and is the background color on layer 0.

Layout:
    0         empty
    1..31     level tiles (blocks per theme, shadows), HUD icons
    32..95    font, so ZGDK's default ASCII_MAP (char == tile) works without ascii_map()
    96..      sprites
Run: python3 tools/make_assets.py   (from the game folder)
"""
import os
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# ---------------------------------------------------------------- palette
PAL = [
    ('.', (10, 10, 30)),     # 0 background / transparent
    ('k', (0, 0, 0)),        # outline
    ('w', (255, 255, 255)),
    ('l', (192, 192, 208)),  # light grey
    ('m', (112, 112, 136)),  # mid grey
    ('s', (22, 22, 52)),     # block shadow on background
    ('G', (120, 230, 90)),   # hero green
    ('g', (40, 160, 60)),    # hero green shade
    ('d', (16, 90, 40)),     # hero dark green
    ('y', (255, 224, 64)),   # yellow
    ('o', (255, 144, 32)),   # orange
    ('r', (224, 48, 48)),    # red
    ('R', (128, 16, 32)),    # dark red
    ('P', (200, 120, 255)),  # slime purple
    ('p', (120, 50, 180)),   # slime purple shade
    ('q', (70, 20, 110)),    # slime purple dark
    ('A', (255, 96, 96)),    # angry slime light
    ('a', (190, 30, 60)),    # angry slime shade
    ('C', (180, 245, 255)),  # bubble light
    ('c', (80, 190, 230)),   # bubble mid
    ('b', (30, 110, 170)),   # bubble dark
    ('B', (90, 150, 255)),   # bat blue
    ('n', (40, 70, 170)),    # bat blue shade
    ('h', (160, 90, 40)),    # brown
    ('H', (96, 50, 20)),     # dark brown
    ('e', (255, 140, 200)),  # pink
    ('E', (200, 60, 140)),   # dark pink
    ('t', (60, 200, 220)),   # teal (theme)
    ('T', (20, 110, 140)),   # teal shade
    ('u', (40, 50, 120)),    # theme stone blue
    ('U', (90, 110, 200)),   # theme stone blue light
    ('v', (250, 200, 120)),  # sand
    ('V', (180, 120, 50)),   # sand shade
    ('W', (16, 44, 104)),    # flood water
    ('X', (40, 96, 176)),    # flood water light
]
CI = {ch: i for i, (ch, _) in enumerate(PAL)}

tiles = [None] * 256
names = {}


def grid(rows):
    rows = [r.replace(' ', '') for r in rows]
    assert len(rows) == 16, len(rows)
    for r in rows:
        assert len(r) == 16, (r, len(r))
    return [[CI[ch] for ch in r] for r in rows]


def put(index, name, px):
    assert tiles[index] is None, (index, name)
    tiles[index] = px
    if name:
        names[name] = index


def blank():
    return [[0] * 16 for _ in range(16)]


def flipx(px):
    return [list(reversed(r)) for r in px]


def recolor(px, mapping):
    m = {CI[a]: CI[b] for a, b in mapping.items()}
    return [[m.get(v, v) for v in r] for r in px]


# ---------------------------------------------------------------- level tiles
put(0, 'TILE_EMPTY', blank())


def block(light, mid, dark):
    # beveled brick block, Bubble Bobble style
    rows = [
        "LLLLLLLLLLLLLLLM",
        "LMMMMMMMMMMMMMMD",
        "LMMMMMMMMMMMMMMD",
        "LMMMMMMMMMMMMMMD",
        "LMMMMMMMMMMMMMMD",
        "LMMMMMMMMMMMMMMD",
        "LMMMMMMMMMMMMMMD",
        "MDDDDDDDDDDDDDDD",
        "LLLLLLLMLLLLLLLL",
        "MMMMMMMDLMMMMMMM",
        "MMMMMMMDLMMMMMMM",
        "MMMMMMMDLMMMMMMM",
        "MMMMMMMDLMMMMMMM",
        "MMMMMMMDLMMMMMMM",
        "MMMMMMMDLMMMMMMM",
        "DDDDDDDDMDDDDDDD",
    ]
    return grid([r.replace('L', light).replace('M', mid).replace('D', dark) for r in rows])


THEMES = [('e', 'E', 'R'), ('t', 'T', 'u'), ('v', 'V', 'H'), ('U', 'u', 'k'), ('G', 'g', 'd'), ('o', 'r', 'R')]
for i, (l, m, d) in enumerate(THEMES):
    put(1 + i, 'TILE_BLOCK%d' % i, block(l, m, d))
    # same art, separate index so it can carry TF_ONEWAY instead of TF_SOLID
    put(17 + i, 'TILE_PLAT%d' % i, block(l, m, d))
THEME_COUNT = len(THEMES)

# drop shadows on the background, like the arcade: right of a block, below a block, corner
sh = blank()
for y in range(16):
    for x in range(0, 6):
        sh[y][x] = CI['s']
put(8, 'TILE_SHADOW_R', sh)
sh = blank()
for y in range(0, 6):
    for x in range(16):
        sh[y][x] = CI['s']
put(9, 'TILE_SHADOW_B', sh)
sh = blank()
for y in range(16):
    for x in range(16):
        if x < 6 or y < 6:
            sh[y][x] = CI['s']
put(10, 'TILE_SHADOW_RB', sh)
sh = blank()
for y in range(0, 6):
    for x in range(0, 6):
        sh[y][x] = CI['s']
put(11, 'TILE_SHADOW_C', sh)

# the flood (HURRY UP): layer 0, behind the blocks
wt = blank()
for y in range(16):
    for x in range(16):
        if y >= 3:
            wt[y][x] = CI['W']
        elif y == 2 or (y == 1 and (x % 8) in (2, 3, 4, 5)) or (y == 0 and (x % 8) in (3, 4)):
            wt[y][x] = CI['X']
for (x, y) in [(3, 7), (4, 7), (11, 11), (12, 11)]:
    wt[y][x] = CI['X']
put(12, 'TILE_WATER_TOP', wt)
wf = [[CI['W']] * 16 for _ in range(16)]
for (x, y) in [(2, 4), (3, 4), (10, 10), (11, 10), (6, 14), (7, 14)]:
    wf[y][x] = CI['X']
put(13, 'TILE_WATER', wf)

# boss health on the HUD: a segment lit / used up
put(14, 'TILE_HP', grid([
    "................",
    "................",
    "................",
    "....kkkkkkkk....",
    "...kAAAwAAAAk...",
    "...kAwwAAAAAk...",
    "...kAAAAAAAak...",
    "...kAAAAAAAak...",
    "...kaAAAAAAak...",
    "...kaaAAAAaak...",
    "...kaaaaaaaak...",
    "....kkkkkkkk....",
    "................",
    "................",
    "................",
    "................",
]))
put(15, 'TILE_HP_OFF', grid([
    "................",
    "................",
    "................",
    "....kkkkkkkk....",
    "...kssssssssk...",
    "...ks......sk...",
    "...ks......sk...",
    "...ks......sk...",
    "...ks......sk...",
    "...ks......sk...",
    "...kssssssssk...",
    "....kkkkkkkk....",
    "................",
    "................",
    "................",
    "................",
]))

# HUD life icon (small hero head)
put(16, 'TILE_LIFE', grid([
    "................",
    "................",
    "................",
    ".....kkkkk......",
    "....kGGGGGk.....",
    "...kGGGGwwwk....",
    "...kGGGwwkwk....",
    "...kGGGGwwwk....",
    "...kgGGGGyyk....",
    "...kggGGGGk.....",
    "....kggggk......",
    ".....kkkk.......",
    "................",
    "................",
    "................",
    "................",
]))

# ---------------------------------------------------------------- font (5x7 scaled x2)
FONT = {
    ' ': ["", "", "", "", "", "", ""],
    '!': ["..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."],
    '-': [".....", ".....", ".....", "#####", ".....", ".....", "....."],
    '.': [".....", ".....", ".....", ".....", ".....", ".##..", ".##.."],
    ':': [".....", ".##..", ".##..", ".....", ".##..", ".##..", "....."],
    '?': [".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."],
    "'": ["..#..", "..#..", ".#...", ".....", ".....", ".....", "....."],
    '=': [".....", ".....", "#####", ".....", "#####", ".....", "....."],
    '0': [".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."],
    '1': ["..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."],
    '2': [".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"],
    '3': ["#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###."],
    '4': ["...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."],
    '5': ["#####", "#....", "####.", "....#", "....#", "#...#", ".###."],
    '6': ["..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."],
    '7': ["#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."],
    '8': [".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."],
    '9': [".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."],
    'A': [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    'B': ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
    'C': [".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."],
    'D': ["###..", "#..#.", "#...#", "#...#", "#...#", "#..#.", "###.."],
    'E': ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
    'F': ["#####", "#....", "#....", "####.", "#....", "#....", "#...."],
    'G': [".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####"],
    'H': ["#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    'I': [".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."],
    'J': ["..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."],
    'K': ["#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"],
    'L': ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
    'M': ["#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"],
    'N': ["#...#", "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#"],
    'O': [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    'P': ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
    'Q': [".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"],
    'R': ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    'S': [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
    'T': ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
    'U': ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    'V': ["#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."],
    'W': ["#...#", "#...#", "#...#", "#.#.#", "#.#.#", "#.#.#", ".#.#."],
    'X': ["#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"],
    'Y': ["#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."],
    'Z': ["#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"],
}
for ch, rows in FONT.items():
    px = blank()
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            if c == '#':
                for dy in range(2):
                    for dx in range(2):
                        px[1 + y * 2 + dy][3 + x * 2 + dx] = CI['w']
                # drop shadow
                for dy in range(2):
                    for dx in range(2):
                        yy, xx = 2 + y * 2 + dy, 4 + x * 2 + dx
                        if yy < 16 and xx < 16 and px[yy][xx] == 0:
                            px[yy][xx] = CI['m']
    put(ord(ch), None, px)
# re-run shadow pass so white always wins over a neighbour's shadow
for ch in FONT:
    px = tiles[ord(ch)]
for i in range(32, 96):
    if tiles[i] is None:
        tiles[i] = blank()

# ---------------------------------------------------------------- sprites
S = 96


def spr(name, px):
    global S
    put(S, name, px)
    S += 1


HERO_TOP = [
    "................",
    "......kkkkk.....",
    "....kkGGGGGkk...",
    "...kGGGGGGwwwk..",
    "..kGGGGGGwwkkwk.",
    "..kGGGGGGwwkkwk.",
    ".kGGGGGGGGwwwGk.",
    ".kgGGGGGGGGGGGk.",
    ".kggGGGGGGyyyyk.",
    ".kggGGGGGGGkkk..",
    ".kgggGGGGGGGk...",
    "..kgggoooooogk..",
    "..kkgggggggggk..",
]
HERO_FEET = {
    'stand': ["...kgggggggk....", "...kyyk.kyyk....", "...kkkk.kkkk...."],
    'walk1': ["...kgggggggk....", "..kyyk...kyyk...", "..kkkk...kkkk..."],
    'walk2': ["...kgggggggk....", "....kyykyyk.....", "....kkkkkkk....."],
    'jump':  ["...kgggggggk....", "....kyyyyyk.....", ".....kkkkk......"],
}
spr('SPR_HERO_STAND', grid(HERO_TOP + HERO_FEET['stand']))
spr('SPR_HERO_WALK1', grid(HERO_TOP + HERO_FEET['walk1']))
spr('SPR_HERO_WALK2', grid(HERO_TOP + HERO_FEET['walk2']))
spr('SPR_HERO_JUMP', grid(HERO_TOP + HERO_FEET['jump']))
blow = [r for r in HERO_TOP]
blow[8] = ".kggGGGGGGkkkkCk"
blow[9] = ".kggGGGGGGk..kCk"
blow[10] = ".kgggGGGGGkkkkk."
spr('SPR_HERO_BLOW', grid(blow + HERO_FEET['stand']))
dead = [r.replace('w', 'l').replace('G', 'g') for r in HERO_TOP]
dead[3] = "...kggggggkkkk.."
dead[4] = "..kggggggk.ww.k."
dead[5] = "..kggggggk.ww.k."
spr('SPR_HERO_DEAD1', grid(dead + HERO_FEET['jump']))
spr('SPR_HERO_DEAD2', [list(reversed(r)) for r in reversed(grid(dead + HERO_FEET['jump']))])

SLIME = [
    "................",
    "................",
    "................",
    "......kkkk......",
    "....kkPPPPkk....",
    "...kPPPPPPPPk...",
    "..kPPwwPPPwwPk..",
    "..kPwwkkPwwkkk..",
    ".kPPwwkkPwwkkPk.",
    ".kPPPwwPPPwwPPk.",
    ".kpPPPPPPPPPPpk.",
    ".kpPPPkkkkPPPpk.",
    ".kppPPPPPPPPppk.",
    ".kqppppppppppqk.",
    "..kqqqqqqqqqqk..",
    "...kkkkkkkkkk...",
]
SLIME2 = [
    "................",
    "................",
    "................",
    "................",
    "......kkkk......",
    "....kkPPPPkk....",
    "...kPPwwPPPwwk..",
    "..kPPwwkkPwwkkk.",
    ".kPPPwwkkPwwkkk.",
    ".kPPPPwwPPPwwPPk",
    ".kpPPPPPPPPPPPpk",
    "kppPPPPkkkkPPPpk",
    "kpppPPPPPPPPPppk",
    "kqqppppppppppqqk",
    ".kqqqqqqqqqqqqk.",
    "..kkkkkkkkkkkk..",
]
spr('SPR_SLIME1', grid(SLIME))
spr('SPR_SLIME2', grid(SLIME2))
ANGRY = {'P': 'A', 'p': 'a', 'q': 'R'}
spr('SPR_SLIME_ANGRY1', recolor(grid(SLIME), ANGRY))
spr('SPR_SLIME_ANGRY2', recolor(grid(SLIME2), ANGRY))

BAT1 = [
    "................",
    "................",
    "k..............k",
    "kk....kkkk....kk",
    "knk..kBBBBk..knk",
    "knnkkBwwBwwkknnk",
    "knnnBBwkBwkBnnnk",
    ".knnBBBBBBBBnnk.",
    ".knnnBBkkBBnnnk.",
    "..knnBBBBBBnnk..",
    "...kk.kBBk.kk...",
    "......kkkk......",
    "................",
    "................",
    "................",
    "................",
]
BAT2 = [
    "................",
    "................",
    "................",
    "......kkkk......",
    ".....kBBBBk.....",
    "....kBwwBwwk....",
    "...kBBwkBwkBk...",
    "..knBBBBBBBBnk..",
    ".knnnBBkkBBnnnk.",
    "knnnnBBBBBBnnnnk",
    "knnkk.kBBk.kknnk",
    "knk...kkkk...knk",
    "kk............kk",
    "k..............k",
    "................",
    "................",
]
spr('SPR_BAT1', grid(BAT1))
spr('SPR_BAT2', grid(BAT2))
spr('SPR_BAT_ANGRY1', recolor(grid(BAT1), {'B': 'A', 'n': 'a'}))
spr('SPR_BAT_ANGRY2', recolor(grid(BAT2), {'B': 'A', 'n': 'a'}))


def bubble(inner=None, phase=0):
    px = blank()
    cx, cy = 7.5, 7.5
    for y in range(16):
        for x in range(16):
            dx, dy = x - cx, y - cy
            if phase:
                dx *= 1.06
                dy *= 0.95
            d = (dx * dx + dy * dy) ** 0.5
            if 6.3 < d <= 7.6:
                px[y][x] = CI['c']
            elif 5.4 < d <= 6.3:
                px[y][x] = CI['b'] if (dx + dy) > 3 else 0
    if inner:
        for y in range(16):
            for x in range(16):
                if inner[y][x] and px[y][x] == 0:
                    px[y][x] = inner[y][x]
    # highlight
    for (x, y) in [(4, 3), (5, 3), (3, 4), (3, 5), (4, 4)]:
        px[y][x] = CI['w']
    px[4][6] = CI['C']
    return px


def shrink_slime(src, offset_y=0):
    # take the slime's body, squash it into the middle of the bubble
    g = grid(src)
    out = blank()
    for y in range(16):
        for x in range(16):
            sy = 3 + (y - 3) * 13 // 10
            sx = 1 + (x - 3) * 14 // 10
            if 3 <= y <= 12 and 3 <= x <= 12 and 0 <= sy < 16 and 0 <= sx < 16:
                out[y + offset_y][x] = g[sy][sx]
    return out


spr('SPR_BUBBLE1', bubble())
spr('SPR_BUBBLE2', bubble(phase=1))
spr('SPR_BUBBLE_SLIME1', bubble(shrink_slime(SLIME)))
spr('SPR_BUBBLE_SLIME2', bubble(shrink_slime(SLIME), phase=1))
spr('SPR_BUBBLE_ANGRY', bubble(recolor(shrink_slime(SLIME), ANGRY)))
spr('SPR_BUBBLE_BAT1', bubble(shrink_slime(BAT1, 1)))
spr('SPR_BUBBLE_BAT2', bubble(shrink_slime(BAT1, 1), phase=1))
spr('SPR_BUBBLE_BAT_ANGRY', bubble(recolor(shrink_slime(BAT1, 1), {'B': 'A', 'n': 'a'})))

# popping burst
pop = blank()
for (x, y) in [(7, 0), (8, 0), (7, 1), (8, 1), (7, 14), (8, 14), (7, 15), (8, 15), (0, 7), (0, 8), (1, 7), (1, 8),
               (14, 7), (14, 8), (15, 7), (15, 8), (2, 2), (3, 3), (13, 2), (12, 3), (2, 13), (3, 12), (13, 13), (12, 12)]:
    pop[y][x] = CI['C']
for (x, y) in [(7, 4), (8, 4), (4, 7), (4, 8), (11, 7), (11, 8), (7, 11), (8, 11)]:
    pop[y][x] = CI['w']
spr('SPR_POP', pop)

# knocked-out slime spinning through the air
ko = list(SLIME)
ko[7] = "..kPwkwkPkwkwk.."
ko[8] = ".kPPPwkPPPwkPPk."
ko[6] = "..kPkwkPPkwkPk.."
ko = grid(ko)
spr('SPR_SLIME_KO1', ko)
spr('SPR_SLIME_KO2', [list(reversed(r)) for r in reversed(ko)])
bko = list(BAT1)
bko[5] = "knnkkBkwkkwkknnk"
bko[6] = "knnnBBwkBwkBnnnk"
bko = grid(bko)
spr('SPR_BAT_KO1', bko)
spr('SPR_BAT_KO2', [list(reversed(r)) for r in reversed(bko)])

# fruit
spr('SPR_CHERRY', grid([
    "................",
    "..........kk....",
    ".........kgk....",
    "........kgk.....",
    ".......kgkgk....",
    "......kgk.kgk...",
    ".....kgk...kgk..",
    "...kkkk....kkkk.",
    "..krrwrk..krrwrk",
    ".krrrwrrkkrrrwrk",
    ".krrrrrrkkrrrrrk",
    ".kRrrrrrkkRrrrrk",
    ".kRRrrrRk.kRRrrk",
    "..kRRRRk...kRRk.",
    "...kkkk.....kk..",
    "................",
]))
spr('SPR_BANANA', grid([
    "................",
    "............kk..",
    "...........khk..",
    "..........kyyk..",
    ".........kyyyk..",
    "........kyyyyk..",
    ".......kyyyyok..",
    "......kyyyyyok..",
    "....kkyyyyyok...",
    "..kkyyyyyyyok...",
    ".kyyyyyyyyook...",
    ".kyyyyyyyook....",
    "..kooyyooook....",
    "...kkooookk.....",
    ".....kkkk.......",
    "................",
]))
spr('SPR_MELON', grid([
    "................",
    "................",
    "................",
    "................",
    "kkkkkkkkkkkkkkkk",
    "krrrrrrrrrrrrrrk",
    "krrkrrrrkrrrkrrk",
    ".krrrrkrrrrrrrk.",
    ".krrrrrrrrkrrrk.",
    "..kwwwwwwwwwwk..",
    "..kGGGGGGGGGGk..",
    "...kggggggggk...",
    "....kddddddk....",
    ".....kkkkkk.....",
    "................",
    "................",
]))
spr('SPR_GEM', grid([
    "................",
    "................",
    "....kkkkkkkk....",
    "...kCCwCCCCck...",
    "..kCwwCCCCccck..",
    ".kCCCCCCCcccccck",
    ".kkkkkkkkkkkkkk.",
    "..kcCCCCcccbbk..",
    "...kcCCCccbbk...",
    "....kcCCcbbk....",
    ".....kcCcbk.....",
    "......kcbk......",
    ".......kk.......",
    "................",
    "................",
    "................",
]))

# score pops, drawn with a 3x5 font
MINI = {
    '0': ["###", "#.#", "#.#", "#.#", "###"], '1': [".#.", "##.", ".#.", ".#.", "###"],
    '2': ["###", "..#", "###", "#..", "###"], '4': ["#.#", "#.#", "###", "..#", "..#"],
    '5': ["###", "#..", "###", "..#", "###"], '8': ["###", "#.#", "###", "#.#", "###"],
    '6': ["###", "#..", "###", "#.#", "###"], '3': ["###", "..#", "###", "..#", "###"],
    'U': ["#.#", "#.#", "#.#", "#.#", "###"], 'P': ["###", "#.#", "###", "#..", "#.."],
}


def mini_text(text, color):
    px = blank()
    w = len(text) * 4 - 1
    x0 = (16 - w) // 2
    for i, ch in enumerate(text):
        for y, r in enumerate(MINI[ch]):
            for x, c in enumerate(r):
                if c == '#':
                    px[5 + y][x0 + i * 4 + x] = CI[color]
                    if px[6 + y][x0 + i * 4 + x + 1] == 0:
                        px[6 + y][x0 + i * 4 + x + 1] = CI['k']
    return px


for txt, col in [('10', 'w'), ('50', 'w'), ('100', 'y'), ('200', 'y'), ('400', 'o'), ('800', 'o'), ('1600', 'e'), ('3200', 'r'), ('6400', 'C')]:
    spr('SPR_PTS_%s' % txt, mini_text(txt, col))

# ---------------------------------------------------------------- added for the fun pass (2026-10-27)
# Appended after the score pops so the older tile numbers stay put.

# second burst frame: the ring flies apart
pop2 = blank()
for (x, y) in [(7, 0), (8, 0), (0, 7), (0, 8), (15, 7), (15, 8), (7, 15), (8, 15),
               (1, 1), (14, 1), (1, 14), (14, 14), (4, 2), (11, 2), (2, 11), (13, 11), (2, 4), (13, 4), (4, 13), (11, 13)]:
    pop2[y][x] = CI['w'] if (x + y) % 3 else CI['C']
spr('SPR_POP2', pop2)

# fire imp: walks the floors and spits fireballs along its row
IMP = [
    "................",
    "..k..........k..",
    ".kyk........kyk.",
    ".kyyk.kkkk.kyyk.",
    "..kyrkrrrrkryk..",
    "...krrrrrrrrk...",
    "..krrwwrrrwwrk..",
    "..krwkkrrwkkrk..",
    "..krrwwrrrwwrk..",
    "..kRrrrrrrrrRk..",
    "..kRrkwkkwkrRk..",
    "...kRrrrrrrRk...",
    "....kRRRRRRk....",
    "...kok.kk.kok...",
    "..kook....kook..",
    "..kkkk....kkkk..",
]
IMP2 = IMP[:13] + [
    "....kok..kok....",
    "...kook..kook...",
    "...kkkk..kkkk...",
]
IMP_SPIT = list(IMP)
IMP_SPIT[9] = "..kRrrkkkkrrRk.."
IMP_SPIT[10] = "..kRrkooyykrRk.."
IMP_SPIT[11] = "...kRrkkkkrRk..."
IMP_ANGRY = {'r': 'o', 'R': 'r', 'o': 'y'}
spr('SPR_IMP1', grid(IMP))
spr('SPR_IMP2', grid(IMP2))
spr('SPR_IMP_ANGRY1', recolor(grid(IMP), IMP_ANGRY))
spr('SPR_IMP_ANGRY2', recolor(grid(IMP2), IMP_ANGRY))
spr('SPR_IMP_SPIT', grid(IMP_SPIT))
spr('SPR_BUBBLE_IMP1', bubble(shrink_slime(IMP)))
spr('SPR_BUBBLE_IMP2', bubble(shrink_slime(IMP), phase=1))
spr('SPR_BUBBLE_IMP_ANGRY', bubble(recolor(shrink_slime(IMP), IMP_ANGRY)))
iko = list(IMP)
iko[6] = "..krkwkrrrkwkrk."[:16]
iko[7] = "..krrwrrrrrwrrk."[:16]
iko[8] = "..krkwkrrrkwkrk."[:16]
iko = grid(iko)
spr('SPR_IMP_KO1', iko)
spr('SPR_IMP_KO2', [list(reversed(r)) for r in reversed(iko)])

# the imp's fireball, flying right (flipped for left)
FIRE1 = [
    "................",
    "................",
    "................",
    "................",
    "........kkkk....",
    "..r.kkkooyyyk...",
    ".r.koooyywwyyk..",
    "rrkoooyywwwwyk..",
    ".rkroooyywwwyk..",
    "..r.koooyyyyk...",
    "....kkkooookk...",
    ".......kkkk.....",
    "................",
    "................",
    "................",
    "................",
]
FIRE2 = [
    "................",
    "................",
    "................",
    "................",
    "........kkkk....",
    ".r..kkkooyyyk...",
    "..rkroooyywwyk..",
    ".rrkooyywwwwyk..",
    "r.koooyyywwwyk..",
    ".r..kooooyyyk...",
    "....kkkooookk...",
    ".......kkkk.....",
    "................",
    "................",
    "................",
    "................",
]
spr('SPR_FIRE1', grid(FIRE1))
spr('SPR_FIRE2', grid(FIRE2))

# power-ups: candy = rapid, long bubbles; shoe = run faster; star = knock out every enemy
spr('SPR_CANDY', grid([
    "................",
    "................",
    "................",
    "................",
    "..k.........k...",
    ".kek..kkkk..kek.",
    ".keekkewweekeek.",
    ".keeekewwEeeeek.",
    ".keeekeeEEeeeek.",
    ".keekkeEEEeekek.",
    ".kek..kkkk..kek.",
    "..k.........k...",
    "................",
    "................",
    "................",
    "................",
]))
spr('SPR_SHOE', grid([
    "................",
    "................",
    "................",
    "......kkkk......",
    ".....krrrrk.....",
    ".....krwwrk.....",
    ".....krrrrk.....",
    ".....krwwrkkk...",
    "....krrrrrrrrk..",
    "...krrrrrrrrrrk.",
    "..kwwrrrrrrrrrrk",
    "..kwwwwwwwwwwwwk",
    "..kllllllllllllk",
    "...kkkkkkkkkkkk.",
    "................",
    "................",
]))
spr('SPR_STAR', grid([
    "................",
    ".......kk.......",
    "......kyyk......",
    "......kyyk......",
    ".....kywyyk.....",
    "kkkkkkywyyykkkkk",
    "kyyyyyyyyyyyyyyk",
    ".kyyyykyykyyyyk.",
    "..kyyykyykyyyk..",
    "...kyyyyyyyyk...",
    "...kyyyyyyyyk...",
    "..kyyyyookyyyk..",
    "..kyyyok..koyyk.",
    ".kyyok......koyk",
    ".kok.........kok",
    ".kk...........kk",
]))
spr('SPR_PTS_1UP', mini_text('1UP', 'G'))

# the Slime King, boss of rounds 10, 20 and 30: one 16x32 hardware sprite (SPRITE_OPTION_32PX)
KING = [
    "................",
    "................",
    "..k....kk....k..",
    ".kyk..kyyk..kyk.",
    ".kyyk.kyyk.kyyk.",
    ".kyyykyyyykyyyk.",
    ".kyryyyyryyyyrk.",
    ".kyyyyyyyyyyyyk.",
    "..kkkkkkkkkkkk..",
    "..kPPPPPPPPPPk..",
    ".kPPPPPPPPPPPPk.",
    ".kPPwwwPPPwwwPk.",
    "kPPwwkkkPwwkkkPk",
    "kPPwwkkkPwwkkkPk",
    "kPPPwwwPPPwwwPPk",
    "kpPPPPPPPPPPPPpk",
    "kpPPPPPPPPPPPPpk",
    "kpPPkkkkkkkkPPpk",
    "kpPPkwkwwkwkPPpk",
    "kppPPkkkkkkPPppk",
    "kppPPPPPPPPPPppk",
    "kppppPPPPPPppppk",
    "kqpppppppppppppk",
    "kqqppppppppppqqk",
    "kqqqppppppppqqqk",
    "kqqqqppppppqqqqk",
    "kqqqqqqqqqqqqqqk",
    ".kqqqqqqqqqqqqk.",
    ".kqqqqqqqqqqqqk.",
    "..kqqqqqqqqqqk..",
    "...kkkkkkkkkk...",
    "................",
]
KING2 = KING[:8] + ["................"] + KING[8:31]   # body bobs 1 px, crown stays


def g32(rows):
    assert len(rows) == 32, len(rows)
    return grid(rows[:16]) + grid(rows[16:])


def tall(name, px32):
    spr(name, px32[:16])
    spr(None, px32[16:])


tall('SPR_KING1', g32(KING))
tall('SPR_KING2', g32(KING2))
tall('SPR_KING_HURT', recolor(g32(KING), {'P': 'A', 'p': 'a', 'q': 'R', 'w': 'y'}))

SPR_END = S
for i in range(256):
    if tiles[i] is None:
        tiles[i] = blank()

# ---------------------------------------------------------------- write gif + header
COLS = 16
img = Image.new('P', (16 * COLS, 16 * (SPR_END + COLS - 1) // COLS))
flat = []
for (_, rgb) in PAL:
    flat += list(rgb)
flat += [0] * (768 - len(flat))
img.putpalette(flat)
count = SPR_END
for i in range(count):
    tx, ty = (i % COLS) * 16, (i // COLS) * 16
    for y in range(16):
        for x in range(16):
            img.putpixel((tx + x, ty + y), tiles[i][y][x])
os.makedirs(os.path.join(ROOT, 'assets'), exist_ok=True)
img.save(os.path.join(ROOT, 'assets', 'tileset.gif'))

# 2x preview for humans
os.makedirs(os.path.join(ROOT, 'screenshots', 'assets'), exist_ok=True)
img.convert('RGB').resize((img.width * 3, img.height * 3), Image.NEAREST).save(os.path.join(ROOT, 'screenshots', 'assets', 'tileset_preview.png'))

with open(os.path.join(ROOT, 'src', 'gfx.h'), 'w') as f:
    f.write('/* Generated by tools/make_assets.py, do not edit */\n#ifndef GFX_H\n#define GFX_H\n\n')
    for name, idx in sorted(names.items(), key=lambda kv: kv[1]):
        f.write('#define %-24s %3d\n' % (name, idx))
    f.write('\n#define THEME_COUNT %d\n#define TILE_COUNT %d\n\n#endif\n' % (THEME_COUNT, count))
print('tiles used:', count)

# Shared SNES art, owned locally so the game remains standalone.
from snes_glyphs import install as install_snes_glyphs
install_snes_glyphs(ROOT, SPR_END)
