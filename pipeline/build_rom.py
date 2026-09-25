from mspaths import ML_ROM, PIPE
import struct, os, sys
sys.path.insert(0,PIPE)
from yaz0 import get_file

SRC=ML_ROM; DST=PIPE + '/tml_zapd.z64'; DMA=0x12f70
rom=bytearray(open(SRC,'rb').read())
E=[]; p=DMA
while True:
    vs,ve,rs,re=struct.unpack('>IIII',rom[p:p+16])
    if vs==0 and ve==0 and rs==0 and re==0: break
    E.append((vs,ve,rs,re)); p+=16

def cmds_of(d):
    out=[]; off=0
    while off+8<=len(d):
        c=d[off]; out.append((off, c, bytes(d[off:off+8])))
        if c==0x14: break
        if c>0x1A: break
        off+=8
    return out

def align4(d):
    if len(d)%4: d += b'\x00'*(4-(len(d)%4))
    return d

def mesh0_to_mesh2(d):
    """ZAPD treats meshHeaderType 0 as RoomShapeCullable; vanilla rooms are type 2.
    Rewrite type-0 (8-byte entries) as type-2 (16-byte entries, never culled)."""
    d=bytearray(d); cmdoff=None; H=None
    for off,c,raw in cmds_of(d):
        if c==0x0A: cmdoff=off; H=struct.unpack('>I',raw[4:8])[0]&0xFFFFFF
    if H is None or d[H]!=0: return bytes(d),0
    n=d[H+1]; start=struct.unpack('>I',d[H+4:H+8])[0]&0xFFFFFF
    ents=[struct.unpack('>II', d[start+i*8:start+i*8+8]) for i in range(n)]
    d=bytearray(align4(bytes(d))); base=len(d); eo=base+12
    d += struct.pack('>BBHII', 2, n, 0, 0x03000000|eo, 0x03000000|(eo+n*16))
    for opa,xlu in ents:
        d += struct.pack('>hhhhII', 0,0,0, 0x7FFF, opa, xlu)
    struct.pack_into('>I', d, cmdoff+4, 0x03000000|base)
    return bytes(d), n

def fix_settimg_fmt(d):
    """libultraship's G_SETTIMG_OTR_HASH handler decodes the texture using fmt/siz taken from the
    SETTIMG command word itself (interpreter.cpp: fmt=C0(21,3), size=C0(19,2)), NOT from the texture
    resource. ML's DLs carry the *load* format there (16b block-load), while the real format lives on
    render tile 0 -- so textures decode with the wrong bit depth and come out psychedelic. Rewrite
    each SETTIMG's fmt/siz to match the tile-0 descriptor that consumes it."""
    d=bytearray(d); patched=0
    mesh=None
    for off,c,raw in cmds_of(d):
        if c==0x0A: mesh=struct.unpack('>I',raw[4:8])[0]&0xFFFFFF
    if mesh is None: return bytes(d),0
    mt=d[mesh]; n=d[mesh+1]
    stride = 16 if mt==2 else 8
    start=struct.unpack('>I',d[mesh+4:mesh+8])[0]&0xFFFFFF
    def walk(a0, depth=0, seen=None):
        nonlocal patched
        if seen is None: seen=set()
        if a0==0 or (a0>>24)!=0x03: return
        a=a0&0xFFFFFF
        if a in seen or depth>6: return
        seen.add(a)
        pending=None
        while a+8<=len(d):
            w0,w1=struct.unpack('>II', d[a:a+8]); op=w0>>24
            if op==0xDF: break
            if op==0xFD:
                pending=a
            elif op==0xF5:                      # G_SETTILE
                tile=(w1>>24)&7
                if tile!=7 and pending is not None:
                    fmt=(w0>>21)&7; siz=(w0>>19)&3
                    pw0,=struct.unpack('>I', d[pending:pending+4])
                    new=(pw0 & ~((7<<21)|(3<<19))) | (fmt<<21) | (siz<<19)
                    if new!=pw0:
                        struct.pack_into('>I', d, pending, new); patched+=1
                    pending=None
            elif op==0xDE:
                walk(w1, depth+1, seen)
                if ((w0>>16)&0xFF)==0: break
            a+=8
    for i in range(n):
        q=start+i*stride
        if mt==2: opa,xlu=struct.unpack('>II', d[q+8:q+16])
        else:     opa,xlu=struct.unpack('>II', d[q:q+8])
        for x in (opa,xlu):
            if x: walk(x)

    return bytes(d), patched

