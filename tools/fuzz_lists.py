#!/usr/bin/env python3
"""Random list programs (append, remove and insert near both ends, pop, index, for, ==, print) on every engine.

  python3 tools/fuzz_lists.py SEED COUNT [BUILD_DIR [OLD_BUILD_DIR]]

Each program makes a few lists, runs a few hundred random operations on them
and prints them along the way: removes and inserts at the front, the back and
the middle, emptying from the front, out-of-range indexes under try, and
lists that hold each other.
It runs on the VM, --tree, selfhost/run.relic and the compiled program, and
on OLD_BUILD_DIR's relic and relicc when given (a build of an older commit),
and prints a program whose outputs differ. Programs go in
/tmp/relic_fuzz/lists_SEED/. Not part of ctest (it needs Python and a C
compiler); run it by hand after changing lists in src/interp/value.* or
runtime/c. Written for 1.0.42's list layout, checked against 1.0.41.
"""
import os, random, subprocess, sys

if len(sys.argv) < 3 or not (sys.argv[1].isdigit() and sys.argv[2].isdigit()):
    sys.exit(__doc__)
seed, count = int(sys.argv[1]), int(sys.argv[2])
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
build = os.path.abspath(sys.argv[3]) if len(sys.argv) > 3 else root + '/build'
old = os.path.abspath(sys.argv[4]) if len(sys.argv) > 4 else None
work = '/tmp/relic_fuzz/lists_%d' % seed
os.makedirs(work, exist_ok=True)
random.seed(seed)


def program():
    names = ['l0', 'l1', 'l2']
    out = ['let l0 = []', 'let l1 = [1, 2, 3]', 'let l2 = []', 'let t = 0', 'let ks = []']
    def lst():
        return random.choice(names)
    def idx(m):
        # mostly in range, near either end; sometimes out of range or negative
        r = random.random()
        if r < 0.4:
            return str(random.randrange(3))
        if r < 0.7:
            return 'len(%s) - %d' % (m, random.randrange(1, 4))
        if r < 0.9:
            return 'int(len(%s) / %d)' % (m, random.randrange(2, 5))
        return random.choice(['-1', 'len(%s)' % m, 'len(%s) + 1' % m, '99999'])
    for _ in range(random.randrange(50, 400)):
        m = lst()
        r = random.random()
        if r < 0.2:
            out.append('append(%s, %d)' % (m, random.randrange(100)))
        elif r < 0.27:
            out.append('for i in 0..%d\n    append(%s, "v{i}")' % (random.randrange(1, 80), m))
        elif r < 0.45:
            out.append('try\n    t = t + len(str(remove(%s, %s))) * 0 + 1\ncatch e\n    print(e.message)' % (m, idx(m)))
        elif r < 0.5:
            out.append('while len(%s) > %d\n    remove(%s, 0)' % (m, random.randrange(10), m))
        elif r < 0.6:
            out.append('try\n    insert(%s, %s, %d)\ncatch e\n    print(e.message)' % (m, idx(m), random.randrange(100)))
        elif r < 0.64:
            out.append('try\n    %s[%s] = "set"\ncatch e\n    print(e.message)' % (m, idx(m)))
        elif r < 0.68:
            out.append('try\n    print(%s[%s])\ncatch e\n    print(e.message)' % (m, idx(m)))
        elif r < 0.72 :
            out.append('if len(%s) > 0\n    pop(%s)' % (m, m))
        elif r < 0.78:
            out.append('print(%s)' % m)
        elif r < 0.82:
            out.append('print(len(%s), slice(%s, 1, 5), %s == %s)' % (m, m, m, lst()))
        elif r < 0.86:
            out.append('t = 0\nfor x in %s\n    t = t + 1\nprint(t)' % m)
        elif r < 0.89:
            out.append('%s = %s + [7, 8]' % (m, lst()))
        elif r < 0.92:
            out.append('append(%s, %s)' % (m, lst()))
        elif r < 0.95:
            out.append('print(str(%s) == str(%s))' % (m, m))
        else:
            out.append('%s = []' % m)
    out.append('print(l0)\nprint(l1)\nprint(l2)\nprint(t)')
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
