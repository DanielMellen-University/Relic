#!/usr/bin/env python3
"""Random strings.find / contains / split and lists.sort programs on every engine.

  python3 tools/fuzz_find_sort.py SEED COUNT [BUILD_DIR [OLD_BUILD_DIR]]

Each program builds texts from a small alphabet with characters past ASCII
(some over 1,024 characters, some like "aaa...ab"), looks for random parts
with strings.find, contains, split and replace, and sorts random lists with
lists.sort: ints, floats with NaN and inf, ints past 2^53 next to floats,
equal ints and floats (1 and 1.0), strings, and lists that cannot be sorted
(mixed types, lists, bools, not a list) under try.
It runs on the VM, --tree, selfhost/run.relic and the compiled program, and
on OLD_BUILD_DIR's relic and relicc when given (a build of an older commit),
and prints a program whose outputs differ. Programs go in
/tmp/relic_fuzz/find_sort_SEED/. Not part of ctest (it needs Python and a C
compiler); run it by hand after changing host_find, host_sort or their std
wrappers.
"""
import os, random, subprocess, sys

if len(sys.argv) < 3 or not (sys.argv[1].isdigit() and sys.argv[2].isdigit()):
    sys.exit(__doc__)
seed, count = int(sys.argv[1]), int(sys.argv[2])
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
build = os.path.abspath(sys.argv[3]) if len(sys.argv) > 3 else root + '/build'
old = os.path.abspath(sys.argv[4]) if len(sys.argv) > 4 else None
work = '/tmp/relic_fuzz/find_sort_%d' % seed
os.makedirs(work, exist_ok=True)
random.seed(seed)

ALPHA = ['a', 'b', 'c', 'ab', 'é', '€', '\\u{1D11E}', ' ', '\\n']


def text(short=False):
    r = random.random()
    if short or r < 0.5:
        return '"%s"' % ''.join(random.choice(ALPHA) for _ in range(random.randrange(0, 6 if short else 14)))
    if r < 0.75:
        return 'strings.repeat("%s", %d) + "%s"' % (random.choice(ALPHA[:4]), random.randrange(200, 1500), random.choice(['', 'b', 'é', 'ab']))
    return 'strings.repeat("%s", %d)' % (''.join(random.choice(ALPHA) for _ in range(random.randrange(1, 5))), random.randrange(1, 400))


def num():
    return random.choice(['0', '1', '-1', '1.0', '2', '2.0', '0.5', '-0.0', '0.0', 'float("nan")', 'float("inf")',
                          '-float("inf")', '9007199254740992', '9007199254740993', '9007199254740992.0',
                          '%d' % random.randrange(-50, 50), '%d.5' % random.randrange(-9, 9)])


def a_list():
    r = random.random()
    n = random.choice([0, 1, 2, 3, 5, 8, 17, 40, 130])
    if r < 0.4:
        return '[%s]' % ', '.join(num() for _ in range(n))
    if r < 0.7:
        return '[%s]' % ', '.join(text(True) for _ in range(n))
    if r < 0.8:
        return '[%s]' % ', '.join(random.choice([num(), text(True)]) for _ in range(max(n, 2)))
    if r < 0.9:
        return '[%s]' % ', '.join(random.choice(['[1]', '[]', 'true', 'none', '{a: 1}', num()]) for _ in range(random.choice([1, 2, 4])))
    return random.choice(['"cba"', '5', 'none', '{b: 1, a: 2}'])


def program():
    out = ['use std/strings', 'use std/lists', 'let t = ""', 'let xs = []']
    for _ in range(random.randrange(10, 40)):
        r = random.random()
        if r < 0.15:
            out.append('t = ' + text())
        elif r < 0.45:
            part = random.choice([text(True), 'slice(t, %d, %d)' % (random.randrange(0, 1500), random.randrange(0, 1600))])
            out.append('print(strings.find(t, %s), strings.contains(t, %s))' % (part, part))
        elif r < 0.55:
            out.append('try\n    print(len(strings.split(t, %s)), len(strings.replace(t, %s, "x")))\ncatch e\n    print(e.message)' % (text(True), text(True)))
        elif r < 0.6:
            out.append('try\n    print(strings.find(%s, %s))\ncatch e\n    print(e.message)' % (random.choice(['[1, 2, 3, 2]', '5', 'none', 't']), random.choice(['[2]', '[]', '"a"', '2'])))
        else:
            out.append('xs = ' + a_list())
            out.append('try\n    let s = lists.sort(xs)\n    print(s, s == xs, len(s))\ncatch e\n    print(e.message)')
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
