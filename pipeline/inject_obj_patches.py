"""Reproduce ML's one-word edits to vanilla objects by patching stock resources (MS-021).

Display lists and limbs reference other resources by path, so ML's own copies (extracted under ml_* names) can't be
renamed and shipped the way textures can. When ML changed only a word or two, patching the stock resource the same
way is the clean route. Each patch first checks the ML ROM for the exact edit it reproduces, so the build fails
loudly if an assumption stops holding. Pure data: drop-in compatible.

  object_vm   gBeamosBodyDL, gBeamosTopEyelidDL   first command G_RDPPIPESYNC -> G_ENDDL: ML draws neither
  object_sa   gSariaRightHandLimb                 display list gSariaRightHandDL -> gSariaRightHandAndOcarinaDL
  gameplay_keep  gPlayerAnim_link_fighter_turn_kiruL_end / _kiruR_end   frame count 16 -> 13 / 14 (MS-026): ML cut the
              recovery after a sword turn short, and re-animated exactly the frames it kept: link_animetion's
              gPlayerAnimData_114420 / _114FB0 take ML's frames (the rest of link_animetion is MS-051)
  object_kw1  gMlKw1MouthFrownTex (new)           ML's custom code (hook 0x80602200, run by En_Ko's Update) copies
              0xE0 bytes over rows 12-18 of gKw1MouthTex once Link has the sword, turning the smile into a frown (MS-032).
              Object memory is ML's to patch but not ours, so the frown ships as its own texture and En_Ko swaps it in
"""
from mspaths import DEBUG_ROM, ML_ROM, OOT_O2R, PIPE, SOH
import os, re, struct, sys, zipfile
from yaz0 import get_file

MOD = os.path.join(PIPE, 'ml.otr')
XML = os.path.join(SOH, 'assets', 'xml', 'GC_MQ_PAL_F', 'objects')
ML_DMA = {'object_vm': 25, 'object_sa': 43, 'gameplay_keep': 349, 'link_animetion': 146, 'object_kw1': 62}
DBG_NAMES = open(os.path.join(SOH, 'assets', 'extractor', 'filelists', 'dbg.txt')).read().split('\n')

def rom_file(path, index):
    rom = open(path, 'rb').read()
    return get_file(rom, *struct.unpack('>IIII', rom[0x12F70 + 16 * index:0x12F70 + 16 * index + 16]))

def offset_of(obj, name):
    return int(re.search(rf'Name="{name}"[^>]*\sOffset="(0x[0-9A-Fa-f]+)"', open(os.path.join(XML, obj + '.xml')).read())
               .group(1), 16)

def fail(msg):
    print(f'object patch FAILED: {msg}')
    sys.exit(1)

oot = zipfile.ZipFile(OOT_O2R)
ml = {obj: rom_file(ML_ROM, i) for obj, i in ML_DMA.items()}
dbg = {obj: rom_file(DEBUG_ROM, DBG_NAMES.index(obj)) for obj in ML_DMA}
out = {}

# Beamos: ML makes both display lists end at their first command.
for name in ('gBeamosBodyDL', 'gBeamosTopEyelidDL'):
    off = offset_of('object_vm', name)
    if dbg['object_vm'][off:off + 8] != bytes.fromhex('e700000000000000') or \
       ml['object_vm'][off:off + 8] != bytes.fromhex('df00000000000000'):
        fail(f'{name}: expected PIPESYNC in debug, ENDDL in ML')
    path = f'objects/object_vm/{name}'
    res = oot.read(path)
    if res[4:8] != b'TLDO' or struct.unpack('<II', res[0x58:0x60]) != (0xE7000000, 0):
        fail(f'{path}: unexpected layout (want the 16-byte marker, then PIPESYNC at 0x58)')
    out[path] = res[:0x58] + struct.pack('<II', 0xDF000000, 0)

# Saria: ML points her right-hand limb at the hand holding the ocarina.
limb = offset_of('object_sa', 'gSariaRightHandLimb')
old_dl, new_dl = offset_of('object_sa', 'gSariaRightHandDL'), offset_of('object_sa', 'gSariaRightHandAndOcarinaDL')
if struct.unpack('>I', dbg['object_sa'][limb + 8:limb + 12])[0] != 0x06000000 | old_dl or \
   struct.unpack('>I', ml['object_sa'][limb + 8:limb + 12])[0] != 0x06000000 | new_dl:
    fail('gSariaRightHandLimb: expected its DL to move from gSariaRightHandDL to gSariaRightHandAndOcarinaDL')
path = 'objects/object_sa/gSariaRightHandLimb'
res = oot.read(path)
old = b'objects/object_sa/gSariaRightHandDL'
new = b'objects/object_sa/gSariaRightHandAndOcarinaDL'
old_ref, new_ref = struct.pack('<I', len(old)) + old, struct.pack('<I', len(new)) + new
if res.count(old_ref) != 1:
    fail(f'{path}: expected one length-prefixed reference to gSariaRightHandDL')
