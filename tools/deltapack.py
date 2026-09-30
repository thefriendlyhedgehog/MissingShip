"""A mod archive as a delta pack: its files as recipes against the player's own base archives.

    deltapack.py make  <mod.o2r> <pack.o2r> <base.o2r> [<base.o2r> ...] [--also <base.o2r> ...]...
                                                                            write the pack, and say what it ships
    deltapack.py apply <pack.o2r> <out.o2r> <base.o2r> [<base.o2r> ...]    rebuild the pack's files as the game would
    deltapack.py scan  <pack.o2r> <base.o2r> [<base.o2r> ...]              base bytes the pack still carries, per folder

A romhack is distributed as a patch: it copies what it can from the player's own ROM and carries only the hack's own
bytes. A pack does the same for an .o2r, file by file, against the archives the player's game already mounts (oot.o2r
and Unbound's converted base oot-unbound.o2r), so it can be shared without Nintendo's data. Bases are listed lowest
priority first, as the game mounts them.

Each --also names the same base as another extractor wrote it (SoH 9.1 and 9.2 name samples differently and lay out
some display lists and rooms differently; Unbound takes any 9.x oot.o2r). A source must then have the same bytes at
its path in every base given, and a file is left out only when every base has it, so one pack mounts over all of them.

A file of the mod that equals what the base has at its path is left out: the player's base supplies it (with its
own copy of any bytes the extractor leaves uninitialised, which the game never reads). Any other file of the pack is
either the mod's file as it is, or a delta (libultraship rebuilds it when the file is loaded):

    'LUSD' u8 version (1), u8 variant count, then per variant u32 length and:
    u32 size, u64 crc                       of the rebuilt file
    u8 source count, then per source:       u8 path count, per path u16 length and the path (utf-8);
                                            u64 crc of the bytes copied from it, piece after piece in op order
    ops to the end of the variant:          0x00 u8 source, u32 offset, u32 length    copy from a source
                                            0x01 u32 length, bytes                    insert

The game uses the first variant whose sources are all there. A delta that fits every base given is one variant; where
the bases lay a file out too differently for one (SoH 9.1's and 9.2's converted message tables), the pack carries one
variant per base instead, if that keeps at least VARIANT_GAIN more of the base out of the pack.

deltas.json lists the pack's deltas ({"files": [...]}); the game mounts the pack only when every source of every one is
there, so a player with another base gets the game without the mod.

Integers are little-endian; crc is libultraship's CRC-64 (update_crc64 from all ones). A source is the first file at
one of its paths, searching the mounted archives from the top, whose copied bytes match; so a pack made against one
base still works where the file sits in another archive (the browser build mounts only oot-unbound.o2r, which carries
every untouched vanilla file) or under another name (a sample another extractor version names differently), bytes
another version writes differently are never copied, and a pack whose base differs is not mounted.
"""
import collections, hashlib, json, os, struct, sys, zipfile

