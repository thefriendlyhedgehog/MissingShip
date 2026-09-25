"""Audit every Yes/No-style choice in the shipped text for MS-018 (B answers "no" / cancels).

Reads the message resource in pipeline/ml.otr (ML's text plus vanilla's untouched messages), finds each two- and
three-choice message (control codes 0x1B / 0x1C; the options are the box's last lines), and checks the LAST option,
which is what B selects by default. Messages whose last option isn't a refusal are listed. Each must be covered by
the exception table in engine/soh/src/code/z_message_PAL.c (sMlChoiceBException); the script checks that too.

Usage: audit_choices.py          (exits 1 if a non-refusal last option has no exception entry)
"""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', 'pipeline'))
from mspaths import PIPE, SOH
import re, struct, sys, zipfile

RES = 'text/nes_message_data_static/ntsc_nes_message_data_static'
CTRL_ARGS = {0x05: 1, 0x06: 1, 0x07: 2, 0x0C: 1, 0x0E: 1, 0x12: 2, 0x13: 1, 0x14: 1, 0x15: 3, 0x1E: 1}
REFUSAL = re.compile(r"^(no\b|nope|don'?t|do not|not\b|never ?mind|nothing|quit|cancel|leave|stop|forget|nah)", re.I)

def choices():
    d = zipfile.ZipFile(_os.path.join(PIPE, 'ml.otr')).read(RES)
    count = struct.unpack('<I', d[0x40:0x44])[0]
    off = 0x44
    for _ in range(count):
        mid = struct.unpack('<H', d[off:off + 2])[0]
        ln = struct.unpack('<I', d[off + 4:off + 8])[0]
        t = d[off + 8:off + 8 + ln]
        off += 8 + ln
        boxes, line, i, choice = [[]], '', 0, None
        while i < len(t):
            b = t[i]
            if b == 0x02: break
            if b == 0x01: boxes[-1].append(line); line = ''
            elif b in (0x04, 0x10): boxes[-1].append(line); line = ''; boxes.append([])
            elif b in (0x1B, 0x1C): choice = (2 if b == 0x1B else 3, len(boxes) - 1)
            elif 0x20 <= b < 0x7F: line += chr(b)
            i += 1 + CTRL_ARGS.get(b, 0)
        boxes[-1].append(line)
        if choice:
            n, bi = choice
            opts = [l.strip() for l in boxes[bi] if l.strip()][-n:]
            yield mid, opts

src = open(_os.path.join(SOH, 'src', 'code', 'z_message_PAL.c')).read()
table = src[src.find('sMlChoiceBException'):]
table = table[:table.find('};')]
covered = {int(m, 16) for m in re.findall(r'\{\s*(0x[0-9A-Fa-f]+)\s*,', table)}

total, missing = 0, []
for mid, opts in choices():
    total += 1
    if not REFUSAL.match(opts[-1]):
        mark = 'covered' if mid in covered else 'MISSING from sMlChoiceBException'
        print(f'{mid:#06x}  {" | ".join(opts):70}  {mark}')
        if mid not in covered: missing.append(mid)
print(f'{total} choice messages; {len(missing)} without a refusal last and without an exception entry')
sys.exit(1 if missing else 0)
