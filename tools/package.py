"""Package The Missing Link for SoH: Unbound: the builder for each OS, and the testing routes.

    python3 tools/package.py unbound-macos # the builder alone, on this Mac
    python3 tools/package.py unbound-ci <run id or artifact dir>   # the same for Windows and Linux, from the builders
                                           # a generate-builds run on the engine/ fork froze (missingship-builder-<os>)
    python3 tools/package.py unbound-pack <MissingLink_Unbound.o2r>   # the pack players drop into mods/ (any OS)
    python3 tools/package.py unbound-app-macos <MissingLink_Unbound.o2r> [<soh.app or Unbound .dmg>]
                                           # SoH: Unbound with the pack inside, as a DMG players open and play

For macOS, freeze the builder first (tools/freeze_builder.py, into dist/builder).

No package holds game data (MS-156): unbound-macos and unbound-ci check theirs with tools/nodata.py when the ROMs it
reads are here, and remove a package that fails.
"""
import io, os, plistlib, shutil, subprocess, sys, tarfile, zipfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'pipeline'))
import mspaths  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(ROOT, 'dist')
MAC_BUILDER = os.path.join(DIST, 'builder', 'MissingShipBuilder')


def fork():
    """owner/name of the engine fork on GitHub (its generate-builds runs), from the engine/ checkout's origin."""
    url = subprocess.run(['git', '-C', mspaths.SHIPWRIGHT, 'remote', 'get-url', 'origin'], check=True,
                         capture_output=True, text=True).stdout.strip()
    return url.split('github.com', 1)[1].lstrip(':/').removesuffix('.git')


def artifacts(arg):
    if os.path.isdir(arg):
        return arg
    out = os.path.join(DIST, f'run-{arg}')
    if not os.path.isdir(out):
        subprocess.run(['gh', 'run', 'download', arg, '--repo', fork(), '--dir', out], check=True)
    return out


def files_under(top):
    for base, _, names in os.walk(top):
        for name in names:
            path = os.path.join(base, name)
            yield path, os.path.relpath(path, top)


UNBOUND_README = """The Missing Link for SoH: Unbound (macOS)

This builds The Missing Link as a mod for SoH: Unbound from your own files. Nothing from the games is included.

You need
  - SoH: Unbound on SoH 9.3.0 or later with skijer's code mods (NEI Unbound 2.0), started once with your Ocarina of
    Time USA 1.0 (NTSC 1.0) ROM, so it has made oot.o2r. Until skijer publishes a release with it, that is his
    unbound-modapi build: https://nightly.link/skijer/Shipwright/actions/runs/38509788047/soh-mac.zip (until 2027-01-12).
  - Kaze's The Missing Link v2.0 patch from https://www.romhacking.net/hacks/5334/ (the .zip as downloaded is fine)
    and that same OoT ROM; or a Missing Link v2.0 ROM you already patched.

Build it: double-click "Build The Missing Link.app", choose your Unbound app, your ROM and the patch, and click Build
(about a minute). If macOS says the app can't be opened, right-click it, choose Open, then Open again.

It writes mods/MissingLink_Unbound.o2r and mods/missinglink.o2r, The Missing Link's behaviour as a code mod (compiled
code only, for Windows, Linux and macOS), into Unbound's folder (~/Library/Application Support/com.shipofharkinian.soh).
Start Unbound and The Missing Link is loaded. Build it again after updating Unbound or this package. The builder refuses
an Unbound on an SoH older than 9.3.0: the code mod calls 9.3.0's functions. The code mod is not signed yet: a build of
Unbound that loads only signed mods refuses it, and one that asks shows its name and content hash the first time;
answer "Always load this version".

The same from Terminal, in this folder:
  MissingShipBuilder/MissingShipBuilder --unbound "/Applications/<your Unbound>.app" --stock <OoT ROM> <patch .zip>
or, with a Missing Link ROM, give it in place of <OoT ROM> <patch .zip>. --oot-o2r and --out choose other places.

MissingShipBuilder/unbound.txt names the Unbound build this package was made for, and MissingShipBuilder/codemod.txt
the code mod's commit.
"""


