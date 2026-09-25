"""Verify every OTR-hash reference in the DLs of given objects resolves to a resource in the archives.

LUS display lists reference textures, vertices and sub-DLs by CRC64 of the resource path
(G_SETTIMG_OTR_HASH 0x20, G_DL_OTR_HASH 0x31, G_VTX_OTR_HASH 0x32; the hash is the NEXT 8-byte word).
A hash that matches nothing renders as garbage or crashes. A vertex load must also land on a vertex resource: a
hash that resolves to a texture (ZAPD folds overlapped vertices into an oversized palette) draws garbage too
(MS-129). Usage: checkhashes.py <mod.otr> [prefix...]
"""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', 'pipeline'))
from mspaths import OOT_O2R
import zipfile, struct, sys
def crc64(s):
    c=0xFFFFFFFFFFFFFFFF
    for b in s.encode():
        c^=b<<56
        for _ in range(8): c=((c<<1)^0x42F0E1EBA9EA3693)&0xFFFFFFFFFFFFFFFF if c&(1<<63) else (c<<1)&0xFFFFFFFFFFFFFFFF
    return c
mod=zipfile.ZipFile(sys.argv[1])
oot=zipfile.ZipFile(OOT_O2R)
where={crc64(n): (oot, n) for n in oot.namelist()}
where.update({crc64(n): (mod, n) for n in mod.namelist()})
known=set(where)
def rtype(h):
    z, n = where[h]
    return z.read(n)[4:8]
prefixes=sys.argv[2:]
bad=0; n=0
for name in mod.namelist():
    if not any(name.startswith(p) for p in prefixes): continue
    d=mod.read(name)
    if d[4:8] != b'TLDO': continue   # display list resources only ('ODLT' little-endian)
    body=d[0x48:]   # 0x40 header + 8-byte prefix; then 8-byte commands stored as two LE u32
    o = 0
    while o < len(body) - 15:
        op=body[o+3]
        if op in (0x33, 0x35, 0x36, 0x42):   # other LUS 128-bit commands: skip their second half
            o += 16; continue
        if op not in (0x20,0x31,0x32):
            o += 8; continue
        if True:
            for h in (struct.unpack('<Q',body[o+8:o+16])[0], struct.unpack('>Q',body[o+8:o+16])[0],
                      (struct.unpack('<I',body[o+8:o+12])[0]<<32)|struct.unpack('<I',body[o+12:o+16])[0]):
                if h in known: break
            else:
                h = None
            if h is not None and op == 0x32 and rtype(h) not in (b'RRAO', b'XTVO'):   # 'OARR' / 'OVTX' little-endian
                bad+=1; print('NOT A VERTEX ARRAY', name, hex(o), where[h][1])
            elif h is None:
                bad+=1; print('UNRESOLVED', name, hex(o), body[o:o+16].hex())
            n+=1
            o += 16
print(f'{n} hash references checked, {bad} unresolved')
