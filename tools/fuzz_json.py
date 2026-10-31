#!/usr/bin/env python3
"""Random std/json programs on every engine, checked against Python's json.

  python3 tools/fuzz_json.py SEED COUNT [BUILD_DIR [OLD_BUILD_DIR]]

Each program decodes JSON texts that Python's json module wrote (nested
objects and arrays, strings with quotes, backslashes, control characters and
characters past the BMP, ints, floats, true, false, null, random spacing and
\\u escapes), and texts with one character inserted, removed or changed. For
each it prints json.encode of the value, or the error, and whether decoding
json.encode and json.pretty of the value gives it back.

Two checks:
- every engine prints the same: the VM, --tree, the compiled program,
  selfhost/run.relic (every fourth program), and OLD_BUILD_DIR's relic and
  relicc when given;
- the VM agrees with Python: a text is accepted exactly when Python's strict
  json.loads accepts it, json.encode gives JSON that Python reads back as the
  same value, and both round trips give the value back. Texts whose value
  Relic cannot hold the same way (lone surrogates, ints past 2^53, inf) are
  only compared across engines.

Floats come from a short list, from random bits, and as random decimals,
so many need 16 or 17 significant digits to come back exactly.

With an OLD_BUILD_DIR older than 1.0.47, its output may differ from the new
one in two ways only, the two changes in 1.0.47: it reads a raw control
character inside a string where the new one says "control character in
string", and it writes a float that needs 16 or 17 digits with 15 (the same
float to 15 digits). Any other difference is reported.

Programs go in /tmp/relic_fuzz/json_SEED/. Not part of ctest (it needs Python
and a C compiler); run it by hand after changing std/json or what it uses.
"""
import json, math, os, random, struct, subprocess, sys

if len(sys.argv) < 3 or not (sys.argv[1].isdigit() and sys.argv[2].isdigit()):
    sys.exit(__doc__)
seed, count = int(sys.argv[1]), int(sys.argv[2])
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
build = os.path.abspath(sys.argv[3]) if len(sys.argv) > 3 else root + '/build'
old = os.path.abspath(sys.argv[4]) if len(sys.argv) > 4 else None
work = '/tmp/relic_fuzz/json_%d' % seed
os.makedirs(work, exist_ok=True)
random.seed(seed)

CHARS = ['a', 'b', 'Z', ' ', '"', '\\', '/', '\n', '\t', '\r', '\b', '\f', '\x01', '\x1f', '\x7f', 'é', '€', '\U0001D11E', '{', '}', '[', ']', ':', ',']
FLOATS = [0.5, -0.5, 0.1, 1.5, -2.25, 1e300, -1e-300, 5e-324, 123456.789, 2.0, -0.0, 1e21, 3.14159, 1e-7,
          0.1 + 0.2, 1.7976931348623157e308, 2.2250738585072014e-308, 1e16 + 2, 1.0000000000000002, 1e23]


def a_float():
    k = random.random()
    if k < 0.4:
        return random.choice(FLOATS)
    if k < 0.7:
        while True:  # any finite float
            x = struct.unpack('<d', struct.pack('<Q', random.getrandbits(64)))[0]
            if math.isfinite(x):
                return x
    return random.randrange(-10 ** 12, 10 ** 12) / random.choice([3, 7, 10, 100, 1000, 10 ** 9])


def a_string():
    return ''.join(random.choice(CHARS) for _ in range(random.randrange(0, 8)))


def a_value(depth):
    r = random.random()
    if depth > 3 or r < 0.45:
        k = random.random()
        if k < 0.25:
            return random.choice([random.randrange(-1000, 1000), 0, -1, 2 ** 53, -(2 ** 53), 9007199254740991])
        if k < 0.45:
            return a_float()
        if k < 0.75:
            return a_string()
        return random.choice([True, False, None])
    if r < 0.7:
        return [a_value(depth + 1) for _ in range(random.randrange(0, 5))]
    m = {}
    for _ in range(random.randrange(0, 5)):
        m[a_string()] = a_value(depth + 1)
    return m


def a_text():
    v = a_value(0)
    style = random.random()
    if style < 0.3:
        t = json.dumps(v)
    elif style < 0.5:
        t = json.dumps(v, ensure_ascii=False)
    elif style < 0.7:
        t = json.dumps(v, indent=random.choice([1, 2, '\t']))
    else:
        t = json.dumps(v, separators=(',', ':'), ensure_ascii=random.random() < 0.5)
    if random.random() < 0.3:
        t = random.choice(['', ' ', '\n', ' \t\r\n']) + t + random.choice(['', ' ', '\n'])
    if random.random() < 0.3:
        # one change: insert, remove or replace a character
        i = random.randrange(0, len(t) + 1)
        c = random.choice(['"', '\\', ',', ':', '[', ']', '{', '}', '0', '1', '-', '.', 'e', 'u', 'x', ' ', 't', 'n', '\x00'])
        k = random.random()
        if k < 0.4:
            t = t[:i] + c + t[i:]
        elif k < 0.7 and t:
            t = t[:i] + t[i + 1:]
        elif t:
            t = t[:i] + c + t[i + 1:]
    return t


def relic_literal(t):
    out = []
    for ch in t:
        o = ord(ch)
        if ch == '\\':
            out.append('\\\\')
        elif ch == '"':
            out.append('\\"')
        elif ch == '\n':
            out.append('\\n')
        elif ch == '\t':
            out.append('\\t')
        elif ch == '{':
            out.append('{{')
        elif ch == '}':
            out.append('}}')
        elif o < 32 or o == 127:
            out.append('\\u{%x}' % o)
        else:
            out.append(ch)
    return '"' + ''.join(out) + '"'