UNBOUND_README_PORTABLE = """The Missing Link for SoH: Unbound ({os})

This builds The Missing Link as a mod for SoH: Unbound from your own files. Nothing from the games is included.

You need
  - SoH: Unbound on SoH 9.3.0 or later with skijer's code mods (NEI Unbound 2.0), started once with your Ocarina of
    Time USA 1.0 (NTSC 1.0) ROM, so it has made oot.o2r. Unbound keeps oot.o2r and mods/ in the folder it starts
    from{home_note}. Until skijer publishes a release with it, that is his unbound-modapi build:
    https://nightly.link/skijer/Shipwright/actions/runs/38509788047/{skijer_artifact}.zip (until 2027-01-12).
  - Kaze's The Missing Link v2.0 patch from https://www.romhacking.net/hacks/5334/ (the .zip as downloaded is fine)
    and that same OoT ROM; or a Missing Link v2.0 ROM you already patched.

Build it: {easy}.
Then choose the folder Unbound is in, your ROM and the patch, and click Build (about a minute).

It writes mods{sep}MissingLink_Unbound.o2r and mods{sep}missinglink.o2r, The Missing Link's behaviour as a code mod
(compiled code only, for Windows, Linux and macOS). Start Unbound and The Missing Link is loaded. Build it again after
updating Unbound or this package. The builder refuses an Unbound on an SoH older than 9.3.0: the code mod calls 9.3.0's
functions. The code mod is not signed yet: a build of Unbound that loads only signed mods refuses it, and one that asks
shows its name and content hash the first time; answer "Always load this version".
{prepare}
The same from {shell}, in this folder:
  {builder} --unbound {game} --stock --oot-o2r "{folder}{sep}oot.o2r" --out "{folder}{sep}mods{sep}MissingLink_Unbound.o2r" <OoT ROM> <patch .zip>
or, with a Missing Link ROM, give it in place of <OoT ROM> <patch .zip>.

MissingShipBuilder{sep}unbound.txt names the Unbound build this package was made for, and
MissingShipBuilder{sep}codemod.txt the code mod's commit.
"""

UNBOUND_PORTABLE = {
    'windows': dict(os='Windows', home_note='', skijer_artifact='soh-windows', prepare='', shell='Command Prompt', sep='\\',
                    easy='double-click "Build The Missing Link.cmd"',
                    builder='MissingShipBuilder\\MissingShipBuilder.exe', game='"<Unbound folder>\\soh.exe"',
                    folder='<Unbound folder>'),
    'linux': dict(os='Linux', home_note=' (or SHIP_HOME, when set)', skijer_artifact='soh-linux', sep='/',
                  easy='run "Build The Missing Link.sh" (double-click it, or ./"Build The Missing Link.sh" in a '
                       'terminal)',
                  prepare='\nFrom a terminal, first unpack Unbound\'s AppImage once, in its folder (the builder runs the '
                          'game inside it;\nthe window does this itself):\n  ./soh.appimage --appimage-extract\n',
                  shell='a terminal', builder='MissingShipBuilder/MissingShipBuilder',
                  game='"<Unbound folder>/squashfs-root/usr/bin/soh.elf"', folder='<Unbound folder>'),
}


