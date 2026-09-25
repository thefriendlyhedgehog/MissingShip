"""Reproduce ML's one-word edits to vanilla objects by patching stock resources (MS-021).

Display lists and limbs reference other resources by path, so ML's own copies (extracted under ml_* names) can't be
renamed and shipped the way textures can. When ML changed only a word or two, patching the stock resource the same
way is the clean route. Each patch first checks the ML ROM for the exact edit it reproduces, so the build fails
loudly if an assumption stops holding. Pure data: drop-in compatible.

  object_vm   gBeamosBodyDL, gBeamosTopEyelidDL   first command G_RDPPIPESYNC -> G_ENDDL: ML draws neither
  object_sa   gSariaRightHandLimb                 display list gSariaRightHandDL -> gSariaRightHandAndOcarinaDL
"""
from mspaths import DEBUG_ROM, ML_ROM, OOT_O2R, PIPE, SOH
import os, re, struct, sys, zipfile
from yaz0 import get_file

MOD = os.path.join(PIPE, 'ml.otr')
XML = os.path.join(SOH, 'assets', 'xml', 'GC_MQ_PAL_F', 'objects')
ML_DMA = {'object_vm': 25, 'object_sa': 43}
DBG_NAMES = open(os.path.join(SOH, 'assets', 'extractor', 'filelists', 'dbg.txt')).read().split('\n')

def rom_file(path, index):
    rom = open(path, 'rb').read()
    return get_file(rom, *struct.unpack('>IIII', rom[0x12F70 + 16 * index:0x12F70 + 16 * index + 16]))

def offset_of(obj, name):
    return int(re.search(rf'Name="{name}"[^>]*Offset="(0x[0-9A-Fa-f]+)"', open(os.path.join(XML, obj + '.xml')).read())
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

with zipfile.ZipFile(MOD) as src, zipfile.ZipFile(MOD + '.tmp', 'w', zipfile.ZIP_DEFLATED) as z:
    for info in src.infolist():
        if info.filename not in out:
            z.writestr(info, src.read(info.filename))
    for path, data in out.items():
        z.writestr(path, data)
os.replace(MOD + '.tmp', MOD)
print(f'object patches: {len(out)} stock resources patched as ML did ({", ".join(p.split("/")[-1] for p in out)})')
