"""Which sound effects play a font's sound-effect slot, or what one sound effect plays, from sequence 0 (MS-154).

    sfxtrace.py <archive> --slot FONT SFX    the sound effects whose scripts play font FONT's sound effect SFX
    sfxtrace.py <archive> --id 0x39B1        the (font, sound effect) slots one sound effect can play

<archive> is anything holding audio/sequences/000_Sound_Effects and audio/fonts/: an oot.o2r or the ML mod. Names come
from the engine's include/sfx.h.

Sequence 0 is read the way SoH's src/code/audio_seqplayer.c plays it. Each sound effect starts on its bank's first
channel with the ports the game sets (code_800F7260.c: port 0 = 1, port 4 = the index's low byte, port 5 = its high bit
for the large bank), so the banks' dispatch tables are followed as the game follows them, and scripts that write their
own notes (Twinrova's laughs: `stseq` from port 4) come out right. A value the script cannot know (random, other ports)
takes both branches, and a table indexed by it counts every entry. Only sound-effect notes (instrument 0x7E) count:
instruments and drums are not followed.
"""
import argparse, os, re, struct, sys, zipfile
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'pipeline'))
from mspaths import SOH

SEQ = 'audio/sequences/000_Sound_Effects'
CHANNELS_PER_BANK = [3, 2, 3, 3, 2, 1, 2]   # gChannelsPerBank[0]: player, item, env, enemy, system, ocarina, voice
LARGE_BANKS = {3}                           # gIsLargeSoundBank: the bank that also gets port 5
# D_80130520: argument layout of commands 0xB0-0xFF (low 2 bits = count; bit 7, 6, 5 = argument 1, 2, 3 is s16)
ARGS = [0x81, 0x00, 0x81, 0x01, 0x00, 0x00, 0x00, 0x81, 0x01, 0x01, 0x01, 0x42, 0x81, 0xC2, 0x00, 0x00,
        0x00, 0x01, 0x81, 0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01, 0x81, 0x01, 0x01, 0x81, 0x81,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x81, 0x01, 0x01, 0x01, 0x81, 0x01,
        0x01, 0x03, 0x03, 0x01, 0x00, 0x01, 0x01, 0x81, 0x03, 0x01, 0x00, 0x02, 0x00, 0x01, 0x01, 0x82,
        0x00, 0x01, 0x01, 0x01, 0x01, 0x81, 0x00, 0x00, 0x01, 0x81, 0x81, 0x81, 0x81, 0x00, 0x00, 0x00]
STEPS = 4000         # per path; sequence 0's scripts are short
DEPTH = 4            # SeqScriptState.stack: a script cannot nest calls deeper
MAX_STATES = 200000  # per sound effect; a trace that reaches it stops and says so


def s8(v):
    return None if v is None else ((v + 128) & 0xFF) - 128


def cu16(s, pc):
    """AudioSeq_ScriptReadCompressedU16"""
    v = s[pc]
    if v & 0x80:
        return ((v << 8) & 0x7F00) | s[pc + 1], pc + 2
    return v, pc + 1


def args(s, pc, cmd):
    t, out = ARGS[cmd - 0xB0], []
    for _ in range(t & 3):
        if t & 0x80:
            out.append(struct.unpack_from('>h', s, pc)[0]); pc += 2
        else:
            out.append(s[pc]); pc += 1
        t = (t << 1) & 0xFF
    return out, pc


def load(archive):
    """Sequence 0's data and its fonts (the archive's AudioSequence resource: 0x40 header, size, data, 3 bytes, fonts)."""
    d = zipfile.ZipFile(archive).read(SEQ)
    size = struct.unpack_from('<I', d, 0x40)[0]
    p = 0x44 + size + 3
    return d[0x44:0x44 + size], list(d[p + 4:p + 4 + struct.unpack_from('<I', d, p)[0]])


