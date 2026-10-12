"""MissingShip's mod builder (MS-154): The Missing Link's mod archive, built from the player's own files.

    build_mod.py <files...> --oot-o2r <oot.o2r> --out <MissingLink_Scenes.o2r>
    build_mod.py <files...> --unbound <Unbound app> [--pack] [--oot-o2r <oot.o2r>] [--out <MissingLink_Unbound.o2r>]

<files> are whatever the player has, in any order. The builder tells them apart by their contents:
  - The Missing Link v2.0 ROM, already patched; or
  - an Ocarina of Time USA 1.0 ROM plus Kaze's patch: either of his .bps files (big- or little-endian), or the .zip
    from his page as downloaded (romhacking.net hack 5334).
ROMs may be in .z64, .n64 or .v64 byte order, loose or inside a .zip. `oot.o2r` is the archive Ship of Harkinian made
from the player's OoT ROM. (--rom, --patch and --ml-rom still work, as names for the same inputs.) --save-rom keeps the
ML ROM it used, so MissingShip can rebuild the mod after an update without asking again.

With --unbound it writes ML's SoH: Unbound layer instead (tools/unbound_layer.py), and with --pack that layer as a
pack of deltas against the player's own base (tools/deltapack.py): ML's own bytes and none of the game's, the file we
distribute for players to drop into Unbound's mods/ folder.

It runs the same pipeline as ms_cycle.py, in a temporary work folder, replaying pipeline/baseline.json instead of
reading the debug ROM, with SoH's files and ZAPD from the builder's own folder. The packages ship it frozen per OS,
so players need no Python. A frozen builder also runs each pipeline step: `<builder> --run-script <step.py>`.
"""
import argparse, os, runpy, shutil, subprocess, sys, tempfile, zipfile, zlib

ML_CRC = 0x59CD42A3        # The Missing Link v2.0, .z64 byte order
OOT10_CRC = 0xCD16C529     # OoT NTSC-U 1.0, .z64 byte order: the ROM Kaze's patch is made for
FROZEN = getattr(sys, 'frozen', False)
HERE = sys._MEIPASS if FROZEN else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # noqa
# The builder's folder: pipeline/ (these scripts and their data), soh/ (SoH's files the pipeline reads), ZAPD.
PIPE_SRC = os.path.join(HERE, 'pipeline')
SKIP = shutil.ignore_patterns('*.z64', '*.n64', '*.v64', '*.otr', '*.o2r', '*.log', 'out', '__pycache__',
                              'ml_code.bin')
Z64, V64, N64 = b'\x80\x37\x12\x40', b'\x37\x80\x40\x12', b'\x40\x12\x37\x80'   # first word of each byte order


class BuildError(Exception):
    pass


def fail(msg):
    print(f'MissingShip builder: {msg}', file=sys.stderr)
    sys.exit(1)


def swap16(data):
    b = bytearray(data)
    b[0::2], b[1::2] = data[1::2], data[0::2]
    return bytes(b)


def swap32(data):
    b = bytearray(data)
    b[0::4], b[1::4], b[2::4], b[3::4] = data[3::4], data[2::4], data[1::4], data[0::4]
    return bytes(b)


def to_z64(data):
    """A ROM in big-endian (.z64) order, from .z64, .n64 (32-bit little-endian) or .v64 (16-bit byte-swapped)."""
    return {Z64: lambda d: d, V64: swap16, N64: swap32}[data[:4]](data)


def gather(paths):
    """(roms, patches) from the given files: each file, or each member of a .zip, is a ROM (by its first word), a
    BPS patch (by its magic) or ignored (readmes)."""
    roms, patches = [], []
    for path in paths:
        if not os.path.isfile(path):
            raise BuildError(f'{path} not found')
        if zipfile.is_zipfile(path):
            with zipfile.ZipFile(path) as z:
                items = [(f'{os.path.basename(path)}:{n}', z.read(n)) for n in z.namelist() if not n.endswith('/')]
        else:
            items = [(os.path.basename(path), open(path, 'rb').read())]
        found = False
        for name, data in items:
            if data[:4] in (Z64, V64, N64):
                roms.append((name, data)); found = True
            elif data[:4] == b'BPS1':
                patches.append((name, data)); found = True
        if not found:
            raise BuildError(f'{os.path.basename(path)} is not an N64 ROM, a BPS patch or a zip holding one')
    return roms, patches