def strip_actors(d):
    """ML actor IDs are meaningless to vanilla SoH: they spawn arbitrary vanilla actors which then
    draw effect DLs (e.g. gameplay_keep's dust motes) with segments they never set up, spraying
    garbage over the frame. Neutralise the actor list the same way as the alt-header command --
    overwrite cmd 0x01 IN PLACE with 0x16 (SetEchoSettings, echo=0). Zeroing the count instead makes
    ZAPD die with std::out_of_range, and shifting the list corrupts the file."""
    d=bytearray(d); n=0
    for off,c,raw in cmds_of(d):
        if c==0x01:
            d[off:off+8]=b'\x16'+b'\x00'*7; n+=1
    return bytes(d), n

# Formats excluded from scene-texture inlining. Some ML room DLs pair a SETTIMG with a
# LOADBLOCK/SETTILE combination whose sizeBytes < lineBytes; interpreter.cpp then computes
# height = sizeBytes / widthBytes == 0 and stock SoH 9.2.3 hands that straight to Metal
# (no zero-dimension guard), aborting. Before inlining these never resolved at all, so the
# importer bailed early and the bug stayed hidden.
SKIP_FMT=set()
# ML_SKIP_RANGE="a:b" skips inlining for per-room texture ordinals a <= i < b (bisect helper).
_sr=os.environ.get('ML_SKIP_RANGE','')
SKIP_RANGE=(int(_sr.split(':')[0]), int(_sr.split(':')[1])) if ':' in _sr else None
# i4 (fmt=4,siz=0) is skipped by default; its LOADBLOCK/SETTILE pairing yields sizeBytes < lineBytes.
for _p in os.environ.get('ML_SKIP_FMT','4:0').split(','):
    if ':' in _p:
        _f,_z=_p.split(':'); SKIP_FMT.add((int(_f),int(_z)))

BPP={ (0,0):0.5,(0,1):1,(0,2):2,(0,3):4,   # RGBA
      (2,0):0.5,(2,1):1,                    # CI
      (3,0):0.5,(3,1):1,(3,2):2,            # IA
      (4,0):0.5,(4,1):1 }                   # I

UNRESOLVED={}
ROOMLOCAL={}
INLINED={}

