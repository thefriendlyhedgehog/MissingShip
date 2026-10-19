"""Check that release packages carry no game data: nothing from OoT's ROM or ML's (MS-156).

    python3 tools/nodata.py <package>...
    python3 tools/nodata.py --rom <OoT USA 1.0 ROM> --patch <Kaze's v2.0 patch> <package>...
    python3 tools/nodata.py --git <repository>...      every file in the repository's whole history (all refs)

With no --rom it reads OoT USA 1.0 and ML v2.0 where mspaths says (MS_OOT10_ROM, MS_ML_ROM). tools/package.py runs
it on every package it makes.

A package is a .zip, .tar.gz, .dmg or folder. Every file in it is scanned, and so is every file inside an archive
in it (.zip, .o2r, .otr, PyInstaller's base_library.zip), as raw bytes; in a text file, so is every list of 16 or
more numbers (a C array, a JSON table, a patch), as the bytes it spells. A file fails when:
  - it is a ROM (an N64 header in any byte order), or
  - it holds a run of game data: every file in both ROMs' dmadata, decompressed, is cut into 16-byte blocks, also
    16- and 32-bit byte-swapped (data compiled into a little-endian program), and a run is blocks from the same ROM
    file next to each other in the package file for MIN_RUN bytes or more. Blocks with few distinct bytes (zeros,
    padding, flat colours) and blocks that count up by a fixed step (as 8-, 16- or 32-bit numbers, e.g. a
    character table) are left out: they turn up everywhere.
    A run from an asset file (scenes, rooms, objects, textures, audio, ...) fails at any length. A run from a code
    file (boot, code, the overlays) of up to CODE_TABLE bytes is a constant table the C code carries (a speed or
    angle list in an actor ported to the code mod; SoH's own source has the same): it is listed, not failed.

The ROMs never leave the machine: the fingerprints are 64-bit hashes, cached in the user cache folder by the ROM's
SHA-1. Exits 1 if any package fails.
"""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', 'pipeline'))
import argparse, hashlib, io, os, re, struct, subprocess, sys, tarfile, tempfile, zipfile
import numpy as np
import build_mod
from mspaths import ML_ROM, OOT10_ROM
from yaz0 import get_file

BLOCK = 16
MIN_DISTINCT = 6        # a block needs this many different byte values to count
MIN_RUN = 64            # bytes of adjacent matching blocks that make a run
CODE_TABLE = 1024       # the longest run from a code file that passes
JR_RA = 0x03E00008      # MIPS `jr $ra`: a file with a few of these is code
MULT = np.uint64(0x9E3779B97F4A7C15)
ROM_MAGIC = (b'\x80\x37\x12\x40', b'\x37\x80\x40\x12', b'\x40\x12\x37\x80')
ARCHIVES = ('.zip', '.o2r', '.otr')
NUMBER = re.compile(rb'(?<![\w.])(0[xX][0-9a-fA-F]+|-?\d+)(?![\w.])')
BETWEEN = re.compile(rb'[\s,{}+]*')
MIN_NUMBERS = 16


def keys_of(a, b):
    with np.errstate(over='ignore'):
        return (a * MULT) ^ b


def dmadata(rom):
    """The ROM's dmadata entries: the table that starts with makerom (0, 0x1060) and whose own entry follows."""
    start = rom.find(struct.pack('>IIII', 0, 0x1060, 0, 0))
    if start < 0:
        sys.exit('no dmadata in a ROM given')
    entries = []
    for p in range(start, len(rom), 16):
        e = struct.unpack('>IIII', rom[p:p + 16])
        if e == (0, 0, 0, 0):
            break
        entries.append(e)
    return entries


def rom_files(rom):
    for i, (vs, ve, rs, re) in enumerate(dmadata(rom)):
        if rs == 0xFFFFFFFF or ve <= vs:
            continue            # a file not in the ROM
        yield i, vs, get_file(rom, vs, ve, rs, re)


