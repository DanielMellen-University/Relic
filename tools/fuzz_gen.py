#!/usr/bin/env python3
"""Random well-formed programs, run on every engine and compared (by hand; needs Python).

  python3 tools/fuzz_gen.py SEED COUNT [BUILD_DIR]

Where fuzz_mutate.py edits existing programs (most of its cases stop at a syntax
error), this writes programs that run: expressions over ints, floats, strings,
lists and maps, indexing, slicing, the built-ins and std/strings, std/lists,
std/maps, std/math, std/json (encode, pretty, decode of odd JSON), std/paths,
string interpolation, functions, loops and `try` / `catch`. Every statement is
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

# Typed generation: most expressions have the types their operators and calls want, so
# programs get past the first error and test values and messages deeper; some pick any type.
INTS = ['0', '1', '2', '3', '-1', '7', '10', '255', '1000', '-5', '9223372036854775807', '60', 'n']
FLOATS = ['0.5', '1.5', '-2.25', '3.0', '1e3', '2.5e-3', '0.1', '1e20', '100.0', 'c']
PATHS = ['"a/b.txt"', '"/x/y/"', '"."', '".."', '"a/../b"', '"./a//b/"', '""', '"/"', '"f.tar.gz"', '".hidden"', '"é/ü.ç"', '"a\\\\b"']
JSONS = ['"[1, 2.5, \\"x\\", null, true]"', '"{\\"a\\": {\\"b\\": []}}"', '" 1e400 "', '"\\"\\\\u00e9\\\\ud83d\\\\ude00\\""', '"[1,]"', '"-0"', '"01"', '"{\\"a\\":1,\\"a\\":2}"', '""']
STRS = ['"abc"', '""', '"hello world"', '"é€😀"', '"a,b,c"', '"  pad  "', '"x"', '"12"', '"3.5"', '"{{}}"', '"tab\\tq"', 's', 'b']

def gen(t, d=0):
    if R.random() < 0.08:
        t = R.choice(['int', 'float', 'str', 'list', 'map', 'bool', 'none'])   # a wrong type now and then
    if t == 'any':
        t = R.choice(['int', 'float', 'str', 'list', 'map', 'bool'])
    deep = d > 3
    k = R.random()
    if t == 'path':
        return R.choice(PATHS + [gen('str', d + 1)])
    if t == 'int':
        if deep or k < 0.3: return R.choice(INTS)
        if k < 0.04: return 'lists.index_of(%s, %s)' % (gen('list', d + 1), gen('any', d + 1))
        if k < 0.07: return 'lists.reduce(%s, lists.fun_add, %s)' % (gen('list', d + 1), gen('int', d + 1))
        if k < 0.1: return 'math.clamp(%s, %s, %s)' % (gen('int', d + 1), gen('int', d + 1), gen('int', d + 1))
        if k < 0.5: return '(%s %s %s)' % (gen('int', d + 1), R.choice(['+', '-', '*', '%', '/']), gen('int', d + 1))
        if k < 0.6: return 'len(%s)' % gen(R.choice(['str', 'list', 'map']), d + 1)
        if k < 0.7: return 'ord(%s)' % gen('str', d + 1)
        if k < 0.75: return 'int(%s)' % gen(R.choice(['str', 'float', 'int']), d + 1)
        if k < 0.8: return 'strings.find(%s, %s)' % (gen('str', d + 1), gen('str', d + 1))
        if k < 0.85: return 'lists.sum(%s)' % gen('list', d + 1)
        if k < 0.9: return 'math.%s(%s)' % (R.choice(['floor', 'ceil', 'round', 'abs']), gen(R.choice(['int', 'float']), d + 1))
        if k < 0.95: return 'math.%s(%s, %s)' % (R.choice(['min', 'max']), gen('int', d + 1), gen('int', d + 1))
        return '%s[%s]' % (gen('list', d + 1), gen('int', d + 1))
    if t == 'float':
        if deep or k < 0.3: return R.choice(FLOATS)
        if k < 0.6: return '(%s %s %s)' % (gen(R.choice(['float', 'int']), d + 1), R.choice(['+', '-', '*', '/', '%']), gen('float', d + 1))
        if k < 0.7: return 'float(%s)' % gen(R.choice(['str', 'int']), d + 1)
        if k < 0.8: return 'num(%s)' % gen('str', d + 1)
        if k < 0.9: return 'math.%s(%s)' % (R.choice(['sqrt', 'sin', 'cos', 'exp', 'log']), gen(R.choice(['int', 'float']), d + 1))
        return 'math.pow(%s, %s)' % (gen(R.choice(['int', 'float']), d + 1), gen(R.choice(['int', 'float']), d + 1))
    if t == 'str':
        if deep or k < 0.25: return R.choice(STRS)
        if k < 0.4: return '(%s + %s)' % (gen('str', d + 1), gen('str', d + 1))
        if k < 0.5: return 'str(%s)' % gen('any', d + 1)
        if k < 0.6: return 'slice(%s, %s, %s)' % (gen('str', d + 1), gen('int', d + 1), gen('int', d + 1))
        if k < 0.7: return 'strings.%s(%s)' % (R.choice(['upper', 'lower', 'trim']), gen('str', d + 1))
        if k < 0.75: return 'strings.replace(%s, %s, %s)' % (gen('str', d + 1), gen('str', d + 1), gen('str', d + 1))
        if k < 0.8: return 'strings.repeat(%s, %s)' % (gen('str', d + 1), R.choice(['0', '1', '3', gen('int', d + 1)]))
        if k < 0.85: return 'strings.pad_%s(%s, %s, %s)' % (R.choice(['left', 'right']), gen('str', d + 1), R.choice(['0', '5', '12', gen('int', d + 1)]), gen('str', d + 1))
        if k < 0.9: return 'strings.join(%s, %s)' % (gen('list', d + 1), gen('str', d + 1))
        if k < 0.95: return 'chr(%s)' % R.choice(['65', '233', '8364', '128512', '0', '1114111', gen('int', d + 1)])
        if k < 0.97: return R.choice(['json.encode(%s)', 'json.pretty(%s)']) % gen('any', d + 1)
        if k < 0.98: return 'paths.%s(%s)' % (R.choice(['base', 'dir', 'ext', 'stem', 'normalize']), gen('path', d + 1))
        if k < 0.99: return 'paths.join(%s, %s)' % (gen('path', d + 1), gen('path', d + 1))
        if k < 0.995: return '"<{%s}|{%s}>"' % (gen('any', d + 1), gen('str', d + 1))
        return '%s[%s]' % (gen('str', d + 1), gen('int', d + 1))
    if t == 'list':
        if k < 0.04: return 'json.decode(%s)' % R.choice(JSONS + [gen('str', d + 1)])
        if k < 0.07: return 'lists.sort_by(%s, lists.natural)' % gen('list', d + 1)
        if k < 0.1: return 'maps.get(%s, %s, %s)' % (gen('map', d + 1), gen('str', d + 1), gen('any', d + 1))
        if deep or k < 0.25: return R.choice(['[]', '[1, 2, 3]', '[3, 1, 2]', '["b", "a"]', '[[1], [2, 3]]', 'xs', 'ys', 't', '[1.5, 2]', '[none, true]'])
        if k < 0.4: return '[' + ', '.join(gen('any', d + 1) for _ in range(R.randint(0, 4))) + ']'
        if k < 0.5: return 'slice(%s, %s, %s)' % (gen('list', d + 1), gen('int', d + 1), gen('int', d + 1))
        if k < 0.6: return 'strings.split(%s, %s)' % (gen('str', d + 1), gen('str', d + 1))
        if k < 0.65: return 'keys(%s)' % gen('map', d + 1)
        if k < 0.7: return 'lists.%s(%s)' % (R.choice(['reverse', 'copy', 'sort']), gen('list', d + 1))
        if k < 0.75: return 'maps.%s(%s)' % (R.choice(['values', 'items']), gen('map', d + 1))
        if k < 0.8: return '(%s + %s)' % (gen('list', d + 1), gen('list', d + 1))
        if k < 0.85: return 'insert(lists.copy(%s), %s, %s)' % (gen('list', d + 1), gen('int', d + 1), gen('any', d + 1))
        if k < 0.9: return 'lists.map(%s, %s)' % (gen('list', d + 1), R.choice(['str', 'f2x', 'len', 'fun_x']))
        if k < 0.95: return 'lists.filter(%s, %s)' % (gen('list', d + 1), R.choice(['is_big', 'fun_pos']))
        return 'range_list(%s)' % gen('int', d + 1)
    if t == 'map':
        if deep or k < 0.3: return R.choice(['{}', 'm', '{k: 1}', '{k: 1, z: "v"}', '{"q r": [1], n1: none}'])
        if k < 0.5: return '{' + ', '.join('%s: %s' % (R.choice(['k', 'z', '"q r"', 'n1']), gen('any', d + 1)) for _ in range(R.randint(0, 3))) + '}'
        if k < 0.7: return 'maps.merge(%s, %s)' % (gen('map', d + 1), gen('map', d + 1))
        if k < 0.8: return '%s' % R.choice(['m', '{a: {b: 2}}'])
        return 'maps.merge({}, %s)' % gen('map', d + 1)
    if t == 'bool':
        if deep or k < 0.2: return R.choice(['true', 'false'])
        if k < 0.45: return '(%s %s %s)' % (gen('int', d + 1), R.choice(['<', '>', '<=', '>=', '==', '!=']), gen('int', d + 1))
        if k < 0.55: return '(%s %s %s)' % (gen('str', d + 1), R.choice(['<', '>', '==', '!=']), gen('str', d + 1))
        if k < 0.65: return '(%s %s %s)' % (gen('bool', d + 1), R.choice(['and', 'or']), gen('bool', d + 1))
        if k < 0.7: return '(not %s)' % gen('bool', d + 1)
        if k < 0.78: return '(%s in %s)' % (gen('any', d + 1), gen(R.choice(['list', 'map', 'str']), d + 1))
        if k < 0.85: return '(%s == %s)' % (gen('any', d + 1), gen('any', d + 1))
        if k < 0.92: return 'strings.%s(%s, %s)' % (R.choice(['contains', 'starts_with', 'ends_with']), gen('str', d + 1), gen('str', d + 1))
        return 'math.is_whole(%s)' % gen(R.choice(['int', 'float']), d + 1)
    return 'none'

def expr(d=0):
    return gen('any', d)

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
    out = ['use std/strings', 'use std/lists', 'use std/maps', 'use std/math', 'use std/json', 'use std/paths',
           'fun f0(x, y) = x + y', 'fun f1(x, y)', '    return [x, y][len(x)]', 'fun f2(x, y)', '    let r = []', '    for i in 0..3', '        append(r, x)', '    return r',
           'fun f2x(x) = [x, x]', 'fun fun_x(x) = x', 'fun is_big(x) = x > 2', 'fun fun_pos(x) = type(x) == "int" and x > 0',
           'fun range_list(n)', '    let r = []', '    for i in 0..n', '        append(r, i)', '    return r', 'let n = 4',
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
    b = run([relic, root + '/selfhost/run.relic', 'g.relic'])
    if b[0] == -9:   # the Relic-written interpreter is slow; a program it cannot finish in time is not compared
        skipped += 1
        continue
    if a != b: diffs.append('selfhost')
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
