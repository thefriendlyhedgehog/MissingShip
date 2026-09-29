"""Freeze the mod builder (pipeline/build_mod.py) for this OS, with no Python needed to run it (MS-154).

    python3 tools/freeze_builder.py [--zapd <ZAPD binary>] [--out <folder>]

Needs PyInstaller (pip install pyinstaller). Writes <out>/MissingShipBuilder/ (default dist/builder/): the program,
plus the pipeline's scripts and data, the SoH files the pipeline reads, ZAPD, and version.txt (the commit it was frozen
from; the game rebuilds the mod when it changes). For SoH: Unbound (build_mod.py --unbound) it also carries the layer
tools, the fork's and Unbound's files they read, and unbound.txt (the Unbound commit those came from). No ROM or game data goes in: the builder makes everything from the
player's own files. Run it on each OS; PyInstaller can't cross-compile.
"""
import argparse, ast, os, shutil, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PIPE = os.path.join(ROOT, 'pipeline')
sys.path.insert(0, PIPE)

# SoH's own files the pipeline reads (paths under engine/soh): scene and entrance tables, one source file
# (ml_entrance.py reads z_demo.c's cutscene table), and the object/texture XMLs the inject steps look offsets up in.
SOH_FILES = ['include/tables/scene_table.h', 'include/tables/entrance_table.h', 'src/code/z_demo.c',
             'assets/xml/GC_MQ_PAL_F/objects', 'assets/xml/GC_MQ_PAL_F/textures',
             # the fork's tables and text the SoH: Unbound layer compares with Unbound's (tools/unbound_ml_*.py)
             'assets/ml_assets.h', 'soh/Enhancements/MlScrubShopText.cpp', 'soh/Enhancements/TimeDisplay/TimeDisplay.cpp',
             'soh/Enhancements/gameplaystats.cpp', 'soh/Enhancements/timesplits/TimeSplits.cpp', 'soh/util.cpp',
             'src/code/ml_entrance_flags.c', 'src/code/ml_entrance_map.c', 'src/code/ml_titlecard_banner.inc',
             'src/code/z_draw.c', 'src/code/z_inventory.c', 'src/code/z_map_data.c', 'src/code/z_parameter.c',
             'src/code/z_scene_table.c', 'src/code/z_sram.c', 'src/overlays/actors/ovl_Door_Ana/z_door_ana.c',
             'src/overlays/actors/ovl_En_GirlA/z_en_girla.c', 'src/overlays/misc/ovl_kaleido_scope',
             'src/overlays/misc/ovl_map_mark_data/z_map_mark_data.c']
# SoH: Unbound's own files the layer compares against (paths under its checkout; the object headers are globbed:
# unbound_ml_getitem.py looks display lists up in them). They must match the Unbound the players run.
UNBOUND_FILES = ['soh/assets/textures/map_i_static/map_i_static.h', 'soh/include/tables/actor_table.h',
                 'soh/include/tables/scene_table.h', 'soh/include/z64item.h', 'soh/include/z64scene.h',
                 'soh/soh/Enhancements/gameplaystats.cpp', 'soh/soh/util.cpp', 'soh/src/code/z_demo.c',
                 'soh/src/code/z_draw.c', 'soh/src/code/z_inventory.c', 'soh/src/code/z_map_data.c',
                 'soh/src/code/z_map_exp.c', 'soh/src/code/z_parameter.c', 'soh/src/code/z_sram.c',
                 'soh/src/overlays/actors/ovl_Door_Ana/z_door_ana.c', 'soh/src/overlays/actors/ovl_En_GirlA',
                 'soh/src/overlays/misc/ovl_kaleido_scope', 'soh/src/overlays/misc/ovl_map_mark_data/z_map_mark_data.c']
UNBOUND_GLOBS = ['soh/assets/objects/*/*.h']
# The SoH: Unbound layer (pipeline/build_mod.py --unbound)
TOOLS = ['tools/unbound_layer.py', 'tools/unbound_ml_getitem.py', 'tools/unbound_ml_items.py',
         'tools/unbound_ml_maps.py', 'tools/unbound_ml_pause.py', 'tools/unbound_ml_shops.py', 'tools/unbound_ml_ui.py',
         'tools/unbound_ml_world.py', 'tools/deltapack.py']
# Pipeline files that are development-only or outputs; everything else tracked under pipeline/ ships.
DEV_ONLY = ('gen_entrance_flags.py', 'gen_entrance_map.py', 'gen_scene_config.py', 'gen_textures.py',
            'ms_cycle.sh')


def pipeline_files():
    out = subprocess.run(['git', 'ls-files', 'pipeline'], cwd=ROOT, check=True, capture_output=True,
                         text=True).stdout.split()
    return [f for f in out if os.path.basename(f) not in DEV_ONLY]


def stdlib_imports(files):
    """Top-level standard-library modules the pipeline scripts import (PyInstaller only sees build_mod.py's)."""
    local = {os.path.splitext(os.path.basename(f))[0] for f in files if f.endswith('.py')}
    mods = set()
    for f in files:
        if not f.endswith('.py'):
            continue
        tree = ast.parse(open(os.path.join(ROOT, f), encoding='utf-8').read())
        for node in ast.walk(tree):
            if isinstance(node, ast.Import):
                mods.update(a.name.split('.')[0] for a in node.names)
            elif isinstance(node, ast.ImportFrom) and node.module and node.level == 0:
                mods.add(node.module.split('.')[0])
    return sorted(m for m in mods - local if m in sys.stdlib_module_names)


