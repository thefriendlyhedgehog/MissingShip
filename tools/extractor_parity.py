"""What two extractors write differently for the same ROM, as the game reads it.

    extractor_parity.py <ours.o2r> <theirs.o2r>

The mod builder extracts ML's ROM with the engine fork's ZAPD (SoH 9.2.3's extractor, patched); SoH 9.3.0 and Unbound
0.11 extract with Torch. To keep the builder's output what the current SoH would write, extract the same ROM with both
(the PAL MQ Debug ROM ML is built on: `soh-macos roms/oot-mq-debug-pal.z64` in a scratch SHIP_HOME writes oot-mq.o2r)
and compare them here. Sound fonts are compared by what SoH's reader keeps (AudioSoundFontFactory.cpp: invalid
instruments and drums with no sample are dropped), so bytes an extractor leaves uninitialised there don't count. Every
other difference is listed by kind, with its first differing byte and the two values.

2026-10-11, 9.2.3's ZAPD against 9.3.0's Torch: the same 35,612 files, and only boss title cards differ (texture type
I8 against IA8, header byte 64), with portVersion. ML's own boss title card follows Torch (object_ml_bv.xml).
"""
import struct, sys, zipfile
from collections import defaultdict

HEADER = 64          # LUS resource header


class _Reader:
    def __init__(self, data):
        self.data, self.pos = data, HEADER

    def read(self, fmt):
        value = struct.unpack_from('<' + fmt, self.data, self.pos)[0]
        self.pos += struct.calcsize(fmt)
        return value

    def string(self):
        n = self.read('I')
        self.pos += n
        return self.data[self.pos - n:self.pos]


def _sound(r):
    return r.read('b'), r.string(), r.read('f')


def font(data):
    """A SoundFont V2 resource as SoH keeps it."""
    r = _Reader(data)
    head = tuple(r.read(f) for f in ('i', 'b', 'b', 'H', 'H', 'H'))
    drums, instruments, effects = r.read('I'), r.read('I'), r.read('I')
    kept = [head]
    for _ in range(drums):
        release, pan, _loaded = r.read('B'), r.read('B'), r.read('B')
        envelope = [(r.read('h'), r.read('h')) for _ in range(r.read('I'))]
        sound = _sound(r)
        kept.append((release, pan, envelope, sound) if sound[1] else None)
    for _ in range(instruments):
        valid, _loaded, low, high, release = (r.read('B') for _ in range(5))
        envelope = [(r.read('h'), r.read('h')) for _ in range(r.read('i'))]
        sounds = [_sound(r) if r.read('b') else None for _ in range(3)]
        kept.append((low, high, release, envelope, sounds) if valid else None)
    for _ in range(effects):
        kept.append(_sound(r) if r.read('b') else None)
    if r.pos != len(data):
        raise ValueError(f'{len(data) - r.pos} bytes after the last entry')
    return kept


def compare(ours, theirs):
    with zipfile.ZipFile(ours) as a, zipfile.ZipFile(theirs) as b:
        names_a, names_b = set(a.namelist()), set(b.namelist())
        print(f'{len(names_a)} files in ours, {len(names_b)} in theirs')
        for label, names in (('only in ours', names_a - names_b), ('only in theirs', names_b - names_a)):
            if names:
                print(f'{label}: {len(names)}, e.g. {", ".join(sorted(names)[:5])}')
        kinds, inert = defaultdict(list), 0
        for name in sorted(names_a & names_b):
            x, y = a.read(name), b.read(name)
            if x == y:
                continue
            if name.startswith('audio/fonts/') and font(x) == font(y):
                inert += 1
                continue
            at = next((i for i in range(min(len(x), len(y))) if x[i] != y[i]), min(len(x), len(y)))
            count = sum(1 for i in range(min(len(x), len(y))) if x[i] != y[i]) + abs(len(x) - len(y))
            kinds[(count, at, x[at] if at < len(x) else None, y[at] if at < len(y) else None)].append(name)
        print(f'{inert} sound fonts differ only in entries SoH drops')
        for (count, at, mine, other), names in sorted(kinds.items(), key=lambda k: -len(k[1])):
            print(f'{len(names)} differ in {count} byte(s) from {at} ({mine} against {other}), e.g. '
                  f'{", ".join(names[:3])}')
        return not kinds and names_a == names_b


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    sys.exit(0 if compare(sys.argv[1], sys.argv[2]) else 1)
