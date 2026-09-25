"""Disassemble one ML actor overlay with its ActorInit, sections and data (for porting). Usage: dumpactor.py 0xNNN"""
import struct, sys
from capstone import *
from mlrom import Rom, entry
ml = Rom('/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/tml_2_0.z64')
code = ml.file(114)
aid = int(sys.argv[1], 0)
vs, ve, vas, vae, ld, ini = entry(code, 0xF9440, 0x20, aid)
d = ml.file_at_vrom(vs)
tail = struct.unpack('>I', d[-4:])[0]; o = len(d) - tail
t, da, ro, bs, n = struct.unpack('>IIIII', d[o:o+20])
io = ini - vas
f = struct.unpack('>hBxIhhI4I', d[io:io+0x20])
print(f'; actor {aid:#x} dma {ml.byvrom[vs]} vram {vas:#x} text {t:#x} data {da:#x} rodata {ro:#x} bss {bs:#x}')
print(f'; ActorInit cat {f[1]} flags {f[2]:#x} object {f[3]:#x} size {f[5]:#x} init +{f[6]-vas:#x} destroy +{f[7]-vas:#x} update +{f[8]-vas:#x} draw +{f[9]-vas:#x}')
md = Cs(CS_ARCH_MIPS, CS_MODE_MIPS32 | CS_MODE_BIG_ENDIAN); md.skipdata = True
for i in md.disasm(d[:t], vas): print(f'{i.address-vas:04x}: {i.mnemonic:8} {i.op_str}')
print('; data+rodata')
for k in range(t, t + da + ro, 16): print(f'{k:04x}: ' + ' '.join(d[k+j:k+j+4].hex() for j in range(0, 16, 4)))
