#!/usr/bin/env python3
"""
Writes the Bubble Keep music as ZMT (Zeal Music Tracker) files in assets/.

All tracks are original. Each pattern is 32 sixteenth steps on up to 3 voices; voice 3 is
never written, so the game's sound effects own it. The files load with ZGDK's zmt_rom_load()
and can also be opened in ZMT itself.

Step tokens (one per sixteenth):
  .      nothing (the previous note keeps sounding)
  -      note off
  C5 F#4 a note (octave 4 holds middle C, like ZMT)
  k s h  drum hits for a noise voice (kick, snare, hat); a hit is cut off on the next empty step
"""
import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "assets")

SEMI = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
NOTE_OFF = 108
WAV = {"square": 0, "triangle": 1, "saw": 2, "noise": 3}
VOL = {0: 0xF0, 25: 0xF2, 50: 0xF4, 75: 0xF6, 100: 0xF8}
# noise "pitch" only changes the colour of the hiss
DRUMS = {"k": 24, "s": 64, "h": 96}   # C2, E5, C8


def note(tok):
    name = tok[0]
    sharp = 1 if tok[1] == "#" else (-1 if tok[1] == "b" else 0)
    octave = int(tok[-1])
    return octave * 12 + SEMI[name] + sharp


def steps(line, drums=False):
    toks = line.split()
    assert len(toks) == 32, (len(toks), line)
    out = []
    for i, t in enumerate(toks):
        if t == ".":
            # cut drum hits after one step, they are noise and would ring forever
            if drums and i > 0 and toks[i - 1] in DRUMS:
                out.append(NOTE_OFF)
            else:
                out.append(None)
        elif t == "-":
            out.append(NOTE_OFF)
        elif drums:
            out.append(DRUMS[t])
        else:
            out.append(note(t))
    return out


def voice(wave, vol, line):
    """One voice of a pattern: waveform, volume (percent) and 32 step tokens."""
    return (WAV[wave], VOL[vol], steps(line, drums=(wave == "noise")))


def pattern_bytes(voices):
    out = bytearray()
    bitmap = 0
    for i, v in enumerate(voices):
        if v is not None:
            bitmap |= 1 << i
    out.append(bitmap)
    for v in voices:
        if v is None:
            continue
        wave, vol, notes = v
        cells = []
        step_bits = 0
        for i, n in enumerate(notes):
            hdr = 0
            data = []
            if n is not None:
                hdr |= 0x01
                data.append(n)
            if i == 0:
                # every pattern sets its own waveform and volume, so patterns and tracks can follow
                # each other in any order
                hdr |= 0x02 | 0x04
                data += [wave, vol]
            if hdr:
                step_bits |= 1 << i
                cells.append(bytes([hdr] + data))
        out += struct.pack("<I", step_bits)
        for c in cells:
            out += c
    return bytes(out)


def zmt(title, tempo, patterns, arrangement):
    """tempo is frames per quarter note (a step is tempo/4 frames). arrangement: pattern indexes."""
    assert tempo % 4 == 0
    assert len(patterns) <= 16 and len(arrangement) <= 64
    out = bytearray(b"ZMT\x00")
    out += title.ljust(12)[:12].encode("ascii")
    out.append(tempo)
    out.append(len(patterns))
    bits = (1 << len(arrangement)) - 1
    out += struct.pack("<II", bits & 0xFFFFFFFF, bits >> 32)
    for p in arrangement:
        out += bytes([p, 0xFF])
    for p in patterns:
        out += pattern_bytes(p)
    return bytes(out)


# ---------------------------------------------------------------------------
# "Keep Theme", the in-game loop. C major, 150 bpm, A B A C.
# ---------------------------------------------------------------------------
DRUM_BEAT = "k . h . s . h . k . k . s . h . k . h . s . h . k . k . s . s s"
DRUM_FILL = "k . h . s . h . k . k . s . h . k . s . k . s . s . s . s s s s"