UNBOUND_PACK_README = """The Missing Link for SoH: Unbound

The Missing Link v2.0 by Kaze Emanuar, as a mod for SoH: Unbound, on Windows, macOS and Linux. The mod holds only
The Missing Link's own work: everything it shares with Ocarina of Time is copied, as the game loads it, from the
files your SoH: Unbound made from your own ROM. Nothing from Nintendo's game is included.

You need
  - a SoH: Unbound that reads delta files and has The Missing Link's features: for now MissingShip's build of it
    (branch missinglink), until they are part of SoH: Unbound itself;
  - started once with your Ocarina of Time USA ROM (1.0, 1.1 or 1.2; not Master Quest), so that it has made
    oot.o2r.

Put MissingLink_Unbound.o2r in SoH: Unbound's mods folder and start the game:
  - macOS: ~/Library/Application Support/com.shipofharkinian.soh/mods
  - Windows and Linux: the mods folder beside the game (or in the folder SHIP_HOME names)

If The Missing Link is not loaded, the game says why as it starts, and its Mod Menu marks it "(not loaded)". Most
often your oot.o2r was made by an older Ship of Harkinian (before 9.1) or from a ROM other than USA 1.0, 1.1 or 1.2:
delete oot.o2r and oot-unbound.o2r and start the game, and it makes them again from your USA ROM.
"""


def check_unbound_pack(path):
    """A pack for players: deltas and ML's own files, never a game archive."""
    if not os.path.isfile(path):
        sys.exit(f'{path} not found (build it: pipeline/build_mod.py --unbound <app> --pack ...)')
    with zipfile.ZipFile(path) as z:
        names = z.namelist()
        if 'version' in names or 'deltas.json' not in names:
            sys.exit(f'{path} is not a pack (no deltas.json, or it has a game version file)')
        deltas = sum(z.read(n)[:4] == b'LUSD' for n in names)
    print(f'pack: {len(names)} files, {deltas} deltas')


def unbound_pack(pack):
    """The pack players drop into Unbound's mods/ folder, with how to (owner decision 2026-09-29: the pack route)."""
    out = os.path.join(DIST, 'MissingLink-Unbound.zip')
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        z.write(pack, 'MissingLink for Unbound/MissingLink_Unbound.o2r')
        z.writestr('MissingLink for Unbound/README.txt', UNBOUND_PACK_README)
    return out


UNBOUND_APP = 'The Missing Link (SoH Unbound).app'
# The app's own folder for its files, settings and saves (owner decision 2026-10-09). Sharing Ship of Harkinian's
# folder let each game move the other's randomizer saves aside (they keep only saves of their exact version), mixed
# ML and SoH saves in the same slots, and left the settings file in whichever format the newer game wrote. Unbound's
# Info.plist names the folder (SHIP_HOME in LSEnvironment); its own bundle identifier keeps macOS from confusing the
# two apps, and from launching ours with an Info.plist it cached before.
UNBOUND_APP_HOME = '~/Library/Application Support/The Missing Link'
UNBOUND_APP_ID = 'com.missingship.TheMissingLink'
# Unbound's own package (cd build-930 && cpack): the app with its libraries bundled, before the DMG is made.
UNBOUND_CPACK_APP = os.path.join(os.path.dirname(ROOT), 'Unbound', 'build-930', '_CPack_Packages', 'Darwin', 'Bundle')

UNBOUND_APP_README = """The Missing Link for SoH: Unbound (macOS)

The Missing Link v2.0 by Kaze Emanuar, playing in SoH: Unbound. Nothing from Nintendo's game is included: on first
launch the game makes its files from your own Ocarina of Time ROM, and The Missing Link copies what it shares with
Ocarina of Time from them.

1. Drag "{app}" to Applications.
2. Open it. macOS asks first, because the app is not notarized yet: if it says it cannot check the app, open
   System Settings > Privacy & Security, and click "Open Anyway" beside its name.
3. The first time, it asks for your Ocarina of Time ROM: a USA one (1.0, 1.1 or 1.2; not Master Quest). It takes a
   minute to make the game's files, then The Missing Link starts. If the game says The Missing Link was not loaded,
   the ROM is another release: delete oot.o2r and oot-unbound.o2r in the folder below, and open the game again with
   a USA ROM.

The game keeps its files, settings and saves in its own folder:
  {home}
Your Ship of Harkinian, its saves and its settings are not touched. Other mods go in the mods folder there. The Missing Link travels inside the app; it is
listed in the game's Mod Menu, where you can turn it off to play Ocarina of Time.

Runs on: {runs_on}.
"""