def program():
    texts = [a_text() for _ in range(random.randrange(4, 12))]
    out = ['use std/json']
    for t in texts:
        out.append('let t = ' + relic_literal(t))
        out.append('try\n    let v = json.decode(t)\n    let e = json.encode(v)\n    print("E", e)\n'
                   '    print("P", json.decode(e) == v, json.decode(json.pretty(v)) == v)\n'
                   'catch err\n    print("X", err.message)')
    return texts, '\n'.join(out) + '\n'


def comparable(v):
    # values Relic holds the same way Python does
    if isinstance(v, bool) or v is None:
        return True
    if isinstance(v, int):
        return abs(v) <= 2 ** 53
    if isinstance(v, float):
        return math.isfinite(v)
    if isinstance(v, str):
        return not any(0xD800 <= ord(c) <= 0xDFFF for c in v)
    if isinstance(v, list):
        return all(comparable(x) for x in v)
    return all(comparable(k) and comparable(x) for k, x in v.items())


def same(a, b):
    # equal as JSON values: 1 and 1.0 are the same number, -0.0 and 0 too
    if isinstance(a, bool) or isinstance(b, bool):
        return type(a) == type(b) and a == b
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        return a == b
    if isinstance(a, list) and isinstance(b, list):
        return len(a) == len(b) and all(same(x, y) for x, y in zip(a, b))
    if isinstance(a, dict) and isinstance(b, dict):
        return list(a.keys()) == list(b.keys()) and all(same(a[k], b[k]) for k in a)
    return a == b


def r15(v):
    # the value with each float as str() writes it (15 significant digits)
    if isinstance(v, float) and not isinstance(v, bool):
        return float('%.15g' % v)
    if isinstance(v, list):
        return [r15(x) for x in v]
    if isinstance(v, dict):
        return {k: r15(x) for k, x in v.items()}
    return v


def against_python(texts, out):
    lines = out.decode('utf-8', 'surrogateescape').split('\n')
    k = 0
    for t in texts:
        try:
            want = json.loads(t)
            ok = True
        except (ValueError, RecursionError):
            ok = False
        if k >= len(lines):
            return 'output ends early'
        line = lines[k]
        k += 1
        if line.startswith('X '):
            if ok and comparable(want):
                return 'Relic rejects what Python reads: %r (%s)' % (t, line)
            continue
        if not line.startswith('E '):
            return 'unexpected line %r' % line
        p = lines[k]
        k += 1
        if not ok:
            return 'Relic reads what Python rejects: %r' % t
        if not comparable(want):
            continue
        try:
            back = json.loads(line[2:])
        except ValueError:
            return 'json.encode wrote what Python cannot read: %r' % line
        if not same(back, want):
            return 'json.encode changed the value: %r became %r' % (t, line[2:])
        if p != 'P true true':
            return 'round trip lost the value: %r (%s)' % (t, p)
    return None


def records(texts, out):
    # the output as one entry per text: the X line, or the E and P lines
    lines = out.decode('utf-8', 'surrogateescape').split('\n')
    k = 0
    recs = []
    for t in texts:
        if k >= len(lines):
            return None
        if lines[k].startswith('X '):
            recs.append([lines[k]])
            k += 1
        else:
            recs.append(lines[k:k + 2])
            k += 2
    return recs


def loose_only(texts, new, old):
    # whether an older release differs only where 1.0.47 changed std/json
    if new[0] != old[0] or new[2] != old[2]:
        return False
    a, b = records(texts, new[1]), records(texts, old[1])
    if a is None or b is None:
        return False
    for t, x, y in zip(texts, a, b):
        if x == y:
            continue
        if x[0].startswith('X json: control character in string'):
            continue  # the old one read it; nothing else to compare
        if not (x[0].startswith('E ') and y[0].startswith('E ')):
            return False
        try:
            vx, vy = json.loads(x[0][2:]), json.loads(y[0][2:])
        except ValueError:
            return False
        if not same(r15(vx), vy):
            return False
    return True


def version(b):
    try:
        v = subprocess.run([b + '/relic', '--version'], capture_output=True, text=True).stdout.split()[-1]
        return tuple(int(x) for x in v.split('.'))
    except (OSError, ValueError, IndexError):
        return (0,)


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


old_loose = old is not None and version(old) < (1, 0, 47)
bad = 0
for n in range(count):
    texts, src = program()
    with open(work + '/m.relic', 'w') as f:
        f.write(src)
    want = run([build + '/relic', 'm.relic'])
    problem = None
    if want[0] != 0:
        problem = 'the VM failed: %r' % (want[2][:300],)
    else:
        problem = against_python(texts, want[1])
    if problem is None:
        got = {'--tree': run([build + '/relic', '--tree', 'm.relic']),
               'compiled': compiled(build, 'm')}
        if n % 4 == 0:  # the Relic-written interpreter is slow
            got['selfhost run'] = run([build + '/relic', root + '/selfhost/run.relic', 'm.relic'])
        if old:
            got['old VM'] = run([old + '/relic', 'm.relic'])
            got['old --tree'] = run([old + '/relic', '--tree', 'm.relic'])
            got['old compiled'] = compiled(old, 'old_m')
        for k, v in got.items():
            if k.startswith('old') and v != want and old_loose and loose_only(texts, want, v):
                continue
            if v != want:
                problem = '%s differs from the VM' % k
                break
    if problem:
        bad += 1
        path = '%s/fail_%d.relic' % (work, n)
        with open(path, 'w') as f:
            f.write(src)
        print('%s: %s' % (path, problem))
print('%d programs, %d mismatches' % (count, bad))
sys.exit(1 if bad else 0)