def inline_scene_textures(room, scene, roomname=None):
    """ZAPD resolves a room DL's segment-2 (scene) texture pointer to a declaration, hashes a path,
    but never exports that resource -- so all 67 refs are unresolvable at runtime (verified: vanilla's
    equivalent hashes DO resolve, ML's match nothing in the mod or the base otr). Sidestep it: copy
    each referenced texture/palette out of the scene file into the ROOM file and repoint the DL to
    segment 3, so it becomes a room-local reference on the path that already works."""
    d=bytearray(room); copied={}; patched=0
    manifest=INLINED.setdefault(roomname, [])
    tex_ord=[0]
    mesh=None
    for off,c,raw in cmds_of(d):
        if c==0x0A: mesh=struct.unpack('>I',raw[4:8])[0]&0xFFFFFF
    if mesh is None: return bytes(d),0
    mt=d[mesh]; n=d[mesh+1]; stride=16 if mt==2 else 8
    start=struct.unpack('>I',d[mesh+4:mesh+8])[0]&0xFFFFFF

    def place(src_off, nbytes):
        """copy nbytes from the scene into the room, return the new room offset"""
        key=(src_off,nbytes)
        if key in copied: return copied[key]
        nonlocal d
        if len(d)%8: d += b'\x00'*(8-(len(d)%8))
        new=len(d)
        d += scene[src_off:src_off+nbytes]
        copied[key]=new
        return new

    def walk(a0, depth=0, seen=None):
        nonlocal patched
        if seen is None: seen=set()
        if a0==0 or (a0>>24)!=0x03: return
        a=a0&0xFFFFFF
        if a in seen or depth>6: return
        seen.add(a)
        timg_pos=None; timg_fmt=None; timg_siz=None; tlut_pos=None
        rend_fmt=None; rend_siz=None   # from the RENDER SETTILE, not SETTIMG
        while a+8<=len(d):
            w0,w1=struct.unpack('>II', d[a:a+8]); op=w0>>24
            if op==0xDF: break
            if op==0xFD:
                timg_pos=a; timg_fmt=(w0>>21)&7; timg_siz=(w0>>19)&3
                if (w1>>24)==0x02: tlut_pos=a      # candidate palette source too
            elif op==0xF0 and tlut_pos is not None:   # G_LOADTLUT: previous SETTIMG was a palette
                pw0,pw1=struct.unpack('>II', d[tlut_pos:tlut_pos+8])
                if (pw1>>24)==0x02:
                    count=((w1>>14)&0x3FF)+1
                    novo=place(pw1&0xFFFFFF, count*2)
                    struct.pack_into('>I', d, tlut_pos+4, 0x03000000|novo); patched+=1
                    manifest.append({"kind":"tlut","off":novo,"count":count})
                tlut_pos=None
            elif op==0xF5:                            # G_SETTILE
                if ((w1>>24)&7)!=7:                   # 7 = G_TX_LOADTILE; anything else renders
                    rend_fmt=(w0>>21)&7; rend_siz=(w0>>19)&3
            elif op==0xF2:                            # SETTILESIZE -> gives real dimensions
                tile=(w1>>24)&7
                if tile!=7 and timg_pos is not None:
                    uls=(w0>>12)&0xFFF; ult=w0&0xFFF
                    lrs=(w1>>12)&0xFFF; lrt=w1&0xFFF
                    wdt=((lrs-uls)>>2)+1; hgt=((lrt-ult)>>2)+1
                    tw0,tw1=struct.unpack('>II', d[timg_pos:timg_pos+8])
                    if (tw1>>24)==0x02:
                        # THE FORMAT COMES FROM THE RENDER SETTILE, NEVER FROM SETTIMG.
                        # SETTIMG carries the *load* format (block loads are 16b), so reading
                        # fmt/siz from it declares every CI texture as rgba16/ia16 -- wrong bpp,
                        # wrong byte count, wrong decode. That is what made Lon Lon Ranch's walls
                        # streak. SETTIMG's siz still matters, but only for the LOADBLOCK math,
                        # which is why it must be left untouched.
                        eff_fmt = rend_fmt if rend_fmt is not None else timg_fmt
                        eff_siz = rend_siz if rend_siz is not None else timg_siz
                        bpp=BPP.get((eff_fmt,eff_siz))
                        tex_ord[0]+=1
                        if (eff_fmt,eff_siz) in SKIP_FMT:
                            bpp=None   # leave on the pre-inline path: see SKIP_FMT note
                        if SKIP_RANGE and SKIP_RANGE[0] <= tex_ord[0]-1 < SKIP_RANGE[1]:
                            bpp=None
                        if bpp:
                            nb=int(wdt*hgt*bpp+0.5)
                            novo=place(tw1&0xFFFFFF, nb)
                            struct.pack_into('>I', d, timg_pos+4, 0x03000000|novo)
                            # the renderer reads width straight out of the SETTIMG word
                            # (interpreter.cpp: width = C0(0,12)+1), so keep it consistent with the
                            # texture we just inlined instead of the block-load placeholder.
                            # NOTE: do NOT rewrite the SETTIMG width field. LOADBLOCK-style loads encode
                            # width=1 there on purpose; interpreter.cpp ImportTextureI4 derives
                            # height = sizeBytes / widthBytes from it, so a 'corrected' width drives
                            # height to 0 and trips the Metal texture-descriptor assert.
                            patched+=1
                            manifest.append({"kind":"tex","off":novo,"fmt":eff_fmt,
                                             "siz":eff_siz,"w":wdt,"h":hgt})
                    timg_pos=None
            elif op==0xDE:
                walk(w1, depth+1, seen)
                if ((w0>>16)&0xFF)==0: break
            a+=8

    for i in range(n):
        q=start+i*stride
        if mt==2: opa,xlu=struct.unpack('>II', d[q+8:q+16])
        else:     opa,xlu=struct.unpack('>II', d[q:q+8])
        for x in (opa,xlu):
            if x: walk(x)

    # DIAGNOSTIC: record every texture reference the inliner did not create, so the remaining
    # untextured surfaces can be attributed. Two kinds matter:
    #   seg 2  -> a scene texture we declined to inline (currently only i4).
    #   seg 3  -> a genuinely ROOM-LOCAL ML texture that ZAPD failed to auto-discover; it needs an
    #             explicit <Texture> declaration or the exporter emits its 0x08000001 fallback
    #             (this is what "texDecl == nullptr! PTR = 0x030014B8" in tent_room_0 was).
    known={m['off'] for m in manifest}
    def diag(a0, depth=0, seen=None):
        if seen is None: seen=set()
        if a0==0 or (a0>>24)!=0x03: return
        a=a0&0xFFFFFF
        if a in seen or depth>6: return
        seen.add(a)
        pend=None
        def flush():
            nonlocal pend
            if pend is None: return
            if pend['seg']==2:
                UNRESOLVED.setdefault(roomname,[]).append(pend)
            elif pend['seg']==3 and pend['addr'] not in known and pend.get('w'):
                ROOMLOCAL.setdefault(roomname,[]).append(pend)
            pend=None
        while a+8<=len(d):
            w0,w1=struct.unpack('>II', d[a:a+8]); op=w0>>24
            if op==0xDF: break
            if op==0xFD:
                flush()
                pend={"seg":w1>>24,"addr":w1&0xFFFFFF,"fmt":(w0>>21)&7,"siz":(w0>>19)&3,
                      "tiles":[],"settile":[],"tlut":None}
            elif pend is not None:
                if op==0xF2:
                    t=(w1>>24)&7; pend["tiles"].append(t)
                    if t!=7 and not pend.get('w'):
                        uls=(w0>>12)&0xFFF; ult=w0&0xFFF
                        lrs=(w1>>12)&0xFFF; lrt=w1&0xFFF
                        pend["w"]=((lrs-uls)>>2)+1; pend["h"]=((lrt-ult)>>2)+1
                elif op==0xF5:
                    t=(w1>>24)&7; pend["settile"].append(t)
                    if t!=7:
                        pend["fmt"]=(w0>>21)&7; pend["siz"]=(w0>>19)&3
            if op==0xDE:
                diag(w1, depth+1, seen)
                if ((w0>>16)&0xFF)==0: break
            a+=8
        flush()
    for i in range(n):
        q=start+i*stride
        if mt==2: opa,xlu=struct.unpack('>II', d[q+8:q+16])
        else:     opa,xlu=struct.unpack('>II', d[q:q+8])
        for x in (opa,xlu):
            if x: diag(x)

    _cand=[(int(m['w']*m['h']*BPP.get((m['fmt'],m['siz']),0)+0.5), m['off'])
           for m in manifest if m['kind']=='tex']
    fallback = max(_cand)[1] if _cand else None
    # SCRUB IS OFF BY DEFAULT and must stay that way. Repointing unresolved SETTIMG operands at a
    # fallback texture is what caused the Metal "MTLTextureDescriptor has height of zero" abort:
    # the DL still programs its own tile width, and stock SoH 9.2.3 computes
    # height = sizeBytes / widthBytes with no zero guard, so a fallback narrower than the tile
    # collapses height to 0. Leaving the pointer unresolved is safe -- the importer bails early.
    if not os.environ.get('ML_SCRUB'):
        fallback = None
    if fallback is not None:
        def scrub(a0, depth=0, seen=None):
            nonlocal patched
            if seen is None: seen=set()
            if a0==0 or (a0>>24)!=0x03: return
            a=a0&0xFFFFFF
            if a in seen or depth>6: return
            seen.add(a)
            while a+8<=len(d):
                w0,w1=struct.unpack('>II', d[a:a+8]); op=w0>>24
                if op==0xDF: break
                if op==0xFD and (w1>>24) not in (0x03,):
                    struct.pack_into('>I', d, a+4, 0x03000000|fallback); patched+=1
                elif op==0xDE:
                    scrub(w1, depth+1, seen)
                    if ((w0>>16)&0xFF)==0: break
                a+=8
        for i in range(n):
            q=start+i*stride
            if mt==2: opa,xlu=struct.unpack('>II', d[q+8:q+16])
            else:     opa,xlu=struct.unpack('>II', d[q:q+8])
            for x in (opa,xlu):
                if x: scrub(x)
    _cand=[(int(m['w']*m['h']*BPP.get((m['fmt'],m['siz']),0)+0.5), m['off'])
           for m in manifest if m['kind']=='tex']
    fallback = max(_cand)[1] if _cand else None
    # SCRUB IS OFF BY DEFAULT and must stay that way. Repointing unresolved SETTIMG operands at a
    # fallback texture is what caused the Metal "MTLTextureDescriptor has height of zero" abort:
    # the DL still programs its own tile width, and stock SoH 9.2.3 computes
    # height = sizeBytes / widthBytes with no zero guard, so a fallback narrower than the tile
    # collapses height to 0. Leaving the pointer unresolved is safe -- the importer bails early.
    if not os.environ.get('ML_SCRUB'):
        fallback = None
    if fallback is not None:
        def scrub(a0, depth=0, seen=None):
            nonlocal patched
            if seen is None: seen=set()
            if a0==0 or (a0>>24)!=0x03: return
            a=a0&0xFFFFFF
            if a in seen or depth>6: return
            seen.add(a)
            while a+8<=len(d):
                w0,w1=struct.unpack('>II', d[a:a+8]); op=w0>>24
                if op==0xDF: break
                if op==0xFD and (w1>>24) not in (0x03,):
                    struct.pack_into('>I', d, a+4, 0x03000000|fallback); patched+=1
                elif op==0xDE:
                    scrub(w1, depth+1, seen)
                    if ((w0>>16)&0xFF)==0: break
                a+=8
        for i in range(n):
            q=start+i*stride
            if mt==2: opa,xlu=struct.unpack('>II', d[q+8:q+16])
            else:     opa,xlu=struct.unpack('>II', d[q:q+8])
            for x in (opa,xlu):
                if x: scrub(x)
    return bytes(d), patched

