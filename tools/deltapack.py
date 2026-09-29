"""A mod archive as a delta pack: its files as recipes against the player's own base archives.

    deltapack.py make  <mod.o2r> <pack.o2r> <base.o2r> [<base.o2r> ...]    write the pack, and say what it ships
    deltapack.py apply <pack.o2r> <out.o2r> <base.o2r> [<base.o2r> ...]    rebuild the pack's files as the game would
    deltapack.py scan  <pack.o2r> <base.o2r> [<base.o2r> ...]              base bytes the pack still carries, per folder

A romhack is distributed as a patch: it copies what it can from the player's own ROM and carries only the hack's own
bytes. A pack does the same for an .o2r, file by file, against the archives the player's game already mounts (oot.o2r
and Unbound's converted base oot-unbound.o2r), so it can be shared without Nintendo's data. Bases are listed lowest
priority first, as the game mounts them.

A file of the mod that equals what the base has at its path is left out: the player's base supplies it (with its
own copy of any bytes the extractor leaves uninitialised, which the game never reads). Any other file of the pack is
either the mod's file as it is, or a delta (libultraship rebuilds it when the file is loaded):

    'LUSD' u8 version (1)
    u32 size, u64 crc                       of the rebuilt file
    u8 source count, then per source:       u16 path length, path (utf-8), u32 size, u64 crc
    ops to the end of the file:             0x00 u8 source, u32 offset, u32 length    copy from a source
                                            0x01 u32 length, bytes                    insert

delta-sources.json lists every source the deltas use ({"sources": [{"path", "size", "crc": 16 hex digits}]}); the
game mounts the pack only when all of them are there, so a player with another base gets the game without the mod.

Integers are little-endian; crc is libultraship's CRC-64 (update_crc64 from all ones). A source is the first file at
its path, searching the mounted archives from the top, whose size and crc match; so a pack made against one base still
works where that file sits in another archive (the browser build mounts only oot-unbound.o2r, which carries every
untouched vanilla file), and a pack whose base differs is not mounted rather than loading wrong data.
"""
import collections, hashlib, json, os, struct, sys, zipfile

MAGIC = b'LUSD'
VERSION = 1
BLOCK = 16          # stride of a source's index: the shortest copy found (an op costs 10 bytes)
SCAN = 64           # block that finds candidate sources anywhere in the base
MAX_SOURCES = 8     # sources per delta: the file at the same path, then those sharing the most blocks
SOURCES = 'delta-sources.json'       # every source of the pack's deltas: the game mounts the pack only if all are there
KEEP = {'version', 'portVersion', SOURCES}    # read by the archive itself as it mounts, never rebuilt
# Never sources: resource types whose bytes differ between two players' oot.o2r. Upstream ZAPD leaves bytes it
# writes uninitialised in skeleton limbs, rooms and sound fonts (UNBOUND_MAP.md B8), and an oot.o2r extracted by stock
# SoH, which Unbound reuses, has them; across ten extractions (two extractors, macOS and Linux, before and after the
# fixes) every entry that differed was one of these types.
UNSTABLE_TYPES = {b'OSLB', b'OROM', b'OSFT'}      # SkeletonLimb, Room, SoundFont, as the resource header spells them

_POLY, _MASK = 0x42F0E1EBA9EA3693, (1 << 64) - 1
_TABLE = []
for _i in range(256):
    _c = _i << 56
    for _ in range(8):
        _c = ((_c << 1) ^ _POLY) & _MASK if _c >> 63 else (_c << 1) & _MASK
    _TABLE.append(_c)


def crc64(data):
    """libultraship's update_crc64(data, len, INITIAL_CRC64)."""
    crc, t = _MASK, _TABLE
    for b in data:
        crc = t[(crc >> 56) ^ b] ^ ((crc << 8) & _MASK)
    return crc ^ _MASK


def _path(p):
    b = p.encode()
    return struct.pack('<H', len(b)) + b


def _interesting(block):
    return len(set(block)) > 4        # padding and flat fills match everywhere and prove nothing


