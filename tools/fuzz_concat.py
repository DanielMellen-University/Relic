#!/usr/bin/env python3
"""Random `x = x + t` programs on every engine (strings added in place since 1.0.43).

  python3 tools/fuzz_concat.py SEED COUNT [BUILD_DIR [OLD_BUILD_DIR]]

Each program adds to strings held in globals, locals, function parameters
and closures, with copies of them kept in other names, lists and maps, adds
a string to itself, nests `x + (slice(x, 0, 9) + t)`, mixes in numbers and lists, and
reads len, indexes and slices (characters past ASCII too) along the way.
It runs on the VM, --tree, selfhost/run.relic and the compiled program, and
on OLD_BUILD_DIR's relic and relicc when given (a build of an older commit),
and prints a program whose outputs differ. Programs go in
/tmp/relic_fuzz/concat_SEED/. Not part of ctest (it needs Python and a C
compiler); run it by hand after changing str_append_in_place, rl_add_to or
how the VM, tree-walker or cgen compile an assignment.
"""
import os, random, subprocess, sys

if len(sys.argv) < 3 or not (sys.argv[1].isdigit() and sys.argv[2].isdigit()):
    sys.exit(__doc__)
seed, count = int(sys.argv[1]), int(sys.argv[2])
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
build = os.path.abspath(sys.argv[3]) if len(sys.argv) > 3 else root + '/build'
old = os.path.abspath(sys.argv[4]) if len(sys.argv) > 4 else None
work = '/tmp/relic_fuzz/concat_%d' % seed
os.makedirs(work, exist_ok=True)
random.seed(seed)

NAMES = ['a', 'b', 'c']
PIECES = ['"x"', '"é"', '"€"', '"\\u{1D11E}"', '""', '"ab"', 'chr(226)', 'chr(130)', 'str(i)', '"{i}é"']


def piece(names):
    r = random.random()
    if r < 0.6:
        return random.choice(PIECES)
    if r < 0.75:
        return 'slice(%s, 0, 9)' % random.choice(names)
    if r < 0.85:
        return '(slice(%s, 0, 9) + %s)' % (random.choice(names), random.choice(PIECES))
    return 'slice(%s, 1, 3)' % random.choice(names)


def body(names, depth, ind):
    out = []
    for _ in range(random.randrange(3, 9)):
        r = random.random()
        x = random.choice(names)
        if r < 0.4:
            out.append('%s = %s + %s' % (x, x, piece(names)))
        elif r < 0.5:
            out.append('if len(%s) < 3000\n%s    %s = %s + %s' % (x, ind, x, x, x))
        elif r < 0.58:
            out.append('%s = %s' % (random.choice(names), x))
        elif r < 0.64:
            out.append('keep = keep + [%s]' % x)
        elif r < 0.68:
            out.append('box.v = %s' % x)
        elif r < 0.74:
            out.append('print(len(%s), %s[len(%s) - 1], slice(%s, len(%s) - 3, len(%s)))' % (x, x, x, x, x, x))
        elif r < 0.78:
            out.append('try\n%s    %s = %s + %s\n%scatch e\n%s    print(e.message)' % (ind, x, x, random.choice(['1', '[1]', 'none']), ind, ind))
        elif r < 0.84 and depth < 2:
            out.append('for i in 0..%d' % random.randrange(1, 60))
            out.extend('    ' + l for l in body(names, depth + 1, ind + '    '))
        elif r < 0.88 and depth < 2:
            out.append('if len(%s) %% 2 == 0' % x)
            out.extend('    ' + l for l in body(names, depth + 1, ind + '    '))
        elif r < 0.92:
            out.append('print(%s == %s, len(keep), len(box.v))' % (x, random.choice(names)))
        else:
            out.append('%s = "%s"' % (x, random.choice(['', 'q', 'é'])))
    return [l.replace('\n', '\n' + '') for l in out]


def program():
    out = ['let keep = []', 'let box = {v: ""}', 'let i = 0']
    for n in NAMES:
        out.append('let %s = %s' % (n, random.choice(['""', '"s"', '"é"', 'str(7)'])))
    out.append('fun add(p, q)')
    out.append('    p = p + q')
    out.append('    if len(p) < 3000')
    out.append('        p = p + p')
    out.append('    return p')
    out.append('fun local(n)')
    out.append('    let s = ""')
    out.append('    fun peek() = s')
    out.append('    for i in 0..n')
    out.append('        s = s + "{i}é"')
    out.append('        if i % 7 == 0')
    out.append('            keep = keep + [peek()]')
    out.append('    return s')
    for l in body(NAMES, 0, ''):
        out.append(l)
    out.append('print(add(a, b), len(local(%d)), keep, box.v)' % random.randrange(0, 50))
    for n in NAMES:
        out.append('print(%s, len(%s))' % (n, n))
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