def fix_objects(d, obj_id=1):
    """ML room object lists reference ML object IDs; SoH resolves them against VANILLA's object
    table, yielding invalid entries and a poison-pointer crash in func_80096FE8 (object bank init).
    Rewrite every entry to object 1 (gameplay_keep), which is always resident. Keep the count
    intact -- zeroing counts makes ZAPD die with std::out_of_range."""
    d=bytearray(d); total=0
    for off,c,raw in cmds_of(d):
        if c==0x0B:
            n=raw[1]; base=struct.unpack('>I',raw[4:8])[0]&0xFFFFFF
            for i in range(n):
                q=base+i*2
                if q+2<=len(d):
                    struct.pack_into('>H', d, q, obj_id); total+=1
    return bytes(d), total

def fix_altheaders(d, seg, count=4):
    """SoH indexes headers[sceneLayer-1] with NO bounds check, so ML's short alt-header list
    crashes it. Removing cmd 0x18 by shifting the command list corrupts the file (proved by a
    vanilla control that crashed in Cutscene_ProcessCommands). Instead overwrite 0x18 IN PLACE
    with 0x16 (SetEchoSettings, echo=0) -- pointer-free and harmless. Same length,
    same layout, no dangling references."""
    d=bytearray(d); n=0
    for off,c,raw in cmds_of(d):
        if c==0x18:
            d[off:off+8] = b'\x16' + b'\x00'*7   # SetEchoSettings(echo=0): pointer-free,
                                                  # supported by SceneFactory, harmless at runtime
            n+=1
    return bytes(d), n

