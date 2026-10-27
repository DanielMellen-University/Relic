#!/usr/bin/env python3
"""Random Relic programs for comparing `relic check` with selfhost/check.relic.

  python3 tools/fuzz_check.py SEED > prog.relic

Not part of ctest (it needs Python); run by hand after changing selfhost/check.relic:

  for s in $(seq 1 1000); do
    python3 tools/fuzz_check.py $s > /tmp/f.relic
    diff <(build/relic check /tmp/f.relic 2>&1; echo $?) \
         <(build/relic selfhost/check.relic /tmp/f.relic 2>&1; echo $?) > /dev/null || echo "differs: seed $s"
  done

The programs mix nested functions, blocks, loops, try, use, assignments and a
small pool of names (some are typos of others), so scopes, late names, names
used before their definition and "did you mean" hints all come up. Seeds 1 to
1500 agreed when selfhost/check.relic was written (1.0.27).
"""
import random,sys
names=['a','b','c','d','e','x','y','f','g','h','print','len','str','i','k','total','totl','pr','lne']
def expr(r,d=0):
    t=r.random()
    if d>2 or t<0.45: return r.choice(names)
    if t<0.55: return str(r.randint(0,9))
    if t<0.7: return f'{expr(r,d+1)} + {expr(r,d+1)}'
    if t<0.8: return f'{r.choice(names)}({expr(r,d+1)})'
    if t<0.85: return f'[{expr(r,d+1)}, {expr(r,d+1)}]'
    if t<0.9: return '{k: '+expr(r,d+1)+'}'
    if t<0.95: return f'{expr(r,d+1)}.k'
    return f'"s {{{expr(r,d+1)}}}"'
def block(r,depth,ind,infn,inloop):
    n=r.randint(1,4)
    out=[]
    for _ in range(n): out+=stmt(r,depth,ind,infn,inloop)
    return out
def stmt(r,depth,ind,infn,inloop):
    p='    '*ind
    t=r.random()
    if depth>3: t=r.random()*0.5
    if t<0.2: return [f'{p}let {r.choice(names[:12])} = {expr(r)}']
    if t<0.3: return [f'{p}print({expr(r)})']
    if t<0.37: return [f'{p}{r.choice(names[:12])} = {expr(r)}']
    if t<0.45:
        nm=r.choice(names[:12]); prm=r.sample(names[:12],r.randint(0,2))
        if r.random()<0.3: return [f'{p}fun {nm}({", ".join(prm)}) = {expr(r)}']
        return [f'{p}fun {nm}({", ".join(prm)})']+block(r,depth+1,ind+1,True,False)
    if t<0.55: 
        o=[f'{p}if {expr(r)}']+block(r,depth+1,ind+1,infn,inloop)
        if r.random()<0.4: o+=[f'{p}else']+block(r,depth+1,ind+1,infn,inloop)
        return o
    if t<0.62: return [f'{p}while {expr(r)}']+block(r,depth+1,ind+1,infn,True)
    if t<0.7: return [f'{p}for {r.choice(names[:12])} in {expr(r)}']+block(r,depth+1,ind+1,infn,True)
    if t<0.76:
        o=[f'{p}try']+block(r,depth+1,ind+1,infn,inloop)
        o+=[f'{p}catch {r.choice(names[:12])}' if r.random()<0.7 else f'{p}catch']+block(r,depth+1,ind+1,infn,inloop)
        return o
    if t<0.8: return [f'{p}use std/{r.choice(["math","os","strings"])}'+(f' as {r.choice(names[:12])}' if r.random()<0.4 else '')]
    if t<0.85: return [f'{p}return {expr(r)}']
    if t<0.9: return [f'{p}break' if r.random()<0.5 else f'{p}next']
    return [f'{p}print({expr(r)})']
if len(sys.argv) != 2 or not sys.argv[1].isdigit(): sys.exit(__doc__)
seed=int(sys.argv[1]); r=random.Random(seed)
print('\n'.join(block(r,0,0,False,False)))