def macos_app_support(app):
    """The Macs an app bundle runs on: its CPU types, and the newest macOS any of its binaries asks for."""
    binaries = [os.path.join(app, 'Contents', 'MacOS', n) for n in os.listdir(os.path.join(app, 'Contents', 'MacOS'))]
    frameworks = os.path.join(app, 'Contents', 'Frameworks')
    if os.path.isdir(frameworks):
        binaries += [os.path.join(frameworks, n) for n in os.listdir(frameworks) if n.endswith('.dylib')]
    archs = subprocess.run(['lipo', '-archs', binaries[0]], check=True, capture_output=True, text=True).stdout.split()
    newest = (0,)
    for b in binaries:
        out = subprocess.run(['vtool', '-show-build', b], capture_output=True, text=True).stdout.split()
        for i, word in enumerate(out[:-1]):
            if word == 'minos':
                newest = max(newest, tuple(int(x) for x in out[i + 1].split('.')))
    cpus = ' and '.join({'arm64': 'Apple silicon', 'x86_64': 'Intel'}.get(a, a) for a in archs)
    return f'{cpus} Macs, macOS {".".join(map(str, newest))} or later'


def unbound_source_app(source):
    """Unbound's soh.app: the given app, the app inside the given DMG (mounted out of sight), or cpack's."""
    import glob, tempfile
    if source and source.endswith('.dmg'):
        mount = tempfile.mkdtemp(prefix='unbound-dmg-')
        subprocess.run(['hdiutil', 'attach', '-nobrowse', '-readonly', '-mountpoint', mount, source], check=True,
                       capture_output=True)
        return os.path.join(mount, 'soh.app'), mount
    if source:
        return source, None
    found = sorted(glob.glob(os.path.join(UNBOUND_CPACK_APP, '*', 'soh.app')))
    if not found:
        sys.exit('no Unbound app: give its soh.app or .dmg, or package it first (cd ../Unbound/build-930 && cpack)')
    return found[-1], None


def bundle_sdl3(app):
    """Homebrew's SDL2 is sdl2-compat, which loads SDL3 when the game starts (from beside itself first), so the
    bundling that follows link-time libraries leaves SDL3 out and the app stops with "Failed loading SDL3 library".
    A build with a real SDL2 (Unbound's CI, MacPorts) needs nothing."""
    frameworks = os.path.join(app, 'Contents', 'Frameworks')
    sdl2 = os.path.join(frameworks, 'libSDL2-2.0.0.dylib')
    if not os.path.isfile(sdl2) or b'libSDL3.dylib' not in open(sdl2, 'rb').read():
        return
    if os.path.exists(os.path.join(frameworks, 'libSDL3.dylib')):
        return
    brew = subprocess.run(['brew', '--prefix', 'sdl3'], capture_output=True, text=True).stdout.strip()
    sdl3 = os.path.realpath(os.path.join(brew, 'lib', 'libSDL3.dylib')) if brew else ''
    if not os.path.isfile(sdl3):
        sys.exit('the app\'s SDL2 is sdl2-compat, which needs SDL3: brew install sdl3')
    shutil.copyfile(sdl3, os.path.join(frameworks, 'libSDL3.dylib'))
    subprocess.run(['install_name_tool', '-id', '@loader_path/libSDL3.dylib', os.path.join(frameworks, 'libSDL3.dylib')],
                   check=True, capture_output=True)
    print('bundled SDL3 for sdl2-compat')