def strip_cutscenes(d):
    """Neutralise scene header cmd 0x17 (cutscene list).

    The title/attract demo runs a scene cutscene. ML's cutscene data is written for ML's own
    rewritten cutscene dispatcher, and vanilla SoH's Cutscene_ProcessCommands walks it forever --
    the game HANGS on the attract demo (frozen frame, ~6% CPU spin, log stops, input ignored)
    while the warp-point boot path, which never runs a cutscene, is fine.
    Same in-place 0x16 trick as cmd 0x01/0x18: never shift the command list."""
    d=bytearray(d); n=0
    for off,c,raw in cmds_of(d):
        if c==0x17:
            d[off:off+8] = b'\x16' + b'\x00'*7
            n+=1
    return bytes(d), n

def stub_cutscenes(d):
    """Replace each scene's cutscene script with a minimal, VALID one that keeps its destination.

    ML's cutscene scripts are written for ML's own rewritten dispatcher; vanilla SoH's
    Cutscene_ProcessCommands loops on them forever (black screen, music still playing because audio
    is a separate thread). Deleting cmd 0x17 instead gives a null-deref crash, and SoH's
    SkipCutscene.Intro toggle avoids the hang but ALSO throws away what the cutscene was for:
    ML's Link's House (slot 52) is a 128-byte STAGING STUB whose intro cutscene does
    CS_DESTINATION to entrance 119 = scene 54 (malon_stable) -- the real start of the game.
    Skipping it strands the player in an empty black room.

    So: rewrite the script IN PLACE (never change its length or ZAPD's layout assumptions) as
        CS_BEGIN_CUTSCENE(1, 2); CS_DESTINATION(dest, 1, 2); CS_END
    which the vanilla dispatcher runs and exits on frame 1, performing ML's intended transition.
    Scenes whose script has no destination get an empty CS_BEGIN_CUTSCENE(0, 1); CS_END.
    """
    d=bytearray(d); n=0
    for off,c,raw in cmds_of(d):
        if c!=0x17: continue
        cs=struct.unpack('>I', raw[4:8])[0]&0xFFFFFF
        if cs==0 or cs+32>len(d): continue
        # find CS_DESTINATION (0x03E8) in the original script
        dest=None; a=cs+8
        while a+16<=len(d) and a < cs+8192:
            w=struct.unpack('>I', d[a:a+4])[0]
            if w==0xFFFFFFFF: break
            if w==0x03E8:
                dest=struct.unpack('>I', d[a+8:a+12])[0]>>16
                break
            a+=4
        if dest is not None:
            struct.pack_into('>IiIIIII', d, cs,
                             1, 2,                    # totalEntries, endFrame
                             0x03E8, 1,               # CS_DESTINATION, 1 entry
                             (dest<<16)|1,            # destination, startFrame=1
                             (2<<16),                 # endFrame=2
                             0xFFFFFFFF)              # CS_END
        else:
            struct.pack_into('>IiI', d, cs, 0, 1, 0xFFFFFFFF)
        n+=1
    return bytes(d), n

