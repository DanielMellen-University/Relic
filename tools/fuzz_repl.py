#!/usr/bin/env python3
"""Compares the host's REPL with selfhost/repl.relic on random scripts (by hand; needs Python).

    python3 tools/fuzz_repl.py SEED [COUNT] [path/to/relic]

Each script is a handful of lines picked at random from the REPL cases and the
goldens (LF or CRLF, with or without a last newline), fed on stdin in tests/golden.
Prints every script where stdout, stderr or the exit code differ (stops after 4).
1.0.31 ran 5000 scripts with no difference.
"""
import glob, os, random, subprocess, sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
count = int(sys.argv[2]) if len(sys.argv) > 2 else 300
relic = os.path.abspath(sys.argv[3]) if len(sys.argv) > 3 else os.path.join(root, "build", "relic")
random.seed(seed)
lines = []
for f in (glob.glob(root + "/tests/repl_cases/*.in") + glob.glob(root + "/tests/golden/repl_*.relic")
          + sorted(glob.glob(root + "/tests/golden/*.relic"))[:80]):
    lines += open(f, encoding="utf-8", errors="replace").read().split("\n")
lines = [l for l in lines if len(l) < 120]
cwd = root + "/tests/golden"
bad = 0
for t in range(count):
    s = "\n".join(random.choice(lines) for _ in range(random.randint(1, 14)))
    if random.random() < 0.7:
        s += "\n"
    if random.random() < 0.2:
        s = s.replace("\n", "\r\n")
    data = s.encode()
    a = subprocess.run([relic], input=data, capture_output=True, cwd=cwd, timeout=30)
    b = subprocess.run([relic, root + "/selfhost/repl.relic"], input=data, capture_output=True, cwd=cwd, timeout=120)
    if (a.stdout, a.stderr, a.returncode) != (b.stdout, b.stderr, b.returncode):
        bad += 1
        print("MISMATCH", t, repr(s[:300]))
        print("  host", a.returncode, a.stdout[:200], a.stderr[:300])
        print("  mine", b.returncode, b.stdout[:200], b.stderr[:300])
        if bad >= 4:
            break
print("done", count, "scripts,", bad, "differ")
sys.exit(1 if bad else 0)
