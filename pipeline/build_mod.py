"""MissingShip's mod builder (MS-154): The Missing Link's mod archive, built from the player's own files.

    build_mod.py --rom <OoT USA 1.0 ROM> --patch <Kaze's ML v2.0 .bps> --oot-o2r <oot.o2r> --out <MissingLink_Scenes.o2r>
    build_mod.py --ml-rom <The Missing Link v2.0 ROM>              --oot-o2r <oot.o2r> --out <MissingLink_Scenes.o2r>

Players download Kaze's patch from his page (romhacking.net hack 5334, `tml_z64_2_0_big_endian.bps`) and apply it to
their OoT USA 1.0 ROM; or they bring a ROM already patched. ROMs in .z64, .n64 or .v64 byte order are all accepted.
`oot.o2r` is the archive Ship of Harkinian made from the player's OoT ROM.

It runs the same pipeline as ms_cycle.py, in a temporary work folder, replaying pipeline/baseline.json instead of
reading the debug ROM, with SoH's files and ZAPD from the builder's own folder. The packages ship it frozen per OS,
so players need no Python. A frozen builder also runs each pipeline step: `<builder> --run-script <step.py>`.
"""
import argparse, os, runpy, shutil, subprocess, sys, tempfile, zlib

ML_CRC = 0x59CD42A3        # The Missing Link v2.0, .z64 byte order
OOT10_CRC = 0xCD16C529     # OoT NTSC-U 1.0, .z64 byte order: the ROM Kaze's patch is made for
FROZEN = getattr(sys, 'frozen', False)
HERE = sys._MEIPASS if FROZEN else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # noqa
# The builder's folder: pipeline/ (these scripts and their data), soh/ (SoH's files the pipeline reads), ZAPD.
PIPE_SRC = os.path.join(HERE, 'pipeline')
SKIP = shutil.ignore_patterns('*.z64', '*.n64', '*.v64', '*.otr', '*.o2r', '*.log', 'out', '__pycache__',
                              'ml_code.bin')


def fail(msg):
    print(f'MissingShip builder: {msg}', file=sys.stderr)
    sys.exit(1)


def to_z64(data):
    """A ROM in big-endian (.z64) order, from .z64, .n64 (32-bit little-endian) or .v64 (16-bit byte-swapped)."""
    magic = data[:4]
    if magic == b'\x80\x37\x12\x40':
        return data
    b = bytearray(data)
    if magic == b'\x37\x80\x40\x12':
        b[0::2], b[1::2] = data[1::2], data[0::2]
    elif magic == b'\x40\x12\x37\x80':
        b[0::4], b[1::4], b[2::4], b[3::4] = data[3::4], data[2::4], data[1::4], data[0::4]
    else:
        fail('that file is not an N64 ROM')
    return bytes(b)


def ml_rom(args):
    """The ML v2.0 ROM, from --ml-rom or from --rom + --patch, checked by CRC."""
    if args.ml_rom:
        rom = to_z64(open(args.ml_rom, 'rb').read())
        if zlib.crc32(rom) != ML_CRC:
            fail(f'{args.ml_rom} is not The Missing Link v2.0 (CRC32 {zlib.crc32(rom):08X}, want {ML_CRC:08X})')
        return rom
    rom = to_z64(open(args.rom, 'rb').read())
    if zlib.crc32(rom) != OOT10_CRC:
        fail(f'{args.rom} is not Ocarina of Time USA 1.0 (CRC32 {zlib.crc32(rom):08X}, want {OOT10_CRC:08X}); '
             'Kaze\'s patch needs that exact ROM')
    sys.path.insert(0, PIPE_SRC)
    from bps_apply import apply_bps
    patch = open(args.patch, 'rb').read()
    if patch[:4] != b'BPS1':
        fail(f'{args.patch} is not a BPS patch')
    rom = apply_bps(rom, patch)
    if zlib.crc32(rom) != ML_CRC:
        fail(f'{args.patch} does not make The Missing Link v2.0: use tml_z64_2_0_big_endian.bps from Kaze\'s page')
    return rom


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


def build(args):
    oot = os.path.abspath(args.oot_o2r)
    if not os.path.exists(oot):
        fail(f'{oot} not found: start MissingShip once so it makes oot.o2r from your ROM')
    out = os.path.abspath(args.out)
    work = tempfile.mkdtemp(prefix='missingship-build-')
    try:
        shutil.copytree(PIPE_SRC, os.path.join(work, 'pipeline'), ignore=SKIP)
        os.makedirs(os.path.join(work, 'mod'))
        rom_path = os.path.join(work, 'tml_2_0.z64')
        with open(rom_path, 'wb') as f:
            f.write(ml_rom(args))
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
    src = p.add_mutually_exclusive_group(required=True)
    src.add_argument('--rom', help='your Ocarina of Time USA 1.0 ROM (with --patch)')
    src.add_argument('--ml-rom', help='your The Missing Link v2.0 ROM (already patched)')
    p.add_argument('--patch', help="Kaze's tml_z64_2_0_big_endian.bps")
    p.add_argument('--oot-o2r', required=True, help='the oot.o2r Ship of Harkinian made from your ROM')
    p.add_argument('--out', required=True, help='where to write MissingLink_Scenes.o2r')
    p.add_argument('--verbose', action='store_true', help='show every pipeline step')
    p.add_argument('--keep', action='store_true', help='keep the temporary work folder')
    args = p.parse_args()
    if args.rom and not args.patch:
        p.error('--rom needs --patch')
    build(args)


if __name__ == '__main__':
    main()