from ml_map import TARGETS, NAMES, SCENES
from ml_entrance import remap_exits, fix_cutscene_destinations
_POP={v['slot'] for v in SCENES.values()}   # all 25 ML scenes at their real vanilla scene IDs
new=bytearray(rom); ap=len(new)
if ap%16: ap+=16-(ap%16)
new += b'\x00'*(ap-len(new))
newvrom={}
for s,rooms in TARGETS.items():
    nm=NAMES[s]
    svs,sve,srs,sre=E[s]
    scene_bytes = get_file(bytes(rom), svs,sve,srs,sre)
    for idx in [s]+rooms:
        isroom = idx in rooms
        vs,ve,rs,re=E[idx]
        d=get_file(bytes(rom),vs,ve,rs,re)
        note=[]
        if isroom:
            d,n=mesh0_to_mesh2(d)
            if n: note.append(f"mesh0->2({n})")
        d,k=fix_altheaders(d, 0x03 if isroom else 0x02)
        if k: note.append(f"altheaders={k}")
        # ML's room object IDs are VANILLA ids (verified: 5=OBJECT_CROW, 124=OBJECT_BV,
        # 241=OBJECT_MASTERKOKIRIHEAD, 273=OBJECT_NWC; 80 distinct, max 399, SoH ships 404).
        # ML only ADDS 18 custom objects on top. The old blanket remap to gameplay_keep was
        # over-defensive -- the original object-bank crash was the command-SHIFTING corruption,
        # fixed separately. Set ML_REMAP_OBJECTS=1 to restore the old behaviour.
        if os.environ.get('ML_REMAP_OBJECTS'):
            d,o=fix_objects(d)
            if o: note.append(f"objects={o}")
        # Likewise ML's actor IDs are VANILLA ids (273=ACTOR_EN_BLKOBJ, 94=ACTOR_EN_BB,
        # 85=ACTOR_EN_DEKUNUTS, 337=ACTOR_EN_ZL3; 94 distinct, max 337, SoH ships 474).
        # Set ML_STRIP_ACTORS=1 to neutralise them again.
        if os.environ.get('ML_STRIP_ACTORS'):
            d,ac=strip_actors(d)
            if ac: note.append(f"actors_removed={ac}")
        # OFF BY DEFAULT. ML's cutscene scripts are NOT malformed -- parsed against vanilla's own
        # rules they walk cleanly (20 standard opcodes, ending in DESTINATION). Replacing them with
        # a minimal stub destroys the opening prologue, which is the whole intro. Opt in only to
        # test, via ML_STUB_CS=1.
        if not isroom and os.environ.get('ML_STUB_CS'):
            d,cst=stub_cutscenes(d)
            if cst: note.append(f"cutscenes_stubbed={cst}")
        if os.environ.get('ML_STRIP_CS'):
            # OFF by default: see strip_cutscenes() -- removing ML's cutscene data trades the
            # attract-demo HANG for a null-deref CRASH in Cutscene_ProcessCommands. Neither is
            # wanted; boot via BOOTSEQUENCE_FILESELECT (2) so the demo never runs instead.
            d,cs=strip_cutscenes(d)
            if cs: note.append(f"cutscenes_removed={cs}")
        if not isroom:
            d,ex,_k=remap_exits(d, _POP)
            if ex: note.append(f"exits_remapped={ex}")
            d,cd=fix_cutscene_destinations(d, _POP, nm)
            if cd: note.append(f"cs_dest_fixed={cd}")
        if isroom:
            # DISABLED BY DEFAULT. Decoding takes fmt/siz from the render SETTILE
            # (interpreter.cpp ImportTexture: texture_tile[tile].fmt/.siz), NOT from SETTIMG.
            # SETTIMG's siz instead drives the LOADBLOCK byte math
            # (word_size_shift: 4b -> size>>1, 16b -> size<<1), so forcing it to match the render
            # tile shrinks size_bytes 4x for block-loaded CI4 and collapses height to 0.
            # That is what made the walls black. Re-enable only with evidence.
            if os.environ.get('ML_FIX_SETTIMG_FMT'):
                d,tf=fix_settimg_fmt(d)
                if tf: note.append(f"settimg_fmt={tf}")
            d,ti=inline_scene_textures(d, scene_bytes, f'{nm}_room_{rooms.index(idx)}')
            if ti: note.append(f"inlined_tex={ti}")
        nve=vs+len(d); newvrom[idx]=(vs,nve)
        new += d
        struct.pack_into('>IIII', new, DMA+idx*16, vs, nve, ap, 0)
        print(f"  dma {idx}: {'room' if isroom else 'scene'} len {len(d)} {' '.join(note)}")
        ap += len(d)
        if ap%16:
            pad=16-(ap%16); new += b'\x00'*pad; ap+=pad

for s,rooms in TARGETS.items():
    svs,sve,srs,sre=struct.unpack('>IIII', new[DMA+s*16:DMA+s*16+16])
    sd=bytearray(new[srs:srs+(sve-svs)])
    for off,c,raw in cmds_of(sd):
        if c==0x04:
            base=struct.unpack('>I',raw[4:8])[0]&0xFFFFFF
            for k,r in enumerate(rooms):
                a,b=newvrom[r]; struct.pack_into('>II', sd, base+k*8, a, b)
    new[srs:srs+len(sd)]=sd

new[0x10:0x14]=(0x871E1C92).to_bytes(4,'big')
open(DST,'wb').write(bytes(new))
import json as _j; _j.dump(INLINED, open(PIPE + "/inlined.json","w"), indent=1)
_j.dump(UNRESOLVED, open(PIPE + "/unresolved.json","w"), indent=1)
_j.dump(ROOMLOCAL, open(PIPE + "/roomlocal.json","w"), indent=1)
print("wrote", DST, "+ inlined.json")
