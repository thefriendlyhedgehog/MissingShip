"""Shared helpers: load a ROM's dmadata, decompress files, find gActorOverlayTable."""
import struct, sys
sys.path.insert(0,'/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline')
from yaz0 import get_file

class Rom:
    def __init__(self, path, dma=0x12f70, code_idx=None):
        self.rom=open(path,'rb').read()
        self.E=[]; p=dma
        while True:
            a,b,c,d=struct.unpack('>IIII',self.rom[p:p+16])
            if a==b==c==d==0: break
            self.E.append((a,b,c,d)); p+=16
        self.byvrom={e[0]:i for i,e in enumerate(self.E)}
        self._cache={}
        self.code_idx=code_idx
    def file(self,i):
        if i not in self._cache: self._cache[i]=get_file(self.rom,*self.E[i])
        return self._cache[i]
    def file_at_vrom(self,vs):
        i=self.byvrom.get(vs)
        return None if i is None else self.file(i)

def find_actor_table(code, rom):
    """Return (base, stride). Anchored by: overlay's own ActorInit.id == table index."""
    import collections
    def W(o): return struct.unpack('>I', code[o:o+4])[0]
    votes=collections.Counter()
    for o in range(0, len(code)-32, 4):
        vs,ve,vas,vae,ld,ini = (W(o+k*4) for k in range(6))
        if not (0 < vs < ve < 0x4000000): continue
        if not (0x80000000 < vas < vae < 0x81000000): continue
        if ld!=0 or not (vas <= ini < vae): continue
        d=rom.file_at_vrom(vs)
        if d is None: continue
        off=ini-vas
        if off+0x20>len(d): continue
        aid,cat=struct.unpack('>hB', d[off:off+3])
        if not (0<aid<0x400) or cat>11: continue
        flags,objId,_p,size=struct.unpack('>IhhI', d[off+4:off+0x10])
        if not (0x14c<=size<=0x8000): continue
        fns=struct.unpack('>IIII', d[off+0x10:off+0x20])
        if not all(f==0 or 0x80000000<=f<0x81000000 for f in fns): continue
        for st in (0x1C,0x20):
            votes[(o-aid*st, st)]+=1
    (base,st),n = votes.most_common(1)[0]
    return base, st, n

def entry(code, base, stride, aid):
    o=base+aid*stride
    return struct.unpack('>IIIIII', code[o:o+24])