class Base:
    """The player's mounted archives; a source is (archive index, path)."""

    def __init__(self, archives):
        self.zips = [zipfile.ZipFile(a) for a in archives]
        self.names = [set(z.namelist()) for z in self.zips]
        self._crc = {}
        self._type = {}
        self.sources = set(range(len(self.zips)))

    def read(self, key):
        return self.zips[key[0]].read(key[1])

    def top(self, path):
        """What the game loads at this path without the mod: the top archive's file, or None."""
        for a in reversed(range(len(self.zips))):
            if path in self.names[a]:
                return self.zips[a].read(path)
        return None

    def at(self, path):
        """Every usable base file at this path, the top archive first."""
        return [(a, path) for a in reversed(range(len(self.zips))) if path in self.names[a] and self.usable((a, path))]

    def keys(self):
        for a in range(len(self.zips)):
            for path in self.zips[a].namelist():
                if self.usable((a, path)):
                    yield a, path

    def usable(self, key):
        if key[1].endswith('/') or key[0] not in self.sources:
            return False
        if key not in self._type:
            with self.zips[key[0]].open(key[1]) as f:
                self._type[key] = f.read(8)[4:8][::-1]
        return self._type[key] not in UNSTABLE_TYPES

    def crc(self, key):
        if key not in self._crc:
            self._crc[key] = crc64(self.read(key))
        return self._crc[key]

    def find(self, path, size, crc):
        """What the game does: the first file at this path, from the top, with this size and crc."""
        for key in self.at(path):
            data = self.read(key)
            if len(data) == size and not data.startswith(MAGIC) and self.crc(key) == crc:
                return data
        return None


def encode(sources, dst):
    """Greedy copy/insert ops building dst from the sources (earlier sources win a tie)."""
    index = {}
    for s, src in enumerate(sources):
        for o in range(0, len(src) - BLOCK + 1, BLOCK):
            index.setdefault(src[o:o + BLOCK], (s, o))
    ops, lit, i = [], bytearray(), 0
    while i < len(dst):
        hit = index.get(dst[i:i + BLOCK]) if i + BLOCK <= len(dst) else None
        if hit is None:
            lit.append(dst[i]); i += 1
            continue
        s, o = hit
        src = sources[s]
        back = 0                                   # grow the copy backwards into the pending insert
        while back < len(lit) and o - back > 0 and src[o - back - 1] == dst[i - back - 1]:
            back += 1
        if back:
            del lit[-back:]
        o, i, n = o - back, i - back, BLOCK + back
        while i + n < len(dst) and o + n < len(src) and src[o + n] == dst[i + n]:
            n += 1
        if lit:
            ops.append((1, bytes(lit))); lit = bytearray()
        ops.append((0, s, o, n))
        i += n
    if lit:
        ops.append((1, bytes(lit)))
    return ops


def delta(dst, sources, base):
    """The delta record for dst from sources [(key, data)]."""
    out = bytearray(MAGIC + bytes([VERSION]) + struct.pack('<IQB', len(dst), crc64(dst), len(sources)))
    for key, data in sources:
        out += _path(key[1]) + struct.pack('<IQ', len(data), base.crc(key))
    for op in encode([d for _, d in sources], dst):
        out += struct.pack('<BBII', 0, *op[1:]) if op[0] == 0 else struct.pack('<BI', 1, len(op[1])) + op[1]
    return bytes(out)


def rebuild(data, base):
    """A pack file's content: itself, or its delta applied to the base."""
    if not data.startswith(MAGIC):
        return data
    if data[4] != VERSION:
        raise ValueError(f'delta version {data[4]}')
    size, want, count = struct.unpack_from('<IQB', data, 5)
    o, sources = 18, []
    for _ in range(count):
        n = struct.unpack_from('<H', data, o)[0]
        path = data[o + 2:o + 2 + n].decode()
        ssize, scrc = struct.unpack_from('<IQ', data, o + 2 + n)
        o += 14 + n
        src = base.find(path, ssize, scrc)
        if src is None:
            raise ValueError(f'no base file {path} of {ssize} bytes with crc {scrc:016x}')
        sources.append(src)
    out = bytearray()
    while o < len(data):
        if data[o] == 0:
            s, off, n = struct.unpack_from('<BII', data, o + 1); o += 10
            out += sources[s][off:off + n]
        else:
            n = struct.unpack_from('<I', data, o + 1)[0]; o += 5
            out += data[o:o + n]; o += n
    if len(out) != size or crc64(out) != want:
        raise ValueError('rebuilt file differs from the one the pack was made from')
    return bytes(out)


def block_index(base):
    """hash of every interesting SCAN-byte block of the base (stride SCAN) -> the first source holding it."""
    keys, index = [], {}
    for key in base.keys():
        data = base.read(key)
        k = len(keys); keys.append(key)
        for o in range(0, len(data) - SCAN + 1, SCAN):
            b = data[o:o + SCAN]
            if _interesting(b):
                index.setdefault(hash(b), k)
    return keys, index


def candidates(dst, keys, index):
    """Base sources sharing blocks with dst, most shared first."""
    count, i = collections.Counter(), 0
    while i + SCAN <= len(dst):
        k = index.get(hash(dst[i:i + SCAN]))
        if k is None:
            i += 1
        else:
            count[k] += 1; i += SCAN
    return [keys[k] for k, _ in count.most_common(MAX_SOURCES)]