def main():
    import mspaths
    ap = argparse.ArgumentParser()
    ap.add_argument('--zapd', default=mspaths.ZAPD, help='the ZAPD binary to bundle (the engine build\'s)')
    ap.add_argument('--out', default=os.path.join(ROOT, 'dist', 'builder'))
    ap.add_argument('--soh', default=mspaths.SOH, help='engine/soh, for the SoH files')
    ap.add_argument('--unbound-src', default=os.path.join(os.path.dirname(ROOT), 'Unbound'),
                    help='the SoH: Unbound checkout the players\' Unbound was built from')
    args = ap.parse_args()
    # PyInstaller resolves relative paths against its spec folder (--out), not the working directory.
    args.zapd, args.out, args.soh, args.unbound_src = (os.path.abspath(a) for a in (args.zapd, args.out, args.soh,
                                                                                    args.unbound_src))
    if not os.path.isfile(args.zapd):
        sys.exit(f'ZAPD not found: {args.zapd}')

    stage = os.path.join(args.out, 'stage')
    shutil.rmtree(args.out, ignore_errors=True)
    files = pipeline_files() + TOOLS
    for f in files:
        dest = os.path.join(stage, f)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        shutil.copy2(os.path.join(ROOT, f), dest)
    for f in SOH_FILES:
        src, dest = os.path.join(args.soh, f), os.path.join(stage, 'soh', f)
        if os.path.isdir(src):
            shutil.copytree(src, dest)
        else:
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copy2(src, dest)

    import glob
    unbound = UNBOUND_FILES + sorted(os.path.relpath(f, args.unbound_src) for g in UNBOUND_GLOBS
                                     for f in glob.glob(os.path.join(args.unbound_src, g)))
    for f in unbound:
        src, dest = os.path.join(args.unbound_src, f), os.path.join(stage, 'unbound', f)
        if os.path.isdir(src):
            shutil.copytree(src, dest)
        else:
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copy2(src, dest)

    sep = os.pathsep
    cmd = [sys.executable, '-m', 'PyInstaller', '--noconfirm', '--onedir', '--console', '--name', 'MissingShipBuilder',
           '--distpath', args.out, '--workpath', os.path.join(args.out, 'work'), '--specpath', args.out,
           '--add-data', f'{os.path.join(stage, "pipeline")}{sep}pipeline',
           '--add-data', f'{os.path.join(stage, "soh")}{sep}soh',
           '--add-data', f'{os.path.join(stage, "tools")}{sep}tools',
           '--add-data', f'{os.path.join(stage, "unbound")}{sep}unbound',
           '--add-binary', f'{args.zapd}{sep}.']
    # macOS: Homebrew's SDL2 (which ZAPD links) is sdl2-compat, which loads SDL3 at start-up from its own folder
    # and aborts without it. PyInstaller can't see that, so SDL3 goes in by hand, under the name it looks for.
    if sys.platform == 'darwin':
        links = subprocess.run(['otool', '-L', args.zapd], check=True, capture_output=True, text=True).stdout
        if 'sdl2-compat' in links:
            sdl3 = os.path.join(stage, 'libSDL3.dylib')
            shutil.copy2(os.path.realpath('/opt/homebrew/opt/sdl3/lib/libSDL3.0.dylib'), sdl3)
            cmd += ['--add-binary', f'{sdl3}{sep}.']
    # Windows: vcpkg copies ZAPD's DLLs next to ZAPD.exe; ship them all rather than rely on PyInstaller finding them.
    if os.name == 'nt':
        zdir = os.path.dirname(os.path.abspath(args.zapd))
        for dll in sorted(f for f in os.listdir(zdir) if f.lower().endswith('.dll')):
            cmd += ['--add-binary', f'{os.path.join(zdir, dll)}{sep}.']
    for m in stdlib_imports(files):
        cmd += ['--hidden-import', m]
    cmd.append(os.path.join(PIPE, 'build_mod.py'))
    subprocess.run(cmd, check=True)
    shutil.rmtree(stage)
    shutil.rmtree(os.path.join(args.out, 'work'))
    # The game rebuilds the mod when this changes (a new builder can make a different mod): the commit it came from.
    head = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, check=True, capture_output=True, text=True)
    dirty = subprocess.run(['git', 'status', '--porcelain', '--', 'pipeline'], cwd=ROOT, check=True,
                           capture_output=True, text=True).stdout.strip()
    with open(os.path.join(args.out, 'MissingShipBuilder', 'version.txt'), 'w') as f:
        f.write(head.stdout.strip() + ('-dirty' if dirty else '') + '\n')
    # The Unbound the bundled sources came from: the layer matches the Unbound built from this commit.
    ub = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=args.unbound_src, check=True, capture_output=True, text=True)
    with open(os.path.join(args.out, 'MissingShipBuilder', 'unbound.txt'), 'w') as f:
        f.write(ub.stdout.strip() + '\n')
    print('builder:', os.path.join(args.out, 'MissingShipBuilder'))


if __name__ == '__main__':
    main()
