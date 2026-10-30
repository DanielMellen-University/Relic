#!/usr/bin/env python3
"""Random map programs (set, remove, re-add, keys, for, len, ==, print) on every engine.

  python3 tools/fuzz_maps.py SEED COUNT [BUILD_DIR [OLD_BUILD_DIR]]

Each program makes a few maps with short keys (so keys come back after a
remove), runs a few hundred random operations on them and prints them along
the way, including removes in a for loop and removing missing keys under try.
It runs on the VM, --tree, selfhost/run.relic and the compiled program, and
on OLD_BUILD_DIR's relic and relicc when given (a build of an older commit),
and prints a program whose outputs differ. Programs go in
/tmp/relic_fuzz/maps_SEED/. Not part of ctest (it needs Python and a C
compiler); run it by hand after changing maps in src/interp/value.* or
runtime/c. Written for 1.0.41's map holes, checked against 1.0.40.
"""
import os, random, subprocess, sys

if len(sys.argv) < 3 or not (sys.argv[1].isdigit() and sys.argv[2].isdigit()):
    sys.exit(__doc__)
seed, count = int(sys.argv[1]), int(sys.argv[2])
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
build = os.path.abspath(sys.argv[3]) if len(sys.argv) > 3 else root + '/build'
old = os.path.abspath(sys.argv[4]) if len(sys.argv) > 4 else None
work = '/tmp/relic_fuzz/maps_%d' % seed
os.makedirs(work, exist_ok=True)
random.seed(seed)


def program():
    nkeys = random.choice([3, 8, 30, 200])
    names = ['m0', 'm1', 'm2']
    out = ['let m0 = {}', 'let m1 = {a: 1, b: 2}', 'let m2 = {}', 'let ks = none', 'let t = 0']
    def key():
        return '"k%d"' % random.randrange(nkeys)
    for _ in range(random.randrange(50, 400)):
        m = random.choice(names)
        r = random.random()
        if r < 0.35:
            out.append('%s[%s] = %d' % (m, key(), random.randrange(100)))
        elif r < 0.45:
            # fill a run of keys, so maps get big enough to compact
            a = random.randrange(nkeys)
            out.append('for i in %d..%d' % (a, a + random.randrange(1, 60)))
            out.append('    ' + m + '["k{i %% %d}"] = i' % nkeys)
        elif r < 0.65:
            out.append('try\n    t = t + remove(%s, %s)\ncatch e\n    print(e.message)' % (m, key()))
        elif r < 0.70:
            # remove every other key while looping over the map
            out.append('t = 0')
            out.append('for k in %s\n    if t %% 2 == 0\n        remove(%s, k)\n    t = t + 1' % (m, m))
        elif r < 0.73:
            out.append('for k in keys(%s)\n    remove(%s, k)' % (m, m))
        elif r < 0.80:
            out.append('print(%s)' % m)
        elif r < 0.85:
            out.append('print(len(%s), keys(%s))' % (m, m))
        elif r < 0.90:
            out.append('print(%s == %s, %s == %s)' % (m, random.choice(names), m, m))
        elif r < 0.93:
            out.append('print(str(%s))' % m)
        elif r < 0.96:
            out.append('try\n    print(%s.%s)\ncatch e\n    print(e.message)' % (m, 'k%d' % random.randrange(nkeys)))
        else:
            out.append('%s = {}' % m)
    out.append('print(m0)\nprint(m1)\nprint(m2)\nprint(t)')
    return '\n'.join(out) + '\n'


def run(cmd):
    try:
        r = subprocess.run(cmd, capture_output=True, timeout=60, cwd=work, stdin=subprocess.DEVNULL)
        return (r.returncode, r.stdout, r.stderr)
    except subprocess.TimeoutExpired:
        return ('timeout', b'', b'')


def compiled(b, name):
    c = subprocess.run([b + '/relicc', 'm.relic', '-o', name + '.c'], capture_output=True, cwd=work)
    if c.returncode:
        return ('relicc failed', c.stdout, c.stderr)
    cc = subprocess.run(['cc', '-std=c99', '-O1', '-I', root + '/runtime/c' if b == build else os.path.dirname(b) + '/runtime/c',
                         '-o', name, name + '.c', b + '/runtime/c/librelic_rt.a', '-lm'], capture_output=True, cwd=work)
    if cc.returncode:
        return ('cc failed', cc.stdout, cc.stderr)
    return run([work + '/' + name])


bad = 0
for n in range(count):
    src = program()
    with open(work + '/m.relic', 'w') as f:
        f.write(src)
    want = run([build + '/relic', 'm.relic'])
    got = {'--tree': run([build + '/relic', '--tree', 'm.relic']),
           'compiled': compiled(build, 'm')}
    if n % 4 == 0:  # the Relic-written interpreter is slow
        got['selfhost run'] = run([build + '/relic', root + '/selfhost/run.relic', 'm.relic'])
    if old:
        got['old VM'] = run([old + '/relic', 'm.relic'])
        got['old --tree'] = run([old + '/relic', '--tree', 'm.relic'])
        got['old compiled'] = compiled(old, 'old_m')
    for k, v in got.items():
        if v != want:
            bad += 1
            path = '%s/fail_%d.relic' % (work, n)
            with open(path, 'w') as f:
                f.write(src)
            print('%s differs from the VM: %s' % (k, path))
            break
print('%d programs, %d mismatches' % (count, bad))
sys.exit(1 if bad else 0)