MAGIC = b'LUSD'
VERSION = 1
BLOCK = 16          # stride of a source's index: the shortest copy found (an op costs 10 bytes)
PLACES = 16         # places kept per block of a source's index, for text that repeats itself
SCAN = 64           # block that finds candidate sources anywhere in the base
MAX_SOURCES = 8     # sources per delta: the file at the same path, then those sharing the most blocks
LIST = 'deltas.json'        # the pack's deltas: the game mounts the pack only if all their sources are there
KEEP = {'version', 'portVersion', LIST}    # read by the archive itself as it mounts, never rebuilt
VARIANT_GAIN = 256          # bytes of the base one variant per base must keep out of the pack to be worth it
MAX_CHANGED = 0.01          # a file another extractor version writes with up to 1% of its bytes changed is still a source
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

    def __init__(self, archives, also=()):
        self.zips = [zipfile.ZipFile(a) for a in archives]
        self.names = [set(z.namelist()) for z in self.zips]
        self._crc = {}
        self._type = {}
        self._sha = {}
        self._info = {}
        self._by_sha = None
        self.sources = set(range(len(self.zips)))
        self.also = [Base(a) for a in also]     # the same base as other extractor versions write it

    def read(self, key):
        return self.zips[key[0]].read(key[1])

    def sha(self, key):
        if key not in self._sha:
            self._sha[key] = hashlib.sha1(self.read(key)).digest()
        return self._sha[key]

    def path_of(self, path, sha):
        """A path this base holds these bytes at: `path` itself when it does, else any, else None."""
        if any(path in self.names[a] and self.sha((a, path)) == sha for a in range(len(self.zips))):
            return path
        if self._by_sha is None:
            self._by_sha = {}
            for a in range(len(self.zips)):
                for p in self.zips[a].namelist():
                    if not p.endswith('/'):
                        self._by_sha.setdefault(self.sha((a, p)), p)
        return self._by_sha.get(sha)

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
        return self.source_info(key) is not None

    def source_info(self, key):
        """(paths, stable) for a base file every base has, or None. paths are where the game may find it: its own,
        then where another extractor version put the same bytes. stable marks with 0 the bytes another version writes
        differently at the same path (None when there are none): a delta never copies those."""
        if key in self._info:
            return self._info[key]
        info = None
        if not key[1].endswith('/') and key[0] in self.sources:
            if key not in self._type:
                with self.zips[key[0]].open(key[1]) as f:
                    self._type[key] = f.read(8)[4:8][::-1]
            if self._type[key] not in UNSTABLE_TYPES:
                info = self._across_versions(key)
        self._info[key] = info
        return info

    def _across_versions(self, key):
        paths, stable, sha = [key[1]], None, self.sha(key)
        for alt in self.also:
            where = alt.path_of(key[1], sha)
            if where is not None:
                if where not in paths:
                    paths.append(where)
                continue
            data, other = self.read(key), alt.top(key[1])
            if other is None or len(other) != len(data):
                return None
            changed = [i for i, (x, y) in enumerate(zip(data, other)) if x != y]
            if len(changed) > MAX_CHANGED * len(data):
                return None
            stable = stable or bytearray(b'\x01' * len(data))
            for i in changed:
                stable[i] = 0
        return paths, stable

    def find(self, paths, copies, crc):
        """What the game does: the first file at one of these paths, from the top, whose copied bytes match."""
        for path in paths:
            for a in reversed(range(len(self.zips))):
                if path not in self.names[a]:
                    continue
                data = self.zips[a].read(path)
                if not data.startswith(MAGIC) and all(o + n <= len(data) for o, n in copies) and \
                        crc64(b''.join(data[o:o + n] for o, n in copies)) == crc:
                    return data
        return None


def encode(sources, dst, stables):
    """Greedy copy/insert ops building dst from the sources (earlier sources win a tie), never copying a byte its
    source's stable mask marks 0."""
    def ok(s, o):
        return stables[s] is None or stables[s][o]
    def reach(s, o, i):                            # how far a copy from source s at o matches dst at i
        src, n = sources[s], BLOCK
        while i + n < len(dst) and o + n < len(src) and src[o + n] == dst[i + n] and ok(s, o + n):
            n += 1
        return n
    index = {}
    for s, src in enumerate(sources):
        for o in range(0, len(src) - BLOCK + 1, BLOCK):
            if stables[s] is None or all(stables[s][o:o + BLOCK]):
                places = index.setdefault(src[o:o + BLOCK], [])
                if len(places) < PLACES:
                    places.append((s, o))
    ops, lit, i = [], bytearray(), 0
    while i < len(dst):
        hits = index.get(dst[i:i + BLOCK]) if i + BLOCK <= len(dst) else None
        if not hits:
            lit.append(dst[i]); i += 1
            continue
        s, o = max(hits, key=lambda h: reach(h[0], h[1], i))    # repeated text: the place that matches longest
        src = sources[s]
        back = 0                                   # grow the copy backwards into the pending insert
        while back < len(lit) and o - back > 0 and src[o - back - 1] == dst[i - back - 1] and ok(s, o - back - 1):
            back += 1
        if back:
            del lit[-back:]
        o, i, n = o - back, i - back, BLOCK + back
        while i + n < len(dst) and o + n < len(src) and src[o + n] == dst[i + n] and ok(s, o + n):
            n += 1
        if lit:
            ops.append((1, bytes(lit))); lit = bytearray()
        ops.append((0, s, o, n))
        i += n
    if lit:
        ops.append((1, bytes(lit)))
    return ops


