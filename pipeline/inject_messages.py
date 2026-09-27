"""Port ML's message text.

SoH keeps all dialogue in ONE resource, `text/nes_message_data_static/ntsc_nes_message_data_static`
(type 'TXTO'), laid out as:
    0x00..0x40  standard resource header
    0x40        u32 count
    then per message: u16 id, u8 textBoxType, u8 textBoxPos, u32 length, length bytes of text

ML keeps vanilla's message SYSTEM but rewrote most of the text:
  * message table at ml_code 0x12e4c8, 2115 entries of { u16 textId, u8 typePos, u8 pad, u32 seg },
    ascending by id, seg is a segment-7 offset;
  * the text blob is **dma 96** (229664 B, ~89% printable) -- NOT dma 21, whose debug-filelist name
    `nes_message_data_static` is misleading exactly like dma 25 was for the minimaps.
  * typePos packs type in the high nibble and position in the low nibble.

Messages are terminated by 0x02 (MSG_END); slicing to the next entry's offset would include padding,
so each message is trimmed at its terminator before comparison and export.

Credits (MS-024): `text/staff_message_data_static/staff_message_data_static`, same resource layout. ML's staff table
(code 0x136908, 48 entries) is byte-identical to the MQ debug ROM's; its text blob is dma 110. A credit ships only if
ML's text differs from the debug ROM's (the baseline ML was built from): 11 do, ML's own staff and thanks.
"""
from mspaths import ML_ROM, OOT_O2R, PIPE, SOH
import zipfile, struct, sys, shutil, os
sys.path.insert(0, PIPE)
from yaz0 import get_file

OOT = OOT_O2R
MOD = PIPE + '/ml.otr'
RES = 'text/nes_message_data_static/ntsc_nes_message_data_static'
TABLE, NENT, BLOB_DMA = 0x12e4c8, 2115, 96

# OoT NES control codes that consume argument bytes. A naive buf.find(b'\x02') treats an
# ARGUMENT equal to 0x02 as MSG_END and truncates the message mid-text -- measured on 28 of
# ML's 2046 messages. Walk the codes instead so only a real 0x02 opcode ends the message.
CTRL_ARGS = {0x05: 1, 0x06: 1, 0x07: 2, 0x0C: 1, 0x0E: 1,
             0x12: 2, 0x13: 1, 0x14: 1, 0x15: 3, 0x1E: 1}

def trim(buf):
    i = 0
    while i < len(buf):
        b = buf[i]
        if b == 0x02:
            return buf[:i+1]
        i += 1 + CTRL_ARGS.get(b, 0)
    return buf

z = zipfile.ZipFile(OOT)
d = z.read(RES)
count = struct.unpack('<I', d[0x40:0x44])[0]
van, order, off = {}, [], 0x44
for _ in range(count):
    mid, typ, pos = struct.unpack('<HBB', d[off:off+4])
    ln = struct.unpack('<I', d[off+4:off+8])[0]
    van[mid] = (typ, pos, d[off+8:off+8+ln]); order.append(mid); off += 8 + ln

code = open(PIPE + '/ml_code.bin', 'rb').read()
rom = open(ML_ROM, 'rb').read()
E = []; p = 0x12f70
while True:
    a, b, c, e = struct.unpack('>IIII', rom[p:p+16])
    if a == 0 and b == 0 and c == 0 and e == 0: break
    E.append((a, b, c, e)); p += 16
blob = get_file(rom, *E[BLOB_DMA])

tbl = []
for k in range(NENT):
    o = TABLE + k*8
    tbl.append((struct.unpack('>H', code[o:o+2])[0], code[o+2],
                struct.unpack('>I', code[o+4:o+8])[0] & 0xFFFFFF))
ml = {}
for i, (tid, tp, seg) in enumerate(tbl):
    if seg >= len(blob): continue
    end = tbl[i+1][2] if i+1 < len(tbl) else len(blob)
    if end <= seg or end > len(blob): end = len(blob)
    ml[tid] = (tp >> 4, tp & 0xF, trim(blob[seg:end]))

body = bytearray(struct.pack('<I', count))
replaced = 0
for mid in order:
    typ, pos, txt = van[mid]
    if mid in ml and ml[mid][2] and ml[mid][2] != trim(txt):
        typ, pos, txt = ml[mid]; replaced += 1
    body += struct.pack('<HBB', mid, typ, pos) + struct.pack('<I', len(txt)) + txt

res = d[:0x40] + bytes(body)

# ---- credits (MS-024) ----
STAFF_RES = 'text/staff_message_data_static/staff_message_data_static'
STAFF_TABLE, STAFF_NENT, STAFF_DMA = 0x136908, 48, 110
import baseline                                   # ML-vs-debug answers, recorded for players' builds (MS-154)
baseline.begin('inject_messages')
if baseline.RECORD:
    dbg_code = baseline.debug_file('code')
    if code[STAFF_TABLE:STAFF_TABLE + 8 * STAFF_NENT] != dbg_code[STAFF_TABLE:STAFF_TABLE + 8 * STAFF_NENT]:
        sys.exit('staff message table differs from the debug ROM: offsets would not line up')
    dbg_staff = baseline.debug_file('staff_message_data_static')
ml_staff = get_file(rom, *E[STAFF_DMA])
stbl = [(struct.unpack('>H', code[STAFF_TABLE + 8 * k:STAFF_TABLE + 8 * k + 2])[0],
         struct.unpack('>I', code[STAFF_TABLE + 8 * k + 4:STAFF_TABLE + 8 * k + 8])[0] & 0xFFFFFF) for k in range(STAFF_NENT)]
staff_new = {}
for i, (tid, seg) in enumerate(stbl):
    end = stbl[i + 1][1] if i + 1 < len(stbl) else len(ml_staff)
    if baseline.value(f'credit 0x{tid:04X}', lambda: trim(ml_staff[seg:end]) != trim(dbg_staff[seg:end])):
        staff_new[tid] = trim(ml_staff[seg:end])
baseline.save()
sd = z.read(STAFF_RES)
scount = struct.unpack('<I', sd[0x40:0x44])[0]
sbody, off = bytearray(struct.pack('<I', scount)), 0x44
written = set()
for _ in range(scount):
    mid, typ, pos = struct.unpack('<HBB', sd[off:off+4])
    ln = struct.unpack('<I', sd[off+4:off+8])[0]
    txt = staff_new.get(mid, sd[off+8:off+8+ln]); off += 8 + ln
    written.add(mid)
    sbody += struct.pack('<HBB', mid, typ, pos) + struct.pack('<I', len(txt)) + txt
if set(staff_new) - written:
    sys.exit(f'credits with no stock message: {sorted(map(hex, set(staff_new) - written))}')
staff_res = sd[:0x40] + bytes(sbody)
shutil.copy(MOD, MOD + '.tmp')
with zipfile.ZipFile(MOD + '.tmp', 'a', zipfile.ZIP_DEFLATED) as zz:
    if RES not in zz.namelist():
        zz.writestr(RES, res)
    if STAFF_RES not in zz.namelist():
        zz.writestr(STAFF_RES, staff_res)
os.replace(MOD + '.tmp', MOD)
print(f"message resource rebuilt: {count} messages, {replaced} replaced with ML text "
      f"({len(res)} bytes vs vanilla {len(d)}); credits: {len(staff_new)} of {scount} replaced with ML text")
