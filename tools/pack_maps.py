#!/usr/bin/env python3
"""Pack SDK-exported tiles and metadata from git-tracked assets/rounds/*.tmx."""
from tmx_helpers import *
a=options(); maps=load_maps('rounds/round[0-9][0-9].tmx','ROUND_COUNT',(20,15))
write(a.out/'rounds.ztm',b''.join(m.tiles(a) for m in maps))
objects_file(a,maps,['Player','Slime','Bat','Imp','Boss'],'rounds')