def delta(dst, sources, base):
    """A variant building dst from sources [(key, data)]; sources it copies nothing from are left out."""
    infos = [base.source_info(key) for key, _ in sources]
    ops = encode([d for _, d in sources], dst, [info[1] for info in infos])
    used = sorted({op[1] for op in ops if op[0] == 0})
    renumber = {s: i for i, s in enumerate(used)}
    out = bytearray(struct.pack('<IQB', len(dst), crc64(dst), len(used)))
    for s in used:
        paths, data = infos[s][0], sources[s][1]
        copied = b''.join(data[op[2]:op[2] + op[3]] for op in ops if op[0] == 0 and op[1] == s)
        out += bytes([len(paths)]) + b''.join(_path(p) for p in paths) + struct.pack('<Q', crc64(copied))
    for op in ops:
        if op[0] == 0:
            out += struct.pack('<BBII', 0, renumber[op[1]], op[2], op[3])
        else:
            out += struct.pack('<BI', 1, len(op[1])) + op[1]
    return bytes(out)


def record(variants):
    """A delta file holding these variants, the first the game can use winning."""
    return MAGIC + bytes([VERSION, len(variants)]) + b''.join(struct.pack('<I', len(v)) + v for v in variants)


def parse(rec):
    """[(size, crc, sources [(paths, crc)], ops)] of each variant of a delta record."""
    variants, o = [], 6
    for _ in range(rec[5]):
        n = struct.unpack_from('<I', rec, o)[0]
        variants.append(parse_variant(rec[o + 4:o + 4 + n]))
        o += 4 + n
    return variants


def parse_variant(body):
    size, crc, count = struct.unpack_from('<IQB', body, 0)
    o, sources = 13, []
    for _ in range(count):
        paths = []
        for _ in range(body[o]):
            n = struct.unpack_from('<H', body, o + 1)[0]
            paths.append(body[o + 3:o + 3 + n].decode())
            o += 2 + n
        sources.append((paths, struct.unpack_from('<Q', body, o + 1)[0]))
        o += 9
    ops = []
    while o < len(body):
        if body[o] == 0:
            ops.append((0,) + struct.unpack_from('<BII', body, o + 1)); o += 10
        else:
            n = struct.unpack_from('<I', body, o + 1)[0]
            ops.append((1, body[o + 5:o + 5 + n])); o += 5 + n
    return size, crc, sources, ops


def rebuild(data, base):
    """A pack file's content: itself, or the first variant of its delta whose sources the base has."""
    if not data.startswith(MAGIC):
        return data
    if data[4] != VERSION:
        raise ValueError(f'delta version {data[4]}')
    missing = None
    for size, want, sources, ops in parse(data):
        found = []
        for s, (paths, crc) in enumerate(sources):
            copies = [(op[2], op[3]) for op in ops if op[0] == 0 and op[1] == s]
            src = base.find(paths, copies, crc)
            if src is None:
                missing = missing or f'{" or ".join(paths)} (crc {crc:016x})'
                break
            found.append(src)
        if len(found) < len(sources):
            continue
        out = bytearray()
        for op in ops:
            out += found[op[1]][op[2]:op[2] + op[3]] if op[0] == 0 else op[1]
        if len(out) != size or crc64(out) != want:
            raise ValueError('rebuilt file differs from the one the pack was made from')
        return bytes(out)
    raise ValueError(f'no base file at {missing} with the bytes it copies')


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


class Finder:
    """What a variant against one base draws on: the base, its whole files by content, its block index."""

    def __init__(self, base):
        self.base = base
        self.whole = {}
        for key in base.keys():
            self.whole.setdefault(base.sha(key), key)           # a lower archive (oot.o2r) wins a tie
        self.keys, self.index = block_index(base)

    def variant(self, dst, path):
        """A variant building dst from this base, or None when nothing is worth copying."""
        same = self.whole.get(hashlib.sha1(dst).digest())
        if same is not None:
            return delta(dst, [(same, dst)], self.base)
        picked = self.base.at(path)[:1]
        picked += [k for k in candidates(dst, self.keys, self.index) if k not in picked]
        picked = picked[:MAX_SOURCES]
        return delta(dst, [(k, self.base.read(k)) for k in picked], self.base) if picked else None