def ml_rom(paths):
    """The ML v2.0 ROM in .z64 order: a given ML ROM, or an OoT USA 1.0 ROM with one of the given patches applied
    (in whichever byte order that patch was made for). Every result is checked by CRC."""
    roms, patches = gather(paths)
    if not roms:
        raise BuildError('no ROM given: add The Missing Link v2.0 ROM, or your Ocarina of Time USA 1.0 ROM '
                         'and Kaze\'s patch')
    oot10 = None
    for name, data in roms:
        z64 = to_z64(data)
        crc = zlib.crc32(z64)
        if crc == ML_CRC:
            return z64
        if crc == OOT10_CRC:
            oot10 = z64
    if oot10 is None:
        names = ', '.join(n for n, _ in roms)
        raise BuildError(f'{names}: not The Missing Link v2.0 or Ocarina of Time USA 1.0. Kaze\'s patch needs '
                         'exactly OoT USA 1.0 (NTSC 1.0)')
    if not patches:
        raise BuildError('found Ocarina of Time USA 1.0 but no patch: add Kaze\'s .bps, or the zip from his page')
    sys.path.insert(0, PIPE_SRC)
    from bps_apply import apply_bps
    forms = {zlib.crc32(f): f for f in (oot10, swap16(oot10), swap32(oot10))}
    for name, patch in patches:
        source_crc = int.from_bytes(patch[-12:-8], 'little')      # BPS footer: source, target, patch CRC32s
        if source_crc not in forms:
            continue
        out = to_z64(apply_bps(forms[source_crc], patch))
        if zlib.crc32(out) == ML_CRC:
            return out
    raise BuildError('none of the patches makes The Missing Link v2.0 from OoT USA 1.0: use the v2.0 patch from '
                     'Kaze\'s page (tml_z64_2_0_big_endian.bps or tml_n64_2_0_little_endian.bps)')


def zapd():
    if FROZEN:
        path = os.path.join(HERE, 'ZAPD.exe' if os.name == 'nt' else 'ZAPD.out')
    else:                                               # a checkout: the engine's ZAPD, as ms_cycle.py uses
        sys.path.insert(0, PIPE_SRC)
        import mspaths
        path = mspaths.ZAPD
    path = os.environ.get('MS_ZAPD') or path
    if not os.path.exists(path):
        fail(f'ZAPD is missing ({path})')
    return path


def unbound_home():
    """Where Unbound keeps oot.o2r and mods/: SHIP_HOME, else on macOS the folder SoH's own builds use
    (libultraship Context::GetAppDirectoryPath, SDL_GetPrefPath). Elsewhere the builds are portable: no default."""
    if os.environ.get('SHIP_HOME'):
        return os.path.expanduser(os.environ['SHIP_HOME'])
    if sys.platform == 'darwin':
        return os.path.expanduser('~/Library/Application Support/com.shipofharkinian.soh')
    return None