def unbound_app_macos(pack, source):
    """SoH: Unbound with the pack in the app's own mods folder (our Unbound branch reads Contents/Resources/mods), as a
    DMG with an Applications link: players open the app and play, with no mods folder to find."""
    import tempfile
    app, mount = unbound_source_app(source)
    stage = tempfile.mkdtemp(prefix='missinglink-app-')
    try:
        if not os.path.isfile(os.path.join(app, 'Contents', 'Resources', 'soh.o2r')):
            sys.exit(f'{app} is not a SoH: Unbound app')
        top = os.path.join(stage, 'The Missing Link')
        os.makedirs(top)
        dest = os.path.join(top, UNBOUND_APP)
        subprocess.run(['ditto', app, dest], check=True)
    finally:
        if mount:
            subprocess.run(['hdiutil', 'detach', mount], capture_output=True)
    bundle_sdl3(dest)
    mods = os.path.join(dest, 'Contents', 'Resources', 'mods')
    os.makedirs(mods, exist_ok=True)
    shutil.copyfile(pack, os.path.join(mods, 'MissingLink_Unbound.o2r'))
    plist_path = os.path.join(dest, 'Contents', 'Info.plist')
    with open(plist_path, 'rb') as f:
        plist = plistlib.load(f)
    if 'SHIP_HOME' not in plist.get('LSEnvironment', {}):
        sys.exit(f'{app}: its Info.plist does not set SHIP_HOME, so the app would share another folder')
    plist['LSEnvironment']['SHIP_HOME'] = UNBOUND_APP_HOME
    plist['CFBundleIdentifier'] = UNBOUND_APP_ID
    plist['CFBundleName'] = 'The Missing Link'
    with open(plist_path, 'wb') as f:
        plistlib.dump(plist, f)
    subprocess.run(['xattr', '-cr', dest], check=True)
    # Every file in Frameworks and MacOS is signed before the bundle, the data file SDL's controller list keeps in
    # MacOS too (codesign keeps its signature in extended attributes, which xattr -cr just removed).
    macos_dir = os.path.join(dest, 'Contents', 'MacOS')
    inner = list(files_under(os.path.join(dest, 'Contents', 'Frameworks')))
    inner += [(os.path.join(macos_dir, n), n) for n in os.listdir(macos_dir) if n != 'soh']
    for path, _ in inner:
        subprocess.run(['codesign', '--force', '--sign', '-', path], check=True, capture_output=True)
    subprocess.run(['codesign', '--force', '--sign', '-', dest], check=True, capture_output=True)
    subprocess.run(['codesign', '--verify', '--strict', dest], check=True)
    runs_on = macos_app_support(dest)
    with open(os.path.join(top, 'README.txt'), 'w') as f:
        f.write(UNBOUND_APP_README.format(app=UNBOUND_APP, runs_on=runs_on, home=UNBOUND_APP_HOME))
    os.symlink('/Applications', os.path.join(top, 'Applications'))
    out = os.path.join(DIST, 'MissingLink-Unbound-macos.dmg')
    # Made plain, then compressed: compressing while hdiutil fills the image from the folder came out three times as
    # large. LZMA (ULMO) is a third of zlib's size and opens on macOS 10.15 and later.
    plain = os.path.join(stage, 'plain.dmg')
    subprocess.run(['hdiutil', 'create', '-quiet', '-ov', '-volname', 'The Missing Link', '-srcfolder', top,
                    '-format', 'UDRW', plain], check=True)
    subprocess.run(['hdiutil', 'convert', '-quiet', '-ov', plain, '-format', 'ULMO', '-o', out], check=True)
    shutil.rmtree(stage)
    print(f'runs on {runs_on}')
    return out


# The builder's window (pipeline/builder_gui.py) from a double-click, beside the builder in each package
WINDOWS_LAUNCHER = """@echo off
start "" "%~dp0MissingShipBuilder\\MissingShipBuilder.exe" --gui
"""
LINUX_LAUNCHER = """#!/bin/sh
cd "$(dirname "$0")" && exec ./MissingShipBuilder/MissingShipBuilder --gui
"""
MAC_LAUNCHER = """#!/bin/sh
exec "$(cd "$(dirname "$0")/../../.." && pwd)/MissingShipBuilder/MissingShipBuilder" --gui
"""


