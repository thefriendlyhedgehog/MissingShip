import struct, sys, json
sys.path.insert(0,'/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline')
from yaz0 import get_file

rom = open('/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/tml_2_0.z64','rb').read()
p=0x12f70; E=[]
while True:
    vs,ve,rs,re = struct.unpack('>IIII', rom[p:p+16])
    if vs==0 and ve==0 and rs==0 and re==0: break
    E.append((vs,ve,rs,re)); p+=16

FMT={0:'rgba',1:'yuv',2:'ci',3:'ia',4:'i'}
SIZ={0:4,1:8,2:16,3:32}

def scan_room(room_idx, scene_idx):
    room = get_file(rom, *E[room_idx])
    scene = get_file(rom, *E[scene_idx])
    off=0; mesh=None
    while off+8<=len(room):
        c=room[off]
        if c==0x14: break
        if c==0x0A: mesh=struct.unpack('>I',room[off+4:off+8])[0]&0xFFFFFF
        off+=8
    n=room[mesh+1]; dlstart=struct.unpack('>I',room[mesh+4:mesh+8])[0]&0xFFFFFF
    found={}
    def walk(addr, depth=0, seen=None):
        if seen is None: seen=set()
        if addr==0 or (addr>>24)!=0x03: return
        a=addr & 0xFFFFFF
        if a in seen or depth>6: return
        seen.add(a)
        last_timg=None; tlut=None; rfmt=None; rsiz=None
        while a+8<=len(room):
            w0,w1=struct.unpack('>II', room[a:a+8]); op=w0>>24
            if op==0xDF: break
            elif op==0xFD:
                last_timg=w1
            elif op==0xF0:                       # G_LOADTLUT
                tlut=last_timg
            elif op==0xF5:                       # G_SETTILE
                tile=(w1>>24)&7
                if tile==0:
                    rfmt=(w0>>21)&7; rsiz=(w0>>19)&3
            elif op==0xF2:                       # G_SETTILESIZE
                tile=(w1>>24)&7
                if tile==0 and last_timg is not None and rfmt is not None:
                    uls=(w0>>12)&0xFFF; ult=w0&0xFFF
                    lrs=(w1>>12)&0xFFF; lrt=w1&0xFFF
                    wdt=((lrs-uls)>>2)+1; hgt=((lrt-ult)>>2)+1
                    if (last_timg>>24)==0x02:     # texture lives in the SCENE file
                        key=last_timg&0xFFFFFF
                        found[key]=(FMT.get(rfmt,'?')+str(SIZ.get(rsiz,0)), wdt, hgt,
                                    (tlut & 0xFFFFFF) if (tlut and (tlut>>24)==0x02) else None)
            elif op==0xDE:
                walk(w1, depth+1, seen)
                if ((w0>>16)&0xFF)==0: break
            a+=8
    for i in range(n):
        q=dlstart+i*8
        opa,xlu=struct.unpack('>II', room[q:q+8])
        for d in (opa,xlu):
            if d: walk(d)
    return found, len(scene)

import sys as _s; _s.path.insert(0,'/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline')
from ml_map import SCENES
TARGETS={k:(v['scene'][:-6], v['rooms']) for k,v in SCENES.items()}
out={}
for sdma,(name,rooms) in TARGETS.items():
    allf={}
    for r in rooms:
        f,slen = scan_room(r, sdma)
        allf.update(f)
    out[name]=allf
    print(f"{name}: {len(allf)} scene-segment textures (scene size 0x{slen:x})")
    for k in sorted(allf)[:4]:
        fmt,w,h,t = allf[k]
        print(f"   0x{k:06x} {fmt} {w}x{h} tlut={hex(t) if t else None}")
json.dump({k:{hex(a):v for a,v in d.items()} for k,d in out.items()}, open('/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/scene_textures.json','w'), indent=1)