KEEP = [
    [   # A: C | G
        voice("square", 50,  "E5 . G5 . C6 . . . B5 . G5 . E5 . - . D5 . F5 . B5 . . . A5 . G5 . D5 . - ."),
        voice("triangle", 100, "C3 . C4 . C3 . C4 . C3 . C4 . E3 . G3 . G2 . G3 . G2 . G3 . G2 . G3 . B2 . D3 ."),
        voice("noise", 25, DRUM_BEAT),
        None,
    ],
    [   # B: Am | F
        voice("square", 50,  "A5 . C6 . E6 . . . D6 . C6 . A5 . - . F5 . A5 . C6 . . . B5 . A5 . F5 . - ."),
        voice("triangle", 100, "A2 . A3 . A2 . A3 . A2 . A3 . C3 . E3 . F2 . F3 . F2 . F3 . F2 . F3 . A2 . C3 ."),
        voice("noise", 25, DRUM_BEAT),
        None,
    ],
    [   # C: F G | C, with a fill back into A
        voice("square", 50,  "F5 . A5 . G5 . F5 . E5 . D5 . E5 . G5 . C6 . . . G5 . E5 . C6 . . . . . - ."),
        voice("triangle", 100, "F2 . F3 . F2 . F3 . G2 . G3 . G2 . G3 . C3 . C4 . C3 . C4 . G2 . . . C3 . . ."),
        voice("noise", 25, DRUM_FILL),
        None,
    ],
]

# ---------------------------------------------------------------------------
# Title: slower, triangle melody over a soft square arpeggio. C Am | F G.
# ---------------------------------------------------------------------------
TITLE = [
    [
        voice("triangle", 100, "G5 . . . E5 . C5 . D5 . E5 . G5 . . . A5 . . . E5 . C5 . E5 . . . - . . ."),
        voice("square", 25,  "C5 E5 G5 E5 C5 E5 G5 E5 C5 E5 G5 E5 C5 E5 G5 E5 A4 C5 E5 C5 A4 C5 E5 C5 A4 C5 E5 C5 A4 C5 E5 C5"),
        voice("triangle", 75, "C3 . . . . . . . G2 . . . . . . . A2 . . . . . . . E2 . . . . . . ."),
        None,
    ],
    [
        voice("triangle", 100, "F5 . . . A5 . C6 . B5 . A5 . G5 . . . G5 . . . F5 . E5 . D5 . . . - . . ."),
        voice("square", 25,  "F4 A4 C5 A4 F4 A4 C5 A4 F4 A4 C5 A4 F4 A4 C5 A4 G4 B4 D5 B4 G4 B4 D5 B4 G4 B4 D5 B4 G4 B4 D5 B4"),
        voice("triangle", 75, "F2 . . . . . . . C3 . . . . . . . G2 . . . . . . . D3 . . . . . . ."),
        None,
    ],
]

# ---------------------------------------------------------------------------
# One-shot jingles, one pattern each. The game stops them when the pattern wraps.
# ---------------------------------------------------------------------------
READY = [[   # 3 frames a step: 96 frames, the length of the READY banner
    voice("square", 50,  "C5 . E5 . G5 . C6 . - . G5 . C6 . - . E6 . . . . . . . . . - . . . . ."),
    voice("triangle", 100, "C3 . . . G3 . . . C4 . . . . . . . C3 . . . . . . . . . - . . . . ."),
    voice("noise", 25,  "k . . . k . . . k . . . s . s . k . . . . . . . . . . . . . . ."),
    None,
]]

CLEAR = [[   # 4 frames a step: 128 frames, inside the 150 frame round-clear pause
    voice("square", 50,  "C6 . G5 . E5 . G5 . C6 . E6 . G6 . . . E6 . . . G6 . . . . . . . . . - ."),
    voice("triangle", 100, "E5 . E5 . C5 . E5 . E5 . G5 . C6 . . . C6 . . . E6 . . . . . . . . . - ."),
    voice("noise", 25,  "s . h . s . h . s . h . s s s . k . . . s . . . k . . . . . . ."),
    None,
]]

GAMEOVER = [[   # 6 frames a step: 192 frames
    voice("square", 50,  "G5 . . . F5 . . . D#5 . . . D5 . . . C5 . . . . . . . . . . . - . . ."),
    voice("triangle", 100, "C4 . . . B3 . . . A#3 . . . A3 . . . G#3 . . . G3 . . . . . . . - . . ."),
    None,
    None,
]]

TRACKS = {
    "keep.zmt": zmt("KEEP THEME", 24, KEEP, [0, 1, 0, 2]),
    "title.zmt": zmt("BUBBLE KEEP", 32, TITLE, [0, 1]),
    "ready.zmt": zmt("READY", 12, READY, [0]),
    "clear.zmt": zmt("ROUND CLEAR", 16, CLEAR, [0]),
    "gameover.zmt": zmt("GAME OVER", 24, GAMEOVER, [0]),
}

if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for name, data in TRACKS.items():
        with open(os.path.join(OUT, name), "wb") as f:
            f.write(data)
        print(f"{name}: {len(data)} bytes")