def mac_launcher(top):
    """Build The Missing Link.app: a double-clickable app that opens the builder's window."""
    app = os.path.join(top, 'Build The Missing Link.app', 'Contents')
    os.makedirs(os.path.join(app, 'MacOS'))
    with open(os.path.join(app, 'MacOS', 'launcher'), 'w') as f:
        f.write(MAC_LAUNCHER)
    os.chmod(os.path.join(app, 'MacOS', 'launcher'), 0o755)
    with open(os.path.join(app, 'Info.plist'), 'wb') as f:
        plistlib.dump({'CFBundleExecutable': 'launcher', 'CFBundleIdentifier': 'com.missingship.builder',
                       'CFBundleName': 'Build The Missing Link', 'CFBundlePackageType': 'APPL',
                       'CFBundleShortVersionString': '1.0', 'LSMinimumSystemVersion': '11.0',
                       'NSHighResolutionCapable': True}, f)


def unbound_macos():
    """The builder with its SoH: Unbound layer, and how to run it: ML as an ordinary Unbound mod (owner decision
    2026-09-29: a standard romhack tool, not a renamed game)."""
    out = os.path.join(DIST, 'MissingLink-Unbound-macos.zip')
    import tempfile
    stage = tempfile.mkdtemp(prefix='missinglink-unbound-')
    top = os.path.join(stage, 'MissingLink for Unbound')
    shutil.copytree(MAC_BUILDER, os.path.join(top, 'MissingShipBuilder'), symlinks=True)
    with open(os.path.join(top, 'README.txt'), 'w') as f:
        f.write(UNBOUND_README)
    mac_launcher(top)
    subprocess.run(['xattr', '-cr', top], check=True)
    for path, _ in files_under(top):
        if not os.path.islink(path) and open(path, 'rb').read(4) in (b'\xcf\xfa\xed\xfe', b'\xca\xfe\xba\xbe'):
            subprocess.run(['codesign', '--force', '--sign', '-', path], check=True, capture_output=True)
    subprocess.run(['codesign', '--force', '--sign', '-', os.path.join(top, 'Build The Missing Link.app')], check=True,
                   capture_output=True)
    if os.path.exists(out):
        os.remove(out)
    subprocess.run(['ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', top, out], check=True)
    shutil.rmtree(stage)
    return out


def unbound_ci(arg):
    """The builder alone for Windows and Linux, like unbound-macos, from a generate-builds run's frozen builders."""
    if os.path.isdir(arg):
        art = arg
    else:
        art = os.path.join(DIST, f'run-{arg}-builders')
        if not os.path.isdir(art):
            subprocess.run(['gh', 'run', 'download', arg, '--repo', fork(), '--dir', art,
                            '--name', 'missingship-builder-windows', '--name', 'missingship-builder-linux'], check=True)
    made = []
    for osname in ('windows', 'linux'):
        builder = os.path.join(art, f'missingship-builder-{osname}')
        check_unbound_builder(builder)
        readme = UNBOUND_README_PORTABLE.format(**UNBOUND_PORTABLE[osname])
        top = 'MissingLink for Unbound'
        if osname == 'windows':
            out = os.path.join(DIST, 'MissingLink-Unbound-windows.zip')
            with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
                for path, rel in files_under(builder):
                    z.write(path, f'{top}/MissingShipBuilder/{rel}')
                z.writestr(f'{top}/README.txt', readme.replace('\n', '\r\n'))
                z.writestr(f'{top}/Build The Missing Link.cmd', WINDOWS_LAUNCHER.replace('\n', '\r\n'))
        else:
            out = os.path.join(DIST, 'MissingLink-Unbound-linux.tar.gz')
            with tarfile.open(out, 'w:gz') as t:
                def add(name, data, mode):
                    info = tarfile.TarInfo(f'{top}/{name}')
                    info.size, info.mode = len(data), mode
                    t.addfile(info, io.BytesIO(data))
                for path, rel in files_under(builder):  # artifacts drop executable bits: the builder and ZAPD get them
                    mode = 0o755 if rel in ('MissingShipBuilder', '_internal/ZAPD.out') else 0o644
                    add(f'MissingShipBuilder/{rel}', open(path, 'rb').read(), mode)
                add('README.txt', readme.encode(), 0o644)
                add('Build The Missing Link.sh', LINUX_LAUNCHER.encode(), 0o755)
        made.append(out)
    return made


