"""Freeze the mod builder (pipeline/build_mod.py) for this OS, with no Python needed to run it (MS-154).

    python3 tools/freeze_builder.py [--zapd <ZAPD binary>] [--out <folder>]

Needs PyInstaller (pip install pyinstaller). Writes <out>/MissingShipBuilder/ (default dist/builder/): the program,
plus the pipeline's scripts and data, the SoH files the pipeline reads, and ZAPD. No ROM or game data goes in: the
builder makes everything from the player's own files. Run it on each OS; PyInstaller can't cross-compile.
"""
import argparse, ast, os, shutil, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PIPE = os.path.join(ROOT, 'pipeline')
sys.path.insert(0, PIPE)

# SoH's own files the pipeline reads (paths under engine/soh): scene and entrance tables, one source file
# (ml_entrance.py reads z_demo.c's cutscene table), and the object/texture XMLs the inject steps look offsets up in.
SOH_FILES = ['include/tables/scene_table.h', 'include/tables/entrance_table.h', 'src/code/z_demo.c',
             'assets/xml/GC_MQ_PAL_F/objects', 'assets/xml/GC_MQ_PAL_F/textures']
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
    args = ap.parse_args()
    # PyInstaller resolves relative paths against its spec folder (--out), not the working directory.
    args.zapd, args.out, args.soh = (os.path.abspath(a) for a in (args.zapd, args.out, args.soh))
    if not os.path.isfile(args.zapd):
        sys.exit(f'ZAPD not found: {args.zapd}')

    stage = os.path.join(args.out, 'stage')
    shutil.rmtree(args.out, ignore_errors=True)
    files = pipeline_files()
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

    sep = os.pathsep
    cmd = [sys.executable, '-m', 'PyInstaller', '--noconfirm', '--onedir', '--console', '--name', 'MissingShipBuilder',
           '--distpath', args.out, '--workpath', os.path.join(args.out, 'work'), '--specpath', args.out,
           '--add-data', f'{os.path.join(stage, "pipeline")}{sep}pipeline',
           '--add-data', f'{os.path.join(stage, "soh")}{sep}soh',
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
    print('builder:', os.path.join(args.out, 'MissingShipBuilder'))


if __name__ == '__main__':
    main()