def channel_starts(seq):
    """Channel -> script offset, from the sequence script's `ldchan` (0x90 | channel, s16) commands."""
    starts, pc = {}, 0
    while pc < len(seq):
        c = seq[pc]; pc += 1
        if c & 0xF0 == 0x90:
            starts[c & 0xF] = struct.unpack_from('>h', seq, pc)[0] & 0xFFFF; pc += 2
        elif c in (0xD7, 0xD6, 0xC5):
            pc += 2
        elif c in (0xD3, 0xD5, 0xDB, 0xDD, 0xD9, 0xDE, 0xDC, 0xD0, 0xCC, 0xC9, 0xC8, 0xF1, 0xDF):
            pc += 1
        else:
            break
    return starts


class Trace:
    def __init__(self, seq, fonts):
        self.seq, self.fonts = seq, fonts
        self.starts = channel_starts(seq)
        self.truncated = False

    def font(self, arg, current):
        # `font n` picks from the sequence's font list, last first (seqPlayer->defaultFont path)
        return self.fonts[len(self.fonts) - arg - 1] if arg < len(self.fonts) else current

    def slots(self, bank, idx):
        """The (font, sound effect) slots sound effect `idx` of `bank` can play."""
        mem = bytearray(self.seq)          # this effect's copy: `stseq` writes into it
        ports = {0: 1, 4: s8(idx & 0xFF), 5: (idx >> 8) & 1 if bank in LARGE_BANKS else None}
        out, seen = set(), set()
        pc = self.starts[sum(CHANNELS_PER_BANK[:bank])]
        stack = [(pc, self.fonts[-1], 0xFF, False, None, (), None, False)]
        while stack:
            pc, font, inst, large, dyn, calls, val, entered = stack.pop()
            for _ in range(STEPS):
                key = (pc, font, inst, large, dyn, calls, val, entered)
                if key in seen or not 0 <= pc < len(mem) or len(calls) > DEPTH:
                    break
                if len(seen) >= MAX_STATES:
                    self.truncated = True
                    return out
                seen.add(key)
                c = mem[pc]; pc += 1
                if c >= 0xB0:
                    if c >= 0xF2:   # AudioSeq_HandleScriptFlowControl
                        t = ARGS[c - 0xB0]
                        a = 0
                        if t & 3 == 1:
                            if t & 0x80:
                                a = struct.unpack_from('>h', mem, pc)[0] & 0xFFFF; pc += 2
                            else:
                                a = mem[pc]; pc += 1
                        if c == 0xFF:
                            if not calls or calls[-1] is None:   # back in the bank's loop: the effect is over
                                break
                            pc, calls = calls[-1], calls[:-1]
                        elif c == 0xFC:
                            calls += (pc,); pc = a
                        elif c == 0xFB:
                            pc = a
                        elif c == 0xF4:
                            pc += s8(a)
                        elif c in (0xFA, 0xF9, 0xF5, 0xF3, 0xF2):
                            target = a if c in (0xFA, 0xF9, 0xF5) else pc + s8(a)
                            if val is None:
                                stack.append((target, font, inst, large, dyn, calls, val, entered))
                            elif {0xFA: val == 0, 0xF3: val == 0, 0xF9: val < 0, 0xF2: val < 0, 0xF5: val >= 0}[c]:
                                pc = target
                        continue    # FD/FE wait, F8/F7/F6 loop: carry on
                    p, pc = args(mem, pc, c)
                    if c == 0xE8:
                        pc += 5
                    elif c == 0xEA:
                        break
                    elif c == 0xC6:
                        font = self.font(p[0], font)
                    elif c == 0xC1:
                        inst = p[0]
                    elif c == 0xC4:
                        large = True
                    elif c == 0xC3:
                        large = False
                    elif c == 0xC2:
                        dyn = p[0] & 0xFFFF
                    elif c == 0xC8:
                        val = s8(val - p[0]) if val is not None else None
                    elif c == 0xCC:
                        val = s8(p[0])
                    elif c == 0xC9:
                        val = s8(val & s8(p[0])) if val is not None else None
                    elif c == 0xCB:
                        val = mem[(p[0] & 0xFFFF) + val] if val is not None else None
                    elif c == 0xC7:
                        if val is not None:
                            mem[p[1] & 0xFFFF] = (val + p[0]) & 0xFF
                    elif c == 0xB8:
                        val = None
                    elif c == 0xE4 and dyn is not None:
                        # the first call is the bank's dispatch: returning from it ends the effect (None)
                        ret = calls + ((pc,) if entered else (None,))
                        for i in (range(128) if val is None else [val]):
                            if not 0 <= dyn + 2 * i + 1 < len(mem):
                                break
                            target = (mem[dyn + 2 * i] << 8) | mem[dyn + 2 * i + 1]
                            if not 0 < target < len(mem):
                                break
                            stack.append((target, font, inst, large, dyn, ret, val, True))
                        break
                elif c >= 0x70:
                    lo, hi = c & 7, c & 0xF8
                    if hi in (0x88, 0x78):
                        v = struct.unpack_from('>h', mem, pc)[0]; pc += 2
                        self.layer(mem, (v & 0xFFFF) if hi == 0x88 else pc + v, font, inst, large, out, seen)
                    elif hi == 0x98 and dyn is not None:
                        for i in (range(128) if val is None else [val]):
                            if not 0 <= dyn + 2 * i + 1 < len(mem):
                                break
                            self.layer(mem, (mem[dyn + 2 * i] << 8) | mem[dyn + 2 * i + 1], font, inst, large, out, seen)
                    elif hi == 0x80:
                        val = None
                else:
                    hi, lo = c & 0xF0, c & 0xF
                    if hi == 0x60:
                        val = ports.get(lo)
                    elif hi == 0x20:
                        pc += 2
                    elif hi == 0x30:
                        pc += 1
                    elif hi == 0x40:
                        pc += 1; val = None
                    elif hi == 0x50:
                        val = None
        return out

    def layer(self, mem, pc, font, chinst, large, out, seen):
        """A layer's notes (AudioSeq_SeqLayerProcessScriptStep2-4): with instrument 0x7E, note n plays sound effect
        (transposition << 6) + n of the channel's font."""
        stack = [(pc, 0xFF, 0, ())]
        while stack:
            pc, inst, tr, calls = stack.pop()
            for _ in range(STEPS):
                key = ('layer', pc, inst, tr, font, chinst, large, calls)
                if key in seen or not 0 <= pc < len(mem) or len(calls) > DEPTH:
                    break
                if len(seen) >= MAX_STATES:
                    self.truncated = True
                    return
                seen.add(key)
                c = mem[pc]; pc += 1
                if c < 0xC0:
                    kind = c & 0xC0
                    if large:
                        if kind == 0x00:
                            _, pc = cu16(mem, pc); pc += 2
                        elif kind == 0x40:
                            _, pc = cu16(mem, pc); pc += 1
                        else:
                            pc += 2
                    elif kind == 0x00:
                        _, pc = cu16(mem, pc)
                    if (chinst if inst == 0xFF else inst) == 0x7E:
                        out.add((font, (tr << 6) + (c & 0x3F)))
                elif c == 0xC0:
                    _, pc = cu16(mem, pc)
                elif c >= 0xF2:
                    t = ARGS[c - 0xB0]
                    a = 0
                    if t & 3 == 1:
                        if t & 0x80:
                            a = struct.unpack_from('>h', mem, pc)[0] & 0xFFFF; pc += 2
                        else:
                            a = mem[pc]; pc += 1
                    if c == 0xFF and calls:
                        pc, calls = calls[-1], calls[:-1]
                    elif c in (0xFF, 0xFE, 0xFD):    # a layer stops here
                        break
                    elif c == 0xFC:
                        calls += (pc,); pc = a
                    elif c == 0xFB:
                        pc = a
                    elif c == 0xF4:
                        pc += s8(a)
                    elif c in (0xFA, 0xF9, 0xF5):
                        stack.append((a, inst, tr, calls))
                    elif c in (0xF3, 0xF2):
                        stack.append((pc + s8(a), inst, tr, calls))
                elif c == 0xC6:
                    inst = mem[pc]; pc += 1
                elif c == 0xC2:
                    tr = mem[pc]; pc += 1
                elif c in (0xC1, 0xCA, 0xC9, 0xCF, 0xCD, 0xCE):
                    pc += 1
                elif c == 0xC3:
                    _, pc = cu16(mem, pc)
                elif c == 0xC7:
                    mode = mem[pc]; pc += 2
                    if mode & 0x80:
                        pc += 1
                    else:
                        _, pc = cu16(mem, pc)
                elif c == 0xCB:
                    pc += 3