def own_bytes(variants):
    return max(sum(len(op[1]) for op in parse_variant(v)[3] if op[0] == 1) for v in variants)


def make(mod, pack, bases, sources=None, also=()):
    base = Base(bases, also)
    if sources is not None:
        base.sources = sources
    common = Finder(base)
    each = None                         # one Finder per base, made when a file first needs per-base variants
    stats = collections.defaultdict(lambda: [0, 0])
    own, deltas = 0, []
    with zipfile.ZipFile(mod) as zin, zipfile.ZipFile(pack, 'w', zipfile.ZIP_DEFLATED) as zout:
        for info in zin.infolist():
            if info.filename == LIST:
                sys.exit(f'{mod} already has a {LIST}')
            dst = zin.read(info)
            if dst.startswith(MAGIC):
                sys.exit(f'{info.filename} starts with {MAGIC!r} and would read as a delta')
            if info.filename not in KEEP and all(b.top(info.filename) == dst for b in [base] + base.also):
                stats['same'][0] += 1; stats['same'][1] += len(dst)     # the player's base has it already
                continue
            rec, kind = dst, 'own'
            if info.filename not in KEEP and len(dst) >= 64:
                one = common.variant(dst, info.filename)
                variants, kind = ([one], 'delta') if one is not None else ([], 'own')
                if base.also and (one is None or own_bytes([one]) >= VARIANT_GAIN):
                    if each is None:
                        each = [Finder(Base(bases))] + [Finder(Base(a)) for a in also]
                    per = [f.variant(dst, info.filename) for f in each]
                    if all(v is not None for v in per) and \
                            own_bytes(per) + VARIANT_GAIN <= (own_bytes(variants) if variants else len(dst)):
                        variants, kind = per, 'split'
                if variants:
                    rec = record(variants)
            # Stored whole only when that is smaller and the delta keeps little of the base out: keeping the base's
            # bytes out of the pack comes before its size.
            if len(rec) >= len(dst) and len(inserts(rec)) + VARIANT_GAIN > len(dst):
                rec, kind = dst, 'own'
            if kind != 'own':
                if any(rebuild(rec, b) != dst for b in [base] + base.also):       # over every version given
                    sys.exit(f'{info.filename}: the delta does not rebuild')
                deltas.append(info.filename)
            own += len(inserts(rec))
            stats[kind][0] += 1; stats[kind][1] += len(dst)
            zout.writestr(info, rec)
        zout.writestr(LIST, json.dumps({'files': deltas}, indent=1) + '\n')
    for kind, (n, size) in stats.items():
        print(f'  {kind:5}: {n:5} files, {size / 1e6:6.2f} MB of the mod')
    print(f'  carries {own / 1e6:.2f} MB of the mod\'s own bytes; {os.path.getsize(pack) / 1e6:.2f} MB zipped')


def inserts(rec):
    """The bytes of a pack file that are its own (not copied from the base), every variant's."""
    if not rec.startswith(MAGIC):
        return rec
    return b''.join(op[1] for v in parse(rec) for op in v[3] if op[0] == 1)


def apply(pack, out, bases):
    base = Base(bases)
    with zipfile.ZipFile(pack) as zin, zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as zout:
        for info in zin.infolist():
            if info.filename != LIST:
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
    if cmd == 'make' and len(sys.argv) >= 5:
        sets = [[]]
        for arg in sys.argv[4:]:
            if arg == '--also':
                sets.append([])
            else:
                sets[-1].append(arg)
        if not all(sets):
            sys.exit(__doc__)
        make(sys.argv[2], sys.argv[3], sets[0], also=sets[1:])
    elif cmd == 'apply' and len(sys.argv) >= 5:
        apply(sys.argv[2], sys.argv[3], sys.argv[4:])
    elif cmd == 'scan' and len(sys.argv) >= 4:
        scan(sys.argv[2], sys.argv[3:])
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