def make(mod, pack, bases, sources=None):
    base = Base(bases)
    if sources is not None:
        base.sources = sources
    whole = {}
    for key in base.keys():
        whole.setdefault(hashlib.sha1(base.read(key)).digest(), key)     # a lower archive (oot.o2r) wins a tie
    keys, index = block_index(base)
    stats = collections.defaultdict(lambda: [0, 0])
    own, used = 0, set()
    with zipfile.ZipFile(mod) as zin, zipfile.ZipFile(pack, 'w', zipfile.ZIP_DEFLATED) as zout:
        for info in zin.infolist():
            if info.filename == SOURCES:
                sys.exit(f'{mod} already has a {SOURCES}')
            dst = zin.read(info)
            if dst.startswith(MAGIC):
                sys.exit(f'{info.filename} starts with {MAGIC!r} and would read as a delta')
            if info.filename not in KEEP and base.top(info.filename) == dst:
                stats['same'][0] += 1; stats['same'][1] += len(dst)     # the player's base has it already
                continue
            rec, kind = dst, 'own'
            same = whole.get(hashlib.sha1(dst).digest())
            if info.filename in KEEP or len(dst) < 64:
                pass
            elif same is not None:
                rec, kind = delta(dst, [(same, dst)], base), 'copy'
            else:
                picked = base.at(info.filename)[:1]
                picked += [k for k in candidates(dst, keys, index) if k not in picked]
                picked = picked[:MAX_SOURCES]
                if picked:
                    rec, kind = delta(dst, [(k, base.read(k)) for k in picked], base), 'delta'
            if len(rec) >= len(dst):
                rec, kind = dst, 'own'
            if kind != 'own':
                if rebuild(rec, base) != dst:
                    sys.exit(f'{info.filename}: the delta does not rebuild')
                used.update(sources_of(rec))
            own += len(inserts(rec))
            stats[kind][0] += 1; stats[kind][1] += len(dst)
            zout.writestr(info, rec)
        listed = [{'path': p, 'size': n, 'crc': f'{c:016x}'} for p, n, c in sorted(used)]
        zout.writestr(SOURCES, json.dumps({'sources': listed}, indent=1) + '\n')
    for kind, (n, size) in stats.items():
        print(f'  {kind:5}: {n:5} files, {size / 1e6:6.2f} MB of the mod')
    print(f'  carries {own / 1e6:.2f} MB of the mod\'s own bytes; {os.path.getsize(pack) / 1e6:.2f} MB zipped')


def sources_of(rec):
    """(path, size, crc) of each source a delta names."""
    o, out = 18, []
    for _ in range(rec[17]):
        n = struct.unpack_from('<H', rec, o)[0]
        size, crc = struct.unpack_from('<IQ', rec, o + 2 + n)
        out.append((rec[o + 2:o + 2 + n].decode(), size, crc))
        o += 14 + n
    return out


def inserts(rec):
    """The bytes of a pack file that are its own (not copied from the base)."""
    if not rec.startswith(MAGIC):
        return rec
    o = 18
    for _ in range(rec[17]):
        o += 14 + struct.unpack_from('<H', rec, o)[0]
    out = bytearray()
    while o < len(rec):
        if rec[o] == 0:
            o += 10
        else:
            k = struct.unpack_from('<I', rec, o + 1)[0]; out += rec[o + 5:o + 5 + k]; o += 5 + k
    return bytes(out)


def apply(pack, out, bases):
    base = Base(bases)
    with zipfile.ZipFile(pack) as zin, zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as zout:
        for info in zin.infolist():
            if info.filename != SOURCES:
                zout.writestr(info, rebuild(zin.read(info), base))


def scan(pack, bases):
    """Interesting base blocks still inside the pack's own bytes (whole files and inserts): what a pack might carry of the base, per folder."""
    base = Base(bases)
    keys, index = block_index(base)
    per = collections.defaultdict(lambda: [0, 0, 0])
    with zipfile.ZipFile(pack) as z:
        for info in z.infolist():
            data = inserts(z.read(info))
            hit = i = 0
            while i + SCAN <= len(data):
                if hash(data[i:i + SCAN]) in index:
                    hit += SCAN; i += SCAN
                else:
                    i += 1
            parts = info.filename.split('/')
            p = per['/'.join(parts[:2]) if len(parts) > 2 else parts[0]]
            p[0] += 1; p[1] += len(data); p[2] += hit
    for folder, (n, size, hit) in sorted(per.items(), key=lambda kv: -kv[1][2]):
        if hit:
            print(f'  {folder:40} {n:5} files {size / 1e6:6.2f} MB, base blocks {hit / 1e3:8.1f} KB')


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else ''
    if cmd in ('make', 'apply') and len(sys.argv) >= 5:
        (make if cmd == 'make' else apply)(sys.argv[2], sys.argv[3], sys.argv[4:])
    elif cmd == 'scan' and len(sys.argv) >= 4:
        scan(sys.argv[2], sys.argv[3:])
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