out[path] = res.replace(old_ref, new_ref)

# Link's sword-turn recoveries: ML shortened two gameplay_keep player-animation headers (MS-026). A header is
# { s16 frameCount; pad; u32 segment pointer into link_animetion }; stock stores type 1 (Link), then frameCount at 0x44.
for name, frames in (('gPlayerAnim_link_fighter_turn_kiruL_end', 13), ('gPlayerAnim_link_fighter_turn_kiruR_end', 14)):
    off = int(re.search(rf'Name="{name}" Offset="(0x[0-9A-Fa-f]+)"',
                        open(os.path.join(XML, 'gameplay_keep.xml')).read()).group(1), 16)
    d, m = dbg['gameplay_keep'][off:off + 8], ml['gameplay_keep'][off:off + 8]
    if struct.unpack('>h', d[:2])[0] != 16 or struct.unpack('>h', m[:2])[0] != frames or d[4:] != m[4:]:
        fail(f'{name}: expected frame count 16 -> {frames} with the same animation data')
    path = f'objects/gameplay_keep/{name}'
    res = oot.read(path)
    if res[4:8] != b'MNAO' or struct.unpack('<Ih', res[0x40:0x46]) != (1, 16):
        fail(f'{path}: unexpected layout (want a Link animation with 16 frames)')
    out[path] = res[:0x44] + struct.pack('<h', frames) + res[0x46:]
    # the frames it points at: 0x86 bytes (67 s16) per frame, big-endian in the ROM, little-endian in stock
    seg = struct.unpack('>I', m[4:])[0] & 0xFFFFFF
    dpath = f'misc/link_animetion/gPlayerAnimData_{seg:06X}'
    data = oot.read(dpath)
    n = struct.unpack('<I', data[0x40:0x44])[0]
    dd, md = dbg['link_animetion'][seg:seg + 2 * n], ml['link_animetion'][seg:seg + 2 * n]
    if data[4:8] != b'MAPO' or struct.unpack(f'<{n}h', data[0x44:]) != struct.unpack(f'>{n}h', dd):
        fail(f'{dpath}: stock is not the debug ROM\'s data')
    if md[frames * 0x86:] != dd[frames * 0x86:]:
        fail(f'{dpath}: ML changed frames past its new count of {frames}')
    out[dpath] = data[:0x44] + struct.pack(f'<{n}h', *struct.unpack(f'>{n}h', md))

# Kokiri frown (MS-032): the hook tests the word at row 12 of the mouth (0x102, vanilla's), then copies 0xE0 bytes
# from custom code 0x80602B00 over it. Check the hook still does exactly that, then ship the patched texture.
custom = rom_file(ML_ROM, 507)
mouth = offset_of('object_kw1', 'gKw1MouthTex')
hook = struct.unpack('>9I', custom[0x2270:0x2294])
if mouth != 0x160C or hook[1] != 0x2508160C or hook[3] != 0x34010102 or hook[7] != 0x252A00E0 or \
   custom[0x2290:0x2298] != bytes.fromhex('3c0b8060356b2b00'):
    fail('hook 0x80602200: expected it to copy 0xE0 bytes from 0x80602B00 to gKw1MouthTex + 0x180, guarded by 0x102')
if ml['object_kw1'] != dbg['object_kw1'] or ml['object_kw1'][mouth + 0x180:mouth + 0x184] != bytes.fromhex('00000102'):
    fail('object_kw1: expected ML\'s copy to be the debug ROM\'s, with the guard word in place')
path = 'objects/object_kw1/gKw1MouthTex'
res = oot.read(path)
if res[4:8] != b'XETO' or len(res) != 0x50 + 0x400 or res[0x50:] != dbg['object_kw1'][mouth:mouth + 0x400]:
    fail(f'{path}: expected the debug ROM\'s 32x32 ci8 mouth, raw, behind a 0x50-byte header')
tex = bytearray(res)
tex[0x50 + 0x180:0x50 + 0x260] = custom[0x2B00:0x2BE0]
out['objects/object_kw1/gMlKw1MouthFrownTex'] = bytes(tex)

with zipfile.ZipFile(MOD) as src, zipfile.ZipFile(MOD + '.tmp', 'w', zipfile.ZIP_DEFLATED) as z:
    for info in src.infolist():
        if info.filename not in out:
            z.writestr(info, src.read(info.filename))
    for path, data in out.items():
        z.writestr(path, data)
os.replace(MOD + '.tmp', MOD)
print(f'object patches: {len(out)} stock resources patched as ML did ({", ".join(p.split("/")[-1] for p in out)})')