def is_code(i, data):
    """makerom, boot, code and the overlays: an overlay ends with the offset of its relocation header, whose
    .text + .data + .rodata sizes add up to where it starts."""
    if i <= 1:
        return True
    words = np.frombuffer(data[:len(data) // 4 * 4], dtype='>u4')
    if np.count_nonzero(words == JR_RA) >= 4:
        return True
    if len(data) < 0x20:
        return False
    off = struct.unpack('>I', data[-4:])[0]
    h = len(data) - off
    if not (0 < off and 0 <= h and h + 20 <= len(data)):
        return False
    text, dat, rodata, bss, nrel = struct.unpack('>5I', data[h:h + 20])
    return text + dat + rodata == h and h + 20 + 4 * nrel <= len(data)


def steps(data):
    """For each 16-byte block: whether it counts up by a fixed step as 8-, 16- or 32-bit numbers, either byte order."""
    out = np.zeros(len(data) // BLOCK, dtype=bool)
    for dtype in ('u1', '<u2', '>u2', '<u4', '>u4'):
        rows = np.frombuffer(data, dtype=dtype).reshape(len(out), -1).astype(np.int64)
        d = np.diff(rows, axis=1)
        out |= np.all(d == d[:, :1], axis=1)
    return out


def block_keys(data):
    """Keys of the 16-byte blocks at 16-byte steps that have MIN_DISTINCT byte values, and their offsets."""
    n = len(data) // BLOCK * BLOCK
    rows = np.frombuffer(data[:n], dtype=np.uint8).reshape(-1, BLOCK)
    srt = np.sort(rows, axis=1)
    distinct = 1 + np.count_nonzero(np.diff(srt, axis=1), axis=1)
    keep = (distinct >= MIN_DISTINCT) & ~steps(data[:n])
    keep = np.nonzero(keep)[0]
    u = np.frombuffer(data[:n], dtype='<u8').reshape(-1, 2)[keep]
    return keys_of(u[:, 0], u[:, 1]), (keep * BLOCK).astype(np.uint32)


def swapped(data, width):
    n = len(data) // width * width
    return np.frombuffer(data[:n], dtype=f'>u{width}').astype(f'<u{width}').tobytes()


def cache_dir():
    if sys.platform == 'darwin':
        base = os.path.expanduser('~/Library/Caches')
    elif sys.platform == 'win32':
        base = os.environ.get('LOCALAPPDATA', os.path.expanduser('~'))
    else:
        base = os.environ.get('XDG_CACHE_HOME', os.path.expanduser('~/.cache'))
    return os.path.join(base, 'MissingShip', 'nodata')


def rom_index(rom):
    """(keys sorted, dmadata index of each, offset in the decompressed file, the code files' indices), cached by the
    ROM's SHA-1."""
    sha = hashlib.sha1(rom).hexdigest()
    path = os.path.join(cache_dir(), f'{sha}-{BLOCK}-{MIN_DISTINCT}-3.npz')
    if os.path.exists(path):
        z = np.load(path)
        return z['keys'], z['files'], z['off'], z['code']
    keys, files, offs, code = [], [], [], []
    for i, vs, data in rom_files(rom):
        if is_code(i, data):
            code.append(i)
        for variant in (data, swapped(data, 2), swapped(data, 4)):
            k, o = block_keys(variant)
            keys.append(k); offs.append(o); files.append(np.full(len(k), i, dtype=np.uint32))
    keys, files, offs = np.concatenate(keys), np.concatenate(files), np.concatenate(offs)
    order = np.argsort(keys, kind='stable')
    keys, files, offs = keys[order], files[order], offs[order]
    os.makedirs(os.path.dirname(path), exist_ok=True)
    code = np.array(code, dtype=np.uint32)
    np.savez(path, keys=keys, files=files, off=offs, code=code)
    return keys, files, offs, code


class Index:
    def __init__(self, roms):
        parts = [rom_index(rom) for rom in roms]
        self.keys = np.concatenate([p[0] for p in parts])
        self.file = np.concatenate([p[1] | np.uint32(n << 16) for n, p in enumerate(parts)])
        self.code = {(n, int(i)) for n, p in enumerate(parts) for i in p[3]}
        order = np.argsort(self.keys, kind='stable')
        self.keys, self.file = self.keys[order], self.file[order]

    def runs(self, data):
        """(start, length, rom, dmadata index) of each run of adjacent blocks from one ROM file in data."""
        hits = []
        for align in (0, 4):           # data is 4-byte aligned in a ROM; a copy in a program is too
            n = (len(data) - align) // 8 * 8
            if n < BLOCK:
                continue
            u = np.frombuffer(data[align:align + n], dtype='<u8')
            k = keys_of(u[:-1], u[1:])
            i = np.searchsorted(self.keys, k)
            i[i == len(self.keys)] = 0
            hit = np.nonzero(self.keys[i] == k)[0]
            for h in hit:
                hits.append((align + 8 * int(h), int(self.file[i[h]])))
        hits.sort()
        out = []
        start = end = None
        src = None
        for pos, f in hits:
            if start is not None and pos <= end and f == src:
                end = max(end, pos + BLOCK)
                continue
            if start is not None and end - start >= MIN_RUN:
                out.append((start, end - start, src >> 16, src & 0xFFFF))
            start, end, src = pos, pos + BLOCK, f
        if start is not None and end - start >= MIN_RUN:
            out.append((start, end - start, src >> 16, src & 0xFFFF))
        return out


def number_lists(text):
    """(line, bytes) of each list of MIN_NUMBERS or more numbers in a text file, as the big-endian bytes it spells:
    each number as wide as the list's widest hex number (or its largest decimal) needs."""
    run, end = [], None

    def spell(run):
        hexes = [t for t in run if t[:2] in (b'0x', b'0X')]
        values = [int(t, 16 if t.lstrip(b'+-')[:2] in (b'0x', b'0X') else 10) for t in run]   # decimal may be 01
        if hexes:
            digits = max(len(t) - 2 for t in hexes)
            width = 1 if digits <= 2 else 2 if digits <= 4 else 4 if digits <= 8 else 8
        else:
            lo, hi = min(values), max(values)
            width = 1 if -128 <= lo and hi <= 255 else 2 if -32768 <= lo and hi <= 65535 else 4
        mask = (1 << (8 * width)) - 1
        return b''.join((v & mask).to_bytes(width, 'big') for v in values)

    start = 0
    for m in NUMBER.finditer(text):
        if run and not BETWEEN.fullmatch(text, end, m.start()):
            if len(run) >= MIN_NUMBERS:
                yield text.count(b'\n', 0, start) + 1, spell(run)
            run = []
        if not run:
            start = m.start()
        run.append(m.group(1))
        end = m.end()
    if len(run) >= MIN_NUMBERS:
        yield text.count(b'\n', 0, start) + 1, spell(run)


def git_blobs(repo):
    """(path@blob, bytes) of every file in the repository's history, each blob once."""
    listed = subprocess.run(['git', '-C', repo, 'rev-list', '--objects', '--all'], check=True, capture_output=True,
                            text=True).stdout.splitlines()
    names = {}
    for line in listed:
        sha, _, path = line.partition(' ')
        if path:
            names.setdefault(sha, path)
    cat = subprocess.Popen(['git', '-C', repo, 'cat-file', '--batch'], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    try:
        for sha, path in names.items():
            cat.stdin.write(sha.encode() + b'\n')
            cat.stdin.flush()
            header = cat.stdout.readline().split()
            size = int(header[2])
            data = cat.stdout.read(size)
            cat.stdout.read(1)
            if header[1] == b'blob':
                yield f'{path}@{sha[:10]}', data
    finally:
        cat.stdin.close()
        cat.wait()


def walk(path):
    """(name, bytes) of every file in a package, and of every file in the archives inside it."""
    if os.path.isdir(path):
        for top, dirs, files in os.walk(path):
            dirs.sort()
            for f in sorted(files):
                p = os.path.join(top, f)
                if os.path.islink(p) or not os.path.isfile(p):
                    continue
                yield from member(os.path.relpath(p, path), open(p, 'rb').read())
    elif path.endswith('.dmg'):
        mnt = tempfile.mkdtemp(prefix='nodata-')
        subprocess.run(['hdiutil', 'attach', '-nobrowse', '-readonly', '-mountpoint', mnt, path], check=True,
                       capture_output=True)
        try:
            yield from walk(mnt)
        finally:
            subprocess.run(['hdiutil', 'detach', mnt], capture_output=True)
            os.rmdir(mnt)
    elif path.endswith(('.tar.gz', '.tgz', '.tar')):
        with tarfile.open(path) as t:
            for m in t.getmembers():
                if m.isfile():
                    yield from member(m.name, t.extractfile(m).read())
    else:
        yield from member(os.path.basename(path), open(path, 'rb').read())


def member(name, data):
    yield name, data
    if name.lower().endswith(ARCHIVES) and zipfile.is_zipfile(io.BytesIO(data)):
        with zipfile.ZipFile(io.BytesIO(data)) as z:
            for info in z.infolist():
                if not info.is_dir():
                    yield from member(f'{name}/{info.filename}', z.read(info))


def check(package, index, rom_names, verbose, git=False):
    problems, tables, files, size = [], [], 0, 0
    for name, data in (git_blobs(package) if git else walk(package)):
        files += 1
        size += len(data)
        if data[:4] in ROM_MAGIC and len(data) >= 0x100000:
            problems.append(f'{name}: an N64 ROM')
            continue
        found = [(f'+0x{start:x}', run) for start, *run in index.runs(data)]
        if b'\0' not in data[:8192]:
            for line, spelled in number_lists(data):
                found += [(f'line {line}', run) for start, *run in index.runs(spelled)]
        for where, (length, rom, f) in found:
            code = (rom, f) in index.code
            line = f'{name} {where}: {length} bytes of {rom_names[rom]} file {f} ({"code" if code else "assets"})'
            (tables if code and length <= CODE_TABLE else problems).append(line)
    status = 'FAIL' if problems else 'PASS'
    print(f'{status}  {package}: {files} files, {size / 1e6:.1f} MB scanned, '
          + (f'{len(problems)} runs of game data' if problems else 'no game data')
          + (f', {len(tables)} constant tables from game code' if tables else ''))
    for p in problems[:None if verbose else 20]:
        print(f'      {p}')
    if len(problems) > 20 and not verbose:
        print(f'      ... and {len(problems) - 20} more (--verbose lists them all)')
    for t in tables if verbose else []:
        print(f'      table: {t}')
    return not problems


def default_roms():
    """OoT USA 1.0 and ML v2.0 from mspaths, or [] when either is missing."""
    return [OOT10_ROM, ML_ROM] if os.path.exists(OOT10_ROM) and os.path.exists(ML_ROM) else []


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--rom', action='append', help='OoT USA 1.0, or ML v2.0 (repeatable; default: both, from mspaths)')
    ap.add_argument('--patch', help="Kaze's v2.0 patch: adds the ML ROM made from the OoT ROM")
    ap.add_argument('--verbose', action='store_true')
    ap.add_argument('--git', action='store_true', help='the arguments are git repositories: scan their whole history')
    ap.add_argument('packages', nargs='+')
    a = ap.parse_args()
    a.rom = a.rom or default_roms()
    if not a.rom:
        sys.exit(f'nodata: no ROMs: put OoT USA 1.0 at {OOT10_ROM} and ML v2.0 at {ML_ROM}, or pass --rom')
    roms, names = [], []
    for path in a.rom:
        data = build_mod.to_z64(open(path, 'rb').read())
        roms.append(data); names.append(os.path.basename(path))
    if a.patch:
        try:
            roms.append(build_mod.ml_rom(a.rom + [a.patch]))
        except build_mod.BuildError as e:
            sys.exit(f'nodata: {e}')
        names.append('ML v2.0')
    index = Index(roms)
    ok = all([check(p, index, names, a.verbose, a.git) for p in a.packages])
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
