#!/usr/bin/env python3
"""group.py <frames file> <byte indexes, e.g. 5,7> [names] : fps and late frames grouped by those watched bytes"""
import sys, collections
idx = [int(x) for x in sys.argv[2].split(',')]
names = sys.argv[3].split(',') if len(sys.argv) > 3 else [str(i) for i in idx]
g = collections.defaultdict(lambda: [0, 0, 0, 0])
for l in open(sys.argv[1]):
    p = l.split(); v = int(p[1]); b = p[2:]
    k = tuple(int(b[i], 16) if i < len(b) else -1 for i in idx)
    r = g[k]; r[0] += 1; r[1] += v; r[2] += v > 1; r[3] = max(r[3], v)
print('%-30s %7s %7s %6s %5s' % (' '.join(names), 'frames', 'fps', 'late', 'worst'))
for k in sorted(g):
    f, vb, late, w = g[k]
    print('%-30s %7d %7.2f %6d %5d' % (' '.join(map(str, k)), f, 60.0 * f / vb, late, w))
