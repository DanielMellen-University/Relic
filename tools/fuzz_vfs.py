#!/usr/bin/env python3
"""Random std/vfs programs (make_dir, write, remove, remove_tree, rename, list, is_dir) on every engine.

  python3 tools/fuzz_vfs.py SEED COUNT [BUILD_DIR [OLD_BUILD_DIR]]

Each program runs up to 300 random calls on one VFS over a few folders and
names (some folders get a hundred files), with failures under try, removes
while going through a listing, and prints listings and the whole tree.
It runs on the VM, --tree, selfhost/run.relic and the compiled program, and
on OLD_BUILD_DIR's relic and relicc when given (a build of an older commit),
and prints a program whose outputs differ. Programs go in
/tmp/relic_fuzz/vfs_SEED/. Not part of ctest (it needs Python and a C
compiler); run it by hand after changing std/vfs or std/maps. Written for 1.0.42's in-place removes in std/vfs, checked against 1.0.41.
"""
import os, random, subprocess, sys

if len(sys.argv) < 3 or not (sys.argv[1].isdigit() and sys.argv[2].isdigit()):
    sys.exit(__doc__)
seed, count = int(sys.argv[1]), int(sys.argv[2])
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
build = os.path.abspath(sys.argv[3]) if len(sys.argv) > 3 else root + '/build'
old = os.path.abspath(sys.argv[4]) if len(sys.argv) > 4 else None
work = '/tmp/relic_fuzz/vfs_%d' % seed
os.makedirs(work, exist_ok=True)
random.seed(seed)


def program():
    dirs = ['/a', '/a/b', '/c', '/home', '/a/b/d', '/tmp']
    out = ['use std/vfs', 'let fs = vfs.new()', 'let t = 0']
    for d in dirs:
        if random.random() < 0.8:
            out.append('vfs.make_dir(fs, "%s")' % d)
    def path():
        d = random.choice(dirs + ['/'])
        if random.random() < 0.6:
            return (d if d != '/' else '') + '/f%d' % random.randrange(random.choice([4, 40, 300]))
        return d
    def guard(stmt):
        return 'try\n    %s\ncatch e\n    print(e.message)' % stmt
    for _ in range(random.randrange(40, 300)):
        r = random.random()
        p = path()
        if r < 0.12:
            out.append(guard('vfs.make_dir(fs, "%s")' % random.choice(dirs)))
        elif r < 0.30:
            out.append(guard('vfs.write(fs, "%s", "x%d")' % (p, random.randrange(9))))
        elif r < 0.36:
            d = random.choice(dirs)
            out.append(guard('for i in 0..%d\n        vfs.write(fs, "%s/f{i}", "y")' % (random.randrange(1, 120), d)))
        elif r < 0.50:
            out.append(guard('vfs.remove(fs, "%s")' % p))
        elif r < 0.55:
            out.append(guard('vfs.remove_tree(fs, "%s")' % p))
        elif r < 0.65:
            out.append(guard('vfs.rename(fs, "%s", "%s")' % (p, path())))
        elif r < 0.78:
            out.append(guard('print(vfs.list(fs, "%s"))' % random.choice(dirs + ['/'])))
        elif r < 0.84:
            out.append(guard('print(vfs.exists(fs, "%s"), vfs.is_dir(fs, "%s"))' % (p, p)))
        elif r < 0.88:
            out.append(guard('print(vfs.read(fs, "%s"))' % p))
        elif r < 0.92:
            d = random.choice(dirs)
            out.append(guard('for n in vfs.list(fs, "%s")\n        if t %% 3 == 0\n            vfs.remove_tree(fs, "%s/" + n)\n        t = t + 1' % (d, d)))
        elif r < 0.96:
            out.append('print(fs)')
        else:
            out.append(guard('print(vfs.size(fs, "%s"))' % p))
    out.append('print(fs)')
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
