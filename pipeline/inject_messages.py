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
"""
import zipfile, struct, sys, shutil, os
sys.path.insert(0, '/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline')
from yaz0 import get_file

OOT = '/Volumes/MacMiniExternal/AI_Projects/MissingShip/home/oot.o2r'
MOD = '/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/ml.otr'
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

code = open('/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/ml_code.bin', 'rb').read()
rom = open('/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/tml_2_0.z64', 'rb').read()
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
shutil.copy(MOD, MOD + '.tmp')
with zipfile.ZipFile(MOD + '.tmp', 'a', zipfile.ZIP_DEFLATED) as zz:
    if RES not in zz.namelist():
        zz.writestr(RES, res)
os.replace(MOD + '.tmp', MOD)
print(f"message resource rebuilt: {count} messages, {replaced} replaced with ML text "
      f"({len(res)} bytes vs vanilla {len(d)})")
