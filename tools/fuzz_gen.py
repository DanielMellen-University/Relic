#!/usr/bin/env python3
"""Random well-formed programs, run on every engine and compared (by hand; needs Python).

  python3 tools/fuzz_gen.py SEED COUNT [BUILD_DIR]

Where fuzz_mutate.py edits existing programs (most of its cases stop at a syntax
error), this writes programs that run: expressions over ints, floats, strings,
lists and maps, indexing, slicing, the built-ins and std/strings, std/lists,
std/maps, std/math, functions, loops and `try` / `catch`. Every statement is
wrapped in `try`, so an error shows up as its message and the program goes on.
Compares `relic` (VM) with `--tree`, `selfhost/run.relic` and the program compiled
to C (stdout, stderr, exit code). A mismatch prints the path of the program under
/tmp/relic_fuzz/gen_SEED/.
"""
import os, random, subprocess, sys

seed, count = int(sys.argv[1]), int(sys.argv[2])
root = os.getcwd()
build = os.path.abspath(sys.argv[3] if len(sys.argv) > 3 else 'build')
relic = build + '/relic'
random.seed(seed)
work = '/tmp/relic_fuzz/gen_%d' % seed
os.makedirs(work, exist_ok=True)

R = random.Random(seed)
NAMES = ['a', 'b', 'c', 'xs', 'ys', 'm', 's', 't']

def lit(d=0):
    k = R.random()
    if k < 0.25: return str(R.choice([0, 1, 2, 3, -1, 7, 10, 255, 1000, -5, 9223372036854775807, 100000]))
    if k < 0.4: return R.choice(['0.5', '1.5', '-2.25', '3.0', '1e3', '2.5e-3', '0.1', '1e20', '100.0'])
    if k < 0.6: return R.choice(['"abc"', '""', '"hello world"', '"é€😀"', '"a,b,c"', '"  pad  "', '"x"', '"12"', '"3.5"', '"{{}}"', '"tab\\tq"'])
    if k < 0.7: return R.choice(['true', 'false', 'none'])
    if d < 2 and k < 0.85: return '[' + ', '.join(expr(d + 1) for _ in range(R.randint(0, 4))) + ']'
    if d < 2: return '{' + ', '.join('%s: %s' % (R.choice(['k', 'z', '"q r"', 'n1']), expr(d + 1)) for _ in range(R.randint(0, 3))) + '}'
    return '1'

def expr(d=0):
    k = R.random()
    if d > 3 or k < 0.3: return lit(d)
    if k < 0.45: return R.choice(NAMES)
    if k < 0.65:
        return '(%s %s %s)' % (expr(d + 1), R.choice(['+', '-', '*', '/', '%', '<', '>', '<=', '>=', '==', '!=', 'and', 'or', 'in']), expr(d + 1))
    if k < 0.7: return '(%s %s)' % (R.choice(['-', 'not']), expr(d + 1))
    if k < 0.8: return '%s[%s]' % (expr(d + 1), expr(d + 1))
    if k < 0.85: return '%s.%s' % (expr(d + 1), R.choice(['k', 'z', 'n1', 'len', 'x']))
    b = R.choice([('len', 1), ('str', 1), ('int', 1), ('float', 1), ('num', 1), ('type', 1), ('keys', 1), ('ord', 1), ('chr', 1),
                  ('slice', 3), ('append', 2), ('pop', 1), ('insert', 3), ('remove', 2),
                  ('strings.find', 2), ('strings.contains', 2), ('strings.starts_with', 2), ('strings.ends_with', 2),
                  ('strings.upper', 1), ('strings.lower', 1), ('strings.trim', 1), ('strings.split', 2), ('strings.join', 2),
                  ('strings.replace', 3), ('strings.repeat', 2), ('strings.pad_left', 3), ('strings.pad_right', 3),
                  ('lists.sum', 1), ('lists.index_of', 2), ('lists.reverse', 1), ('lists.copy', 1), ('lists.sort', 1),
                  ('maps.values', 1), ('maps.items', 1), ('maps.get', 3), ('maps.merge', 2),
                  ('math.abs', 1), ('math.min', 2), ('math.max', 2), ('math.clamp', 3), ('math.floor', 1), ('math.ceil', 1),
                  ('math.round', 1), ('math.sqrt', 1), ('math.pow', 2), ('math.is_whole', 1)])
    return '%s(%s)' % (b[0], ', '.join(expr(d + 1) for _ in range(b[1] if R.random() < 0.93 else R.randint(0, 3))))

