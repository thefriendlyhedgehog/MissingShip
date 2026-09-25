"""Port ML's custom objects with ZAPD and merge them into the mod archive.

ML replaced the contents of the vanilla object slots its custom actors use, so an actor port is
never just C -- the display lists it draws live in ML's object, at offsets that do not match
vanilla's. Each object gets an XML in xml/objects/ declaring only the display lists the ported
actor actually draws; ZAPD follows them and pulls in the vertices and textures they reference, so
we never have to describe a whole 54KB object by hand.

Requires the object's dma index to be named in filelists/dbg.txt to match the XML's File Name --
ms_cycle.sh rewrites that filelist every run, so the name is forced there (see ML_OBJECTS).
"""
from mspaths import PIPE, SOH, ZAPD, ZAPD_CWD, zapd_config
import subprocess, zipfile, shutil, os, sys

P    = PIPE
CWD  = ZAPD_CWD
OUT  = P + '/ml_obj.otr'
MOD  = P + '/ml.otr'

ALIASES = {
    'objects/object_gi_boomerang/gGiBoomerangDL':  'objects/object_ml_gi_hourglass/gMlGiHourglassDL',
    'objects/object_gi_gerudo/gGiGerudoCardDL':    'objects/object_ml_gi_dekupass/gMlGiDekuPassDL',
    'objects/object_gi_map/gGiStoneOfAgonyDL':     'objects/object_ml_gi_certificate/gMlGiCertificateDL',
    'objects/gameplay_keep/gDropBombTex':          'objects/object_ml_gkeep/gMlDropBombTex',
}


# --- raw gameplay_keep pointers -------------------------------------------------------------------------
# ML's display lists sometimes load a gameplay_keep texture by its N64 segment-4 address. ZAPD only warns
# ("hardcoded pointer") and leaves G_SETTIMG with the raw address, which LUS cannot resolve. Rewrite each one as
# G_SETTIMG_OTR_HASH pointing at the stock resource: same fmt/siz/width, 16 bytes instead of 8 (LUS reads
# display lists command by command until G_ENDDL, so growing one is safe).
import re, struct
GK_XML = SOH + '/assets/xml/GC_MQ_PAL_F/objects/gameplay_keep.xml'
GK_TEX = {int(m.group(2), 16): m.group(1) for m in
          re.finditer(r'<Texture Name="(\w+)"[^>]*Offset="(0x[0-9A-Fa-f]+)"', open(GK_XML).read())}

def crc64(text):
    c = 0xFFFFFFFFFFFFFFFF
    for b in text.encode():
        c ^= b << 56
        for _ in range(8):
            c = ((c << 1) ^ 0x42F0E1EBA9EA3693) & 0xFFFFFFFFFFFFFFFF if c & (1 << 63) else (c << 1) & 0xFFFFFFFFFFFFFFFF
    return c

def fix_raw_keep_pointers(data):
    if data[4:8] != b'TLDO':
        return data, 0
    out, body, n = bytearray(data[:0x48]), data[0x48:], 0
    o = 0
    while o + 8 <= len(body):
        w0, w1 = struct.unpack('<II', body[o:o+8])
        op = w0 >> 24
        if op == 0xFD and (w1 >> 24) == 0x04 and ((w1 & ~1) & 0xFFFFFF) in GK_TEX:
            h = crc64('objects/gameplay_keep/' + GK_TEX[(w1 & ~1) & 0xFFFFFF])
            out += struct.pack('<II', (0x20 << 24) | (w0 & 0xFFFFFF), 0)
            out += struct.pack('<II', h >> 32, h & 0xFFFFFFFF)
            n += 1
        elif op in (0x20, 0x31, 0x32, 0x33, 0x35, 0x36, 0x42):   # LUS 128-bit commands: copy both halves
            out += body[o:o+16]
            o += 8
        else:
            out += body[o:o+8]
        o += 8
    return bytes(out), n

if os.path.exists(OUT):
    os.remove(OUT)

r = subprocess.run([ZAPD, 'ed', '-se', 'OTR', '--otrfile', OUT, '--portVer', '9.1.1',
                    '-i', P + '/xml/objects', '-b', P + '/tml_zapd.z64',
                    '-fl', P + '/filelists', '-o', P + '/out',
                    '-osf', P + '/out', '-gsf', '0',
                    '-rconf', zapd_config()],
                   cwd=CWD, capture_output=True, text=True)
if r.returncode != 0 or not os.path.exists(OUT):
    print(f"object extraction FAILED (exit {r.returncode})")
    print(r.stdout[-2000:], r.stderr[-2000:])
    sys.exit(1)

bad = r.stdout.count('texDecl == nullptr')
if bad:
    print(f"  warning: {bad} undeclared textures -- add explicit <Texture> entries")

src = zipfile.ZipFile(OUT)
objs = [n for n in src.namelist() if n.startswith('objects/')]
ours = {n.split('/')[1] for n in objs}
# REPLACE, don't append: an object whose XML changed must not keep stale resources under the same
# names (append-only silently kept a broken display list after its fix). Rewrite the archive with every
# resource of the objects we regenerate dropped first, then add the fresh set.
with zipfile.ZipFile(MOD) as old, zipfile.ZipFile(MOD + '.tmp', 'w', zipfile.ZIP_DEFLATED) as z:
    for info in old.infolist():
        parts = info.filename.split('/')
        if parts[0] == 'objects' and len(parts) > 2 and parts[1] in ours:
            continue
        if info.filename in ALIASES:
            continue
        z.writestr(info, old.read(info.filename))
    added = fixed = 0
    for n in objs:
        data, k = fix_raw_keep_pointers(src.read(n)); fixed += k
        z.writestr(n, data); added += 1
    # Drop-in overrides: where ML only refilled a vanilla pickup model and the draw path is unchanged,
    # ship ML's display list under the VANILLA resource path, so stock SoH draws it with no code change.
    # The display list still references its vertices/textures under the ML object's own path.
    for van, ours_dl in ALIASES.items():
        if ours_dl in objs:
            z.writestr(van, src.read(ours_dl)); added += 1
        else:
            print(f"  warning: alias source {ours_dl} was not extracted")
os.replace(MOD + '.tmp', MOD)

names = sorted({n.split('/')[1] for n in objs})
print(f"  rewrote {fixed} raw gameplay_keep texture pointer(s) as resource hashes")
print(f"injected {added} object resources across {len(names)} object(s): {', '.join(names)}")
