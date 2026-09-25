"""Alignment-tolerant instruction diff (difflib over relocation-masked text) for actors ML patched with
inserted/removed instructions. Usage: aligndiff.py 0xACTOR [mq_actor_id]"""
import sys, difflib, re
from ovldiff import load_pair
aid = int(sys.argv[1], 0); bid = int(sys.argv[2], 0) if len(sys.argv) > 2 else aid
m, b = load_pair(aid, bid)
def norm(s):   # mask branch targets and local addresses so a shift does not flag every branch
    s = re.sub(r'0x[0-9a-f]{3,}$', 'X', s) if s.split()[0].startswith('b') or s.split()[0] in ('j', 'jal') else s
    return s
A = [norm(x[1]) for x in m[4]]; B = [norm(x[1]) for x in b[4]]
sm = difflib.SequenceMatcher(a=B, b=A, autojunk=False)
for tag, i1, i2, j1, j2 in sm.get_opcodes():
    if tag == 'equal': continue
    print(f'--- {tag}: MQ +{i1*4:#x}..{i2*4:#x}  ML +{j1*4:#x}..{j2*4:#x}')
    for k in range(i1, i2): print(f'   MQ {k*4:05x}: {b[4][k][2]}')
    for k in range(j1, j2): print(f'   ML {k*4:05x}: {m[4][k][2]}')