def check_builder(path):
    """The frozen mod builder for a package: present, and made from this checkout's commit."""
    version = os.path.join(path, 'version.txt')
    if not os.path.isfile(version):
        sys.exit(f'no mod builder at {path} (macOS: python3 tools/freeze_builder.py; others: the CI run\'s '
                 'missingship-builder-<os> artifact)')
    head = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, check=True, capture_output=True, text=True)
    made = open(version).read().strip()
    if made.removesuffix('-dirty') != head.stdout.strip():
        print(f'warning: the builder in {path} was frozen from {made}, not HEAD')


def check_unbound_builder(path):
    """The builder also carries the layer's Unbound files, from a known Unbound commit."""
    check_builder(path)
    ub = os.path.join(path, 'unbound.txt')
    if not os.path.isfile(ub) or not os.path.isdir(os.path.join(path, '_internal', 'unbound')):
        sys.exit(f'the builder at {path} has no SoH: Unbound files: freeze it again (tools/freeze_builder.py)')
    print(f'builder made for Unbound {open(ub).read().strip()[:10]}')
    cm = os.path.join(path, 'codemod.txt')
    if not os.path.isfile(cm) or not os.path.isfile(os.path.join(path, '_internal', 'codemod', 'missinglink.o2r')):
        sys.exit(f'the builder at {path} has no code mod for --stock: make it (tools/codemod.py), then freeze again')
    print(f'builder carries the code mod from {open(cm).read().strip()}')


def check_no_game_data(paths):
    """MS-156: run tools/nodata.py on the packages; remove them all and stop if any carries game data."""
    sys.path.insert(0, os.path.join(ROOT, 'pipeline'))
    from mspaths import ML_ROM, OOT10_ROM
    if not (os.path.exists(OOT10_ROM) and os.path.exists(ML_ROM)):
        print(f'NOT CHECKED for game data: tools/nodata.py needs OoT USA 1.0 at {OOT10_ROM} and ML v2.0 at {ML_ROM} '
              '(MS_OOT10_ROM, MS_ML_ROM); check before releasing')
        return
    if subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'nodata.py'), *paths]).returncode:
        for path in paths:
            os.remove(path)
        sys.exit('package.py: game data in a package (above); removed what was made')


def main():
    if len(sys.argv) in (3, 4) and sys.argv[1] == 'unbound-app-macos':
        os.makedirs(DIST, exist_ok=True)
        check_unbound_pack(sys.argv[2])
        path = unbound_app_macos(sys.argv[2], sys.argv[3] if len(sys.argv) == 4 else None)
        print(f'{os.path.relpath(path, ROOT)}  {os.path.getsize(path) / 1e6:.1f} MB')
        return
    if len(sys.argv) == 3 and sys.argv[1] == 'unbound-pack':
        os.makedirs(DIST, exist_ok=True)
        check_unbound_pack(sys.argv[2])
        path = unbound_pack(sys.argv[2])
        print(f'{os.path.relpath(path, ROOT)}  {os.path.getsize(path) / 1e6:.1f} MB')
        return
    os.makedirs(DIST, exist_ok=True)
    if len(sys.argv) == 3 and sys.argv[1] == 'unbound-ci':
        made = unbound_ci(sys.argv[2])
        check_no_game_data(made)
        for path in made:
            print(f'{os.path.relpath(path, ROOT)}  {os.path.getsize(path) / 1e6:.1f} MB')
        return
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    if sys.argv[1] == 'unbound-macos':
        check_unbound_builder(MAC_BUILDER)
        path = unbound_macos()
        check_no_game_data([path])
        print(f'{os.path.relpath(path, ROOT)}  {os.path.getsize(path) / 1e6:.1f} MB')
        return
    sys.exit(__doc__)


if __name__ == '__main__':
    main()