def stmt(ind):
    p = '    ' * ind
    k = R.random()
    if k < 0.3: return [p + 'print(%s)' % expr()]
    if k < 0.5: return [p + '%s = %s' % (R.choice(NAMES), expr())]
    if k < 0.58: return [p + '%s[%s] = %s' % (R.choice(NAMES), expr(1), expr(1))]
    if k < 0.63: return [p + 'let %s = %s' % (R.choice(NAMES), expr())]
    if k < 0.72:
        return [p + 'for i in %s' % R.choice(['0..4', '[%s, %s]' % (expr(2), expr(2)), expr(2), '1..%s' % expr(2)]),
                p + '    print(i, %s)' % expr(2)]
    if k < 0.78:
        return [p + 'if %s' % expr(1), p + '    print("then", %s)' % expr(2), p + 'else', p + '    print("else")']
    if k < 0.86:
        return [p + 'print(f%d(%s, %s))' % (R.randint(0, 2), expr(2), expr(2))]
    if k < 0.92:
        return [p + 'let w = 0', p + 'while w < %s' % R.choice(['3', expr(2)]), p + '    w = w + 1', p + '    if w > 5', p + '        break',
                p + 'print(w)']
    return [p + 'print(str(%s), type(%s))' % (expr(), expr())]

def program():
    out = ['use std/strings', 'use std/lists', 'use std/maps', 'use std/math',
           'fun f0(x, y) = x + y', 'fun f1(x, y)', '    return [x, y][len(x)]', 'fun f2(x, y)', '    let r = []', '    for i in 0..3', '        append(r, x)', '    return r',
           'let a = 1', 'let b = "s"', 'let c = 2.5', 'let xs = [1, 2, 3]', 'let ys = ["a", "b"]', 'let m = {k: 1, z: "v"}', 'let s = "hello"', 'let t = [[1, 2], [3]]']
    for _ in range(R.randint(10, 25)):
        out.append('try')
        for l in stmt(1): out.append(l)
        out.append('catch e')
        out.append('    print("error:", e.message)')
    return '\n'.join(out) + '\n'

def run(cmd):
    try:
        r = subprocess.run(cmd, capture_output=True, timeout=30, cwd=work, stdin=subprocess.DEVNULL)
        return (r.returncode, r.stdout, r.stderr)
    except subprocess.TimeoutExpired:
        return (-9, b'', b'')

bad = skipped = 0
for n in range(count):
    src = program()
    open(work + '/g.relic', 'w', encoding='utf-8', newline='').write(src)
    a = run([relic, 'g.relic'])
    if a[0] == -9:
        skipped += 1
        continue
    diffs = []
    if a != run([relic, '--tree', 'g.relic']): diffs.append('tree')
    if a != run([relic, root + '/selfhost/run.relic', 'g.relic']): diffs.append('selfhost')
    c = subprocess.run([build + '/relicc', 'g.relic', '-o', 'g.c'], capture_output=True, cwd=work)
    if c.returncode == 0:
        cc = subprocess.run(['cc', '-std=c99', '-O0', '-I', root + '/runtime/c', '-o', 'g', 'g.c', build + '/runtime/c/librelic_rt.a', '-lm'],
                            capture_output=True, cwd=work)
        if cc.returncode == 0 and run([work + '/g']) != a: diffs.append('compiled')
    if diffs:
        bad += 1
        q = '%s/bad%d.relic' % (work, bad)
        open(q, 'w', encoding='utf-8', newline='').write(src)
        print('MISMATCH', diffs, q)
        if bad >= 5: break
print('done: %d mismatches, %d skipped' % (bad, skipped))
sys.exit(1 if bad else 0)
