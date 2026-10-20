"""Make the pack ML's code mod carries, from which its porter makes ML's layer in the game (ml_porter.c).

    python3 tools/porterpack.py --unbound <players' Unbound app> --oot-o2r <its oot.o2r of USA 1.0>
                                [--also-base <oot.o2r>]... [--out <layer.pack>]

The players' Unbound is skijer's PR #11 safe build, the one players run: it converts each oot.o2r as the player's game
does, and the pack's deltas copy from that conversion. Give --also-base for each other extractor's oot.o2r of USA 1.0
a player may have mounted (SoH 9.2.3's): the porter takes the first variant whose sources the player's game has. Not
USA 1.1 or 1.2: their scenes lack bytes ML's layer keeps from 1.0, which a variant for them would have to carry (the
nodata check fails), so the porter refuses an oot.o2r from another release instead. The ML ROM is mspaths.ML_ROM.

It builds ML's scene mod from the ML ROM (pipeline/build_mod.py), the stock layer and its pack from that
(tools/unbound_layer.py --stock --porter), checks that the porter can read every path the pack copies from and that
the pack, inside the code mod, passes tools/nodata.py, and writes it to the code mod's assets on ml-codemod-930
(mspaths.CODEMOD_SRC, mod-sdk/mods/missinglink/assets/missinglink/porter/layer.pack), to commit there.
"""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', 'pipeline'))
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
from mspaths import ROOT, ML_ROM, CODEMOD_SRC, UNBOUND_SRC
import argparse, shutil, subprocess, sys, tempfile, zipfile
import deltapack

PACK = 'missinglink/porter/layer.pack'
OUT = _os.path.join(CODEMOD_SRC, 'mod-sdk', 'mods', 'missinglink', 'assets', PACK)
GLOB = set('*?[\\')     # the porter reads mounted files by mask (ReadArchiveFiles) and refuses these in a path


def run(cmd):
    r = subprocess.run([sys.executable] + cmd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit((r.stdout + r.stderr).strip() + f'\nporterpack: {_os.path.basename(cmd[0])} failed')
    return r.stdout


def source_paths(pack):
    """Every path the pack's deltas copy from, in any variant."""
    paths = set()
    with zipfile.ZipFile(pack) as z:
        for info in z.infolist():
            rec = z.read(info)
            if rec.startswith(deltapack.MAGIC):
                paths.update(p for v in deltapack.parse(rec) for ps, _ in v[2] for p in ps)
    return paths


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--unbound', required=True, help="the players' Unbound: its macOS app, or its binary")
    ap.add_argument('--oot-o2r', required=True, help='the oot.o2r that Unbound made from OoT USA 1.0')
    ap.add_argument('--also-base', action='append', default=[], metavar='OOT_O2R')
    ap.add_argument('--out', default=OUT)
    args = ap.parse_args()
    tools = _os.path.join(ROOT, 'tools')
    work = tempfile.mkdtemp(prefix='porterpack-')
    try:
        scenes = _os.path.join(work, 'MissingLink_Scenes.o2r')
        run([_os.path.join(ROOT, 'pipeline', 'build_mod.py'), ML_ROM, '--oot-o2r', _os.path.abspath(args.oot_o2r),
             '--out', scenes])
        layer, pack = _os.path.join(work, 'MissingLink_Unbound.o2r'), _os.path.join(work, 'layer.pack')
        print(run([_os.path.join(tools, 'unbound_layer.py'), '--unbound', args.unbound, '--unbound-src', UNBOUND_SRC,
                   '--oot-o2r', args.oot_o2r, '--mod', scenes, '--out', layer, '--stock', '--porter', pack] +
                  [x for other in args.also_base for x in ('--also-base', other)]).splitlines()[0])
        bad = sorted(p for p in source_paths(pack) if GLOB & set(p))
        if bad:
            sys.exit(f'porterpack: the porter can\'t read {len(bad)} source path(s), e.g. {bad[0]}')
        mod = _os.path.join(work, 'missinglink.o2r')    # the pack as the code mod holds it, for nodata's allowance
        with zipfile.ZipFile(mod, 'w', zipfile.ZIP_DEFLATED) as z:
            z.write(pack, PACK)
        r = subprocess.run([sys.executable, _os.path.join(tools, 'nodata.py'), mod], capture_output=True, text=True)
        print(r.stdout.strip())
        if r.returncode != 0:
            shutil.copyfile(pack, args.out + '.rejected')
            sys.exit(f'porterpack: the pack carries game data (kept as {args.out}.rejected)')
        _os.makedirs(_os.path.dirname(_os.path.abspath(args.out)), exist_ok=True)
        shutil.copyfile(pack, args.out)
        print(f'{args.out}: {_os.path.getsize(args.out) / 1e6:.1f} MB')
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    main()