def unbound_layer(args, step, env, oot, mod, work):
    """MissingLink_Unbound.o2r from the built mod, with the player's Unbound (tools/unbound_layer.py). The Unbound
    sources it compares against are the builder's own copy (unbound/), or in a checkout Unbound at the fork's SoH
    version (mspaths.UNBOUND_SRC)."""
    tools = os.path.join(HERE, 'tools')
    if FROZEN:
        src = os.environ.get('MS_UNBOUND_SRC') or os.path.join(HERE, 'unbound')
    else:
        sys.path.insert(0, PIPE_SRC)
        import mspaths
        src = mspaths.UNBOUND_SRC
        problem = mspaths.unbound_src_problem(src)
        if problem:
            fail(problem)
    layer = os.path.join(work, 'mod', 'MissingLink_Unbound.o2r')
    print('MissingShip builder: making the SoH: Unbound ' + ('pack' if args.pack else 'layer'))
    r = subprocess.run(step + [os.path.join(tools, 'unbound_layer.py'), '--unbound', os.path.abspath(args.unbound),
                               '--unbound-src', src, '--oot-o2r', oot, '--mod', mod, '--out', layer] +
                       (['--pack'] if args.pack else []) + (['--stock'] if args.stock else []) +
                       [x for other in args.also_base for x in ('--also-base', os.path.abspath(other))],
                       env=env, capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(layer):
        print((r.stdout + r.stderr).strip(), file=sys.stderr)
        fail('making the SoH: Unbound ' + ('pack' if args.pack else 'layer') + ' failed')
    for line in r.stdout.splitlines():                  # what a stock Unbound will not read (tools/unbound_stock.py)
        if line.startswith(('stock Unbound', '  ')):
            print('MissingShip builder: ' + line if line[0] != ' ' else line)
    return layer


def build(args):
    oot = os.path.abspath(args.oot_o2r)
    if not os.path.exists(oot):
        fail(f'{oot} not found: start the game once so it makes oot.o2r from your ROM')
    out = os.path.abspath(args.out)
    work = tempfile.mkdtemp(prefix='missingship-build-')
    try:
        shutil.copytree(PIPE_SRC, os.path.join(work, 'pipeline'), ignore=SKIP)
        os.makedirs(os.path.join(work, 'mod'))
        rom_path = os.path.join(work, 'tml_2_0.z64')
        try:
            rom = ml_rom(args.files)
        except BuildError as e:
            fail(str(e))
        with open(rom_path, 'wb') as f:
            f.write(rom)
        if args.save_rom:                               # the game keeps it, to rebuild after an update unasked
            save = os.path.abspath(args.save_rom)
            os.makedirs(os.path.dirname(save), exist_ok=True)
            with open(save + '.tmp', 'wb') as f:
                f.write(rom)
            os.replace(save + '.tmp', save)
        # the folder holding soh/, the SoH files the steps read: the builder's own, or a checkout's engine/
        soh = os.environ.get('MS_SHIPWRIGHT') or (HERE if FROZEN else os.path.join(HERE, 'engine'))
        env = dict(os.environ, PYTHONUTF8='1', MS_ML_ROM=rom_path, MS_OOT_O2R=oot, MS_SHIPWRIGHT=soh,
                   MS_ZAPD=zapd(), MS_ZAPD_CWD=os.path.join(soh, 'soh'), MS_BASELINE='replay',
                   MS_DEBUG_ROM=os.path.join(work, 'no-debug-rom.z64'))
        step = [sys.executable, '--run-script'] if FROZEN else [sys.executable]
        log_path = os.path.join(work, 'build.log')
        with open(log_path, 'w', encoding='utf-8') as log:
            r = subprocess.run(step + [os.path.join(work, 'pipeline', 'ms_cycle.py'), '--no-launch'], env=env,
                               stdout=None if args.verbose else log,
                               stderr=None if args.verbose else subprocess.STDOUT)
        if r.returncode != 0:
            if not args.verbose:
                tail = open(log_path, encoding='utf-8', errors='replace').read().splitlines()[-25:]
                print('\n'.join(tail), file=sys.stderr)
            fail('building the mod failed')
        built = os.path.join(work, 'mod', 'MissingLink_Scenes.o2r')
        if args.unbound:                                # SoH: Unbound reads ML as its own mod layer
            built = unbound_layer(args, step, env, oot, built, work)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        shutil.copyfile(built, out + '.tmp')
        os.replace(out + '.tmp', out)
        print(f'MissingShip builder: wrote {out}')
    finally:
        if not args.keep:
            shutil.rmtree(work, ignore_errors=True)
        else:
            print(f'work folder kept: {work}')


def main():
    if sys.argv[1:2] == ['--run-script']:                   # a pipeline step, inside the frozen builder
        script = sys.argv[2]
        sys.argv = [script] + sys.argv[3:]
        sys.path.insert(0, os.path.dirname(script))
        runpy.run_path(script, run_name='__main__')
        return
    p = argparse.ArgumentParser(description='Build The Missing Link mod archive for MissingShip.')
    p.add_argument('files', nargs='*', help='The Missing Link v2.0 ROM, or an OoT USA 1.0 ROM and Kaze\'s patch '
                                            '(.bps or the .zip from his page), in any order')
    p.add_argument('--rom', '--ml-rom', '--patch', dest='more', action='append', default=[], help=argparse.SUPPRESS)
    p.add_argument('--oot-o2r', help='the oot.o2r Ship of Harkinian made from your ROM (with --unbound on macOS, '
                                     'default: the one in its home folder)')
    p.add_argument('--out', help='where to write MissingLink_Scenes.o2r (with --unbound, MissingLink_Unbound.o2r; '
                                 'on macOS the default is the mods folder in its home folder)')
    p.add_argument('--unbound', help="build for SoH: Unbound: its macOS app, or its binary with soh.o2r beside it")
    p.add_argument('--pack', action='store_true', help="with --unbound, write the mod as a pack of deltas against the "
                                                       "player's own base, carrying none of the game's data: what we "
                                                       "distribute (give an oot.o2r from Unbound's own extractor)")
    p.add_argument('--stock', action='store_true', help="with --unbound, for a stock Unbound (roborich's, or with "
                                                        "skijer's code mods): exits only to entrances it has, no "
                                                        "unbound/scenes.json; not with --pack")
    p.add_argument('--also-base', action='append', default=[], metavar='OOT_O2R',
                   help='with --pack, an oot.o2r another SoH 9.x extractor wrote from the same ROM, so the pack '
                        'works over it too (players keep an oot.o2r from whichever SoH they had)')
    p.add_argument('--save-rom', help='also write The Missing Link v2.0 ROM (.z64) here')
    p.add_argument('--verbose', action='store_true', help='show every pipeline step')
    p.add_argument('--keep', action='store_true', help='keep the temporary work folder')
    args = p.parse_args()
    args.files += args.more
    if not args.files:
        p.error('give The Missing Link v2.0 ROM, or an OoT USA 1.0 ROM and Kaze\'s patch')
    if args.pack and not args.unbound:
        p.error('--pack needs --unbound: packs are read by SoH: Unbound')
    if args.stock and (args.pack or not args.unbound):
        p.error('--stock needs --unbound and not --pack: stock Unbound does not read packs')
    if args.also_base and not args.pack:
        p.error('--also-base needs --pack')
    if args.unbound and (not args.oot_o2r or not args.out):
        home = unbound_home()
        if home is None:
            p.error('give --oot-o2r and --out: the folder Unbound keeps oot.o2r and mods/ in')
        args.oot_o2r = args.oot_o2r or os.path.join(home, 'oot.o2r')
        args.out = args.out or os.path.join(home, 'mods', 'MissingLink_Unbound.o2r')
    if not args.oot_o2r or not args.out:
        p.error('give --oot-o2r and --out')
    build(args)


if __name__ == '__main__':
    main()