def sfx_names():
    """(bank, index) -> names, from include/sfx.h (an ID is bank << 12 | 0x800 | index)."""
    names = {}
    for m in re.finditer(r'#define (NA_SE_\w+)\s+0x([0-9A-Fa-f]+)', open(os.path.join(SOH, 'include', 'sfx.h')).read()):
        v = int(m.group(2), 16)
        names.setdefault((v >> 12, v & 0x1FF), []).append(m.group(1))
    return names


def sfx_counts(archive):
    """Font -> how many sound effects it has (the AudioSoundFont resource's third count)."""
    z, counts = zipfile.ZipFile(archive), {}
    for n in z.namelist():
        m = re.match(r'audio/fonts/(\d+)_', n)
        if m:
            counts[int(m.group(1))] = struct.unpack_from('<I', z.read(n), 0x40 + 4 + 8 + 8)[0]
    return counts


def sample_of(archive, font, sfx):
    """The sample behind a font's sound effect, from the archive's font resource."""
    z = zipfile.ZipFile(archive)
    name = next((n for n in z.namelist() if n.startswith(f'audio/fonts/{font:02d}_')), None)
    if name is None:
        return '?'
    d, p = z.read(name), [0x40 + 4 + 8]

    def u32():
        p[0] += 4
        return struct.unpack_from('<I', d, p[0] - 4)[0]

    def string():
        n = u32(); p[0] += n
        return d[p[0] - n:p[0]].decode()

    def entry():
        if d[p[0]]:
            p[0] += 2; ref = string(); p[0] += 4
            return ref
        p[0] += 1
    drums, insts, sfxs = u32(), u32(), u32()
    for _ in range(drums):
        p[0] += 3
        envelopes = u32()
        p[0] += 4 * envelopes + 1; string(); p[0] += 4
    for _ in range(insts):
        p[0] += 5
        envelopes = u32()
        p[0] += 4 * envelopes
        for _ in range(3):
            entry()
    refs = [entry() for _ in range(sfxs)]
    return refs[sfx] if sfx < len(refs) else '(no such slot)'


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('archive')
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--slot', nargs=2, type=lambda v: int(v, 0), metavar=('FONT', 'SFX'))
    g.add_argument('--id', type=lambda v: int(v, 0))
    a = ap.parse_args()
    trace, names, counts = Trace(*load(a.archive)), sfx_names(), sfx_counts(a.archive)

    def readable(slots):
        # a table entry past the table's end points at other data, which decodes as notes for slots no font has
        return all(sfx < counts.get(font, 0) for font, sfx in slots)
    if a.id is not None:
        bank, idx = a.id >> 12, a.id & 0x1FF
        print(f'{a.id:#06x} {" ".join(names.get((bank, idx), ["(no name)"]))}')
        slots = trace.slots(bank, idx)
        if not readable(slots):
            print('  unreadable: its table entry leads into data, not a script (slots no font has)')
            return
        for font, sfx in sorted(slots):
            print(f'  font {font} sfx {sfx}: {sample_of(a.archive, font, sfx)}')
        if trace.truncated:
            print(f'  (stopped at {MAX_STATES} states: the list may be incomplete)')
        return
    font, sfx = a.slot
    print(f'font {font} sfx {sfx}: {sample_of(a.archive, font, sfx)}')
    found, unreadable = 0, []
    for bank in range(len(CHANNELS_PER_BANK)):
        count = max((i for b, i in names if b == bank), default=-1) + 1
        for idx in range(count):
            trace.truncated = False
            slots = trace.slots(bank, idx)
            sid = bank << 12 | 0x800 | idx
            if not readable(slots) or trace.truncated:
                unreadable.append(sid)
            elif (font, sfx) in slots:
                found += 1
                print(f'  {sid:#06x} {" ".join(names.get((bank, idx), ["(no name)"]))}')
    if not found:
        print('  no sound effect plays it')
    if unreadable:
        print(f'  not traced (entry leads into data, or too many states): {" ".join(f"{v:#06x}" for v in unreadable)}')


if __name__ == '__main__':
    main()
