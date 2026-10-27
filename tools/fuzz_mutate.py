#!/usr/bin/env python3
"""Mutate the test programs a little and compare the Relic-written tools with the host.

  python3 tools/fuzz_mutate.py MODE SEED COUNT [BUILD_DIR]

MODE is one of
  tokens   relic --tokens against selfhost/tokens.relic (stdout, stderr, exit code)
  ast      relic --ast against selfhost/ast.relic
  check    relic check against selfhost/check.relic
  run      relic (VM) against --tree, selfhost/run.relic and, when relicc compiles it, the compiled program

Each case is a golden or check case with one to three small edits (a quote, a
bracket, a keyword, a multi-byte character, a control character, a digit ...).
A mismatch prints the mutated source's path under /tmp/relic_fuzz/MODE_SEED/,
so runs with different modes or seeds can go in parallel. Not part of
ctest (it needs Python); run it by hand after changing the lexer, parser, check,
errfmt or interp in selfhost/. It found the host lexer printing half a
character in "unknown escape" and the selfhost tokens/ast tools exiting 0 on
errors (1.0.29). It can run from any folder; BUILD_DIR defaults to build/ in the
source tree.
"""
import glob, os, random, re, subprocess, sys

if len(sys.argv) < 4 or sys.argv[1] not in ('tokens', 'ast', 'check', 'run') or not (sys.argv[2].isdigit() and sys.argv[3].isdigit()):
    sys.exit(__doc__)
mode, seed, count = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
build = os.path.abspath(sys.argv[4]) if len(sys.argv) > 4 else root + '/build'
relic = build + '/relic'
random.seed(seed)
work = '/tmp/relic_fuzz/%s_%d' % (mode, seed)  # one folder per mode and seed, so several runs can go at once
os.makedirs(work, exist_ok=True)
if not os.path.exists(work + '/modules'):
    for d in ('modules', 'lib'):
        subprocess.run(['cp', '-r', root + '/tests/golden/' + d, work + '/' + d], stderr=subprocess.DEVNULL)

files = glob.glob(root + '/tests/golden/*.relic') + glob.glob(root + '/tests/check_cases/*.relic')
if mode == 'run':  # programs that finish fast and touch nothing outside the folder
    skip = r'std_files|std_folders|std_vfs|os_time|clipboard|graphics|gc_cycles|cycles_and|c_demo|recursion|long_chain|factorial|repl_|std_time'
    files = [f for f in glob.glob(root + '/tests/golden/*.relic') if not re.search(skip, f) and not os.path.exists(f[:-6] + '.args')]
pool = ['"', '\\', '{', '}', '(', ')', '[', ']', '#', '\t', ' ', '\n', '\r', 'é', '日', '😀', '\x00', '\x01', '\x7f',
        '\xa0', '\u200b', '\u201c', '\ufeff', '1', '0', '.', 'e', '-', '+', '*', '/', '%', '<', '>', '=', '!', ':', ',',
        '..', 'and', 'or', 'not', 'fun', 'let', 'if', 'else', 'for', 'in', 'try', 'catch', 'while', 'return', 'break',
        'next', 'use', 'as', 'true', 'none', '""', "'", '@', '$', '[]', '{}', '1.5', '9223372036854775807', 'len', 'str']


def run(cmd, cwd=work):
    try:
        r = subprocess.run(cmd, capture_output=True, timeout=10, cwd=cwd, stdin=subprocess.DEVNULL)
        return (r.returncode, r.stdout, r.stderr)
    except subprocess.TimeoutExpired:
        return (-9, b'', b'')


bad = skipped = 0
for n in range(count):
    s = open(random.choice(files), encoding='utf-8', errors='replace').read()
    if len(s) > 3000:
        i = random.randrange(len(s) - 1500)
        s = s[i:i + 1500]
    for _ in range(random.randint(1, 3)):
        i = random.randrange(len(s) + 1)
        k = random.random()
        if k < 0.4:
            s = s[:i] + random.choice(pool) + s[i:]
        elif k < 0.7:
            s = s[:i] + s[i + random.randint(1, 3):]
        else:
            s = s[:i] + random.choice(pool) + s[i + 1:]
    open(work + '/m.relic', 'w', encoding='utf-8', newline='').write(s)
    diffs = []
    if mode in ('tokens', 'ast', 'check'):
        host = [relic, '--' + mode if mode != 'check' else 'check', 'm.relic']
        mine = [relic, root + '/selfhost/%s.relic' % mode, 'm.relic']
        if run(host) != run(mine):
            diffs.append(mode)
    else:
        a = run([relic, 'm.relic'])
        if a[0] == -9:
            skipped += 1
            continue
        if a != run([relic, '--tree', 'm.relic']):
            diffs.append('tree')
        if a != run([relic, root + '/selfhost/run.relic', 'm.relic']):
            diffs.append('selfhost')
        # a program that stops at a compile-time error reads differently once compiled, so those are left out
        if a[0] == 0 or (a[0] == 1 and b'error' in a[2] and b'not defined' not in a[2]):
            c = subprocess.run([build + '/relicc', 'm.relic', '-o', 'm.c'], capture_output=True, cwd=work)
            if c.returncode == 0:
                cc = subprocess.run(['cc', '-std=c99', '-O0', '-I', root + '/runtime/c', '-o', 'm', 'm.c',
                                     build + '/runtime/c/librelic_rt.a', '-lm'], capture_output=True, cwd=work)
                if cc.returncode == 0 and run([work + '/m']) != a:
                    diffs.append('compiled')
    if diffs:
        bad += 1
        q = '%s/bad%d.relic' % (work, bad)
        open(q, 'w', encoding='utf-8', newline='').write(s)
        print('MISMATCH', diffs, q)
        if bad >= 5:
            break
print('done: %d mismatches, %d skipped' % (bad, skipped))
