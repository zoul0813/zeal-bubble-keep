import sys, collections
f, cond = sys.argv[1], sys.argv[2]   # cond: python expr on b (list of ints)
rows = []
for l in open(f):
    p = l.split(); t = float(p[0]); v = int(p[1]); b = [int(x, 16) for x in p[2:]]
    if eval(cond): rows.append((t, v))
fr = len(rows); vb = sum(v for _, v in rows)
sec = collections.defaultdict(lambda: [0, 0])
for t, v in rows: sec[int(t)][0] += 1; sec[int(t)][1] += v
full = [60.0 * a / b for a, b in sec.values() if b >= 50]
h = collections.Counter(v for _, v in rows)
print('%s: play frames %d, %.1f fps avg, %.1f%% frames late, worst 1-s window %.0f fps, frames by vblanks %s' %
      (f, fr, 60.0 * fr / vb, 100.0 * sum(v > 1 for _, v in rows) / fr, min(full), dict(sorted(h.items()))))
