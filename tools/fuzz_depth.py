#!/usr/bin/env python3
"""Expressions and `else if` chains right at the 10,000-level limit, read by
every parser (by hand; needs Python).

  python3 tools/fuzz_depth.py SEED COUNT [BUILD_DIR]

Each case builds one expression from a leaf by random steps whose height is
known (a run of `+ a`, `.a`, `(1)` or `[0]` links, `-`, `not`, a list, a map,
a string with `{...}`, a call argument, an index), up to a total of 9,997 to
10,003 levels, or nests one `else if` chain in a branch of another so their
links add up near 10,000. `relic check`, selfhost/check.relic (the
Relic-written parser on the host) and relicc (the same parser compiled to C)
must give the same output and exit code, and the error must come exactly when
the count passes 10,000. A mismatch prints the case's path under
/tmp/relic_fuzz/depth_SEED/. It can run from any folder; BUILD_DIR defaults
to build/ in the source tree.
"""
import os, random, subprocess, sys

if len(sys.argv) < 3 or not (sys.argv[1].isdigit() and sys.argv[2].isdigit()):
    sys.exit(__doc__)
seed, count = int(sys.argv[1]), int(sys.argv[2])
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
build = os.path.abspath(sys.argv[3]) if len(sys.argv) > 3 else root + '/build'
work = '/tmp/relic_fuzz/depth_%d' % seed
os.makedirs(work, exist_ok=True)
R = random.Random(seed)
LIMIT = 10000

# Steps that wrap e and how many levels each adds. The ones that need e in
# brackets use parentheses, which count toward the separate 32-level limit.
def wrap(e):
    k = R.random()
    if k < 0.12: return '-(%s)' % e, 1
    if k < 0.2: return 'not (%s)' % e, 1
    if k < 0.32: return '[%s, a]' % e, 1
    if k < 0.42: return '{k: %s}' % e, 2
    if k < 0.52: return '"<{ %s }>"' % e, 1  # spaces: "{{" in a string is a brace
    if k < 0.64: return 'f(a, %s)' % e, 1
    if k < 0.74: return 'xs[%s]' % e, 1
    return '(%s)' % e, 0

LINKS = [' + a', ' * a', ' and a', ' or a', '.a', '(1)', '[0]']

def expression(target):
    e, h, wraps = 'a', 0, 0
    while h < target:
        left = target - h
        if wraps < 20 and R.random() < 0.5:
            e2, dh = wrap(e)
            if dh <= left:
                e, h = e2, h + dh
                wraps += 1
                continue
        n = R.randint(1, left) if R.random() < 0.4 else left
        if wraps < 20 and R.random() < 0.3:
            n = min(left, R.randint(1, 40))
        e, h = '(%s)%s' % (e, R.choice(LINKS) * n), h + n
        wraps += 1
    return e

def elif_chain(n, indent, inner):
    p = '    ' * indent
    out = [p + 'if x < 0', p + '    print(0)']
    for i in range(n):
        out += [p + 'else if x < 0', p + '    print(0)']
    out += [p + 'else'] + inner
    return out

def case():
    if R.random() < 0.7:
        t = LIMIT + R.randint(-3, 3)
        src = 'let a = 1\nlet xs = [0]\nfun f(p, q) = p\nlet y = %s\n' % expression(t)
        return src, t > LIMIT
    outer = R.randint(1, LIMIT)
    inner = LIMIT - outer + R.randint(-2, 2)
    if inner < 0:
        inner = 0
    body = elif_chain(inner, 1, ['        print(x)'])
    return 'let x = 3\n' + '\n'.join(elif_chain(outer, 0, body)) + '\n', outer + inner > LIMIT

def run(cmd):
    r = subprocess.run(cmd, capture_output=True, timeout=600, cwd=work)
    return (r.returncode, r.stdout, r.stderr.replace(b'\r\n', b'\n'))

bad = 0
for n in range(count):
    src, over = case()
    open(work + '/d.relic', 'w', newline='').write(src)
    a = run([build + '/relic', 'check', 'd.relic'])
    b = run([build + '/relic', root + '/selfhost/check.relic', 'd.relic'])
    c = run([build + '/relicc', 'd.relic', '-o', 'd.c'])
    msg = b'too deeply nested (limit 10000)' in a[2] or b"chain is too long (limit 10000)" in a[2]
    # relicc prints only the error, check also its warnings; both start with the same line
    first = lambda r: r[2].split(b'\n')[0]
    problems = []
    if a != b: problems.append('check.relic')
    if (a[0] == 1) != (c[0] == 1) or (c[0] == 1 and first(a) != first(c)): problems.append('relicc')
    if msg != over: problems.append('limit (wanted %s)' % ('an error' if over else 'no error'))
    if problems:
        bad += 1
        q = '%s/bad%d.relic' % (work, bad)
        open(q, 'w', newline='').write(src)
        print('MISMATCH', problems, q, first(a)[:120])
        if bad >= 5: break
print('done: %d mismatches' % bad)
sys.exit(1 if bad else 0)
