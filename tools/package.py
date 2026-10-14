"""Package MissingShip for Windows and Linux from a generate-builds run on the engine fork (MS-080, MS-081).

    python3 tools/package.py <run id>      # downloads the run's artifacts with gh
    python3 tools/package.py <artifact dir>
    python3 tools/package.py macos         # the local macOS build (engine/build-ml), on this Mac
    python3 tools/package.py unbound-macos # The Missing Link for SoH: Unbound: the builder alone, on this Mac
    python3 tools/package.py unbound-pack <MissingLink_Unbound.o2r>   # the pack players drop into mods/ (any OS)
    python3 tools/package.py unbound-app-macos <MissingLink_Unbound.o2r> [<soh.app or Unbound .dmg>]
                                           # SoH: Unbound with the pack inside, as a DMG players open and play

For macOS, build the engine first (cmake --build engine/build-ml --target soh) at the commit being released.

CI builds the engine (soh.o2r, the Windows exe, the Linux AppImage) and freezes the mod builder for each OS. This adds
a portable settings profile and writes dist/MissingShip-windows.zip and dist/MissingShip-linux.tar.gz. For macOS, freeze
the builder first (tools/freeze_builder.py, into dist/builder).

No package holds game data (MS-156): on first launch SoH makes oot.o2r from the player's own OoT ROM, then the game runs
the bundled builder (builder/) on Kaze's patch to make mods/MissingLink_Scenes.o2r.

Where the game looks for its files: on Windows, the folder it is started from; on Linux, SHIP_HOME, which the
MissingShip.sh launcher points at its own folder.
"""
import io, json, os, plistlib, shutil, subprocess, sys, tarfile, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(ROOT, 'dist')
FORK = 'thefriendlyhedgehog/MissingShip-Shipwright'
MAC_BUILDER = os.path.join(DIST, 'builder', 'MissingShipBuilder')
DEV_APP = os.path.join(ROOT, 'MissingShip.app')
MAC_BINARY = os.path.join(ROOT, 'engine', 'build-ml', 'soh', 'soh-macos')
MAC_HOME = '~/Library/Application Support/MissingShip'
VERSION = '0.1-beta'

README = """MissingShip -- The Missing Link on the Ship of Harkinian engine ({os})

First launch
  {launch}
{first_launch}
Files
  soh.o2r                       Ship of Harkinian's own assets
  builder/                      Builds The Missing Link from your files (the game runs it)
  oot.o2r                       Made from your OoT ROM on first launch
  mods/MissingLink_Scenes.o2r   The Missing Link, built on first launch
  MissingLink/                  Your Missing Link ROM, kept so updates can rebuild the mod without asking
  shipofharkinian.json          Settings (created or updated by the game)
  Save/                         Save files (created by the game)
{extra}"""

FIRST_LAUNCH = """  You need your own Ocarina of Time USA 1.0 ROM (NTSC 1.0) and Kaze's The Missing Link v2.0 patch, from
  https://www.romhacking.net/hacks/5334/ (the .zip as downloaded is fine). A Missing Link v2.0 ROM you already
  patched works instead of the two.
  1. The game asks for your OoT ROM and makes oot.o2r from it (once).
  2. It then asks for Kaze's patch and builds The Missing Link (about a minute). Nothing to install by hand.
"""

LINUX_EXTRA = """
Steam Controller
  Outside Steam it acts as a mouse and keyboard. Add MissingShip.sh to Steam (Games > Add a Non-Steam Game) and
  launch it from there, so Steam Input presents it as a gamepad.
"""

LINUX_LAUNCHER = """#!/bin/sh
# MissingShip: keep settings, saves and archives next to this script (SoH reads SHIP_HOME on Linux).
HERE="$(cd "$(dirname "$0")" && pwd)"
export SHIP_HOME="$HERE"
cd "$HERE"
exec "$HERE/MissingShip.AppImage" "$@"
"""


def portable_profile():
    """The macOS default profile without its Mac-only window block (Metal renderer, window position) and the
    developer warp point, so each OS picks its own renderer and window placement."""
    with open(os.path.join(ROOT, 'profile', 'shipofharkinian.json')) as f:
        profile = json.load(f)
    profile.pop('Window', None)
    profile.pop('WarpPoints', None)
    return json.dumps(profile, indent=4).encode()


def artifacts(arg):
    if os.path.isdir(arg):
        return arg
    out = os.path.join(DIST, f'run-{arg}')
    if not os.path.isdir(out):
        subprocess.run(['gh', 'run', 'download', arg, '--repo', FORK, '--dir', out], check=True)
    return out


def files_under(top):
    for base, _, names in os.walk(top):
        for name in names:
            path = os.path.join(base, name)
            yield path, os.path.relpath(path, top)


def windows(src, builder, soh_o2r, profile):
    out = os.path.join(DIST, 'MissingShip-windows.zip')
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        for path, rel in files_under(src):
            name = 'MissingShip.exe' if rel == 'soh.exe' else rel
            if name.lower() in ('readme.txt', 'readme.md'):
                continue
            z.write(path, f'MissingShip/{name}')
        if not any(n.endswith('/soh.o2r') for n in z.namelist()):
            z.write(soh_o2r, 'MissingShip/soh.o2r')
        for path, rel in files_under(builder):
            z.write(path, f'MissingShip/builder/{rel}')
        z.writestr('MissingShip/shipofharkinian.json', profile)
        z.writestr('MissingShip/README.txt', README.format(os='Windows', launch='Run MissingShip.exe.',
                                                           first_launch=FIRST_LAUNCH, extra=''))
    return out


def linux(src, builder, soh_o2r, profile):
    out = os.path.join(DIST, 'MissingShip-linux.tar.gz')

    def add(t, name, data=None, path=None, mode=0o644):
        info = tarfile.TarInfo(f'MissingShip/{name}')
        data = data if data is not None else open(path, 'rb').read()
        info.size, info.mode = len(data), mode
        t.addfile(info, io.BytesIO(data))

    with tarfile.open(out, 'w:gz') as t:
        add(t, 'MissingShip.AppImage', path=os.path.join(src, 'soh.appimage'), mode=0o755)
        add(t, 'MissingShip.sh', LINUX_LAUNCHER.encode(), mode=0o755)
        add(t, 'soh.o2r', path=soh_o2r)
        for path, rel in files_under(builder):       # keeps the builder's executable bits
            mode = 0o755 if os.access(path, os.X_OK) or rel in ('MissingShipBuilder', '_internal/ZAPD.out') else 0o644
            add(t, f'builder/{rel}', path=path, mode=mode)
        add(t, 'shipofharkinian.json', profile)
        add(t, 'README.txt', README.format(os='Linux', launch='Run ./MissingShip.sh (not the AppImage directly).',
                                           first_launch=FIRST_LAUNCH, extra=LINUX_EXTRA).encode())
    return out


MAC_README = """MissingShip -- The Missing Link on the Ship of Harkinian engine (macOS, Apple Silicon)

First launch
  1. Move MissingShip.app to Applications (or anywhere you like).
  2. macOS blocks apps from unidentified developers the first time: open MissingShip once, then go to
     System Settings > Privacy & Security and click "Open Anyway" next to MissingShip.
  3. MissingShip asks for your own Ocarina of Time USA 1.0 ROM (NTSC 1.0) and makes oot.o2r from it (once).
  4. It then asks for Kaze's The Missing Link v2.0 patch, from https://www.romhacking.net/hacks/5334/ (the .zip as
     downloaded is fine), and builds The Missing Link (about a minute). A Missing Link v2.0 ROM you already patched
     works instead of the two.

Where your files are
  ~/Library/Application Support/MissingShip   settings, saves, logs, oot.o2r and the built mod
  (Settings > "Open App Files Folder" in the game's menu opens it.)

Needs an Apple Silicon Mac (M1 or later) and macOS 11 or later.
"""


def _deps(path):
    out = subprocess.run(['otool', '-L', path], check=True, capture_output=True, text=True).stdout.splitlines()[1:]
    return [l.strip().split(' (')[0] for l in out]


def _bundle_dylibs(binary, frameworks):
    """Copy every non-system dylib the binary needs (recursively) into Contents/Frameworks and point all references
    there, so the app runs on a Mac without Homebrew."""
    def external(ref):
        return not (ref.startswith('/System/') or ref.startswith('/usr/lib/') or ref.startswith('@executable_path'))

    def resolve(ref, owner):
        if ref.startswith('@loader_path/'):
            return os.path.realpath(os.path.join(os.path.dirname(owner), ref[len('@loader_path/'):]))
        if ref.startswith('@rpath/'):
            for base in (os.path.dirname(owner), '/opt/homebrew/lib', '/usr/local/lib'):
                cand = os.path.join(base, ref[len('@rpath/'):])
                if os.path.exists(cand):
                    return os.path.realpath(cand)
            sys.exit(f'cannot resolve {ref} (needed by {owner})')
        return os.path.realpath(ref)

    os.makedirs(frameworks, exist_ok=True)
    todo, seen = [(binary, binary)], {}
    # Homebrew's SDL2 is sdl2-compat, which dlopens SDL3 at run time (otool can't see it); it looks for
    # @loader_path/libSDL3.dylib first, so SDL3 goes next to it.
    sdl3 = '/opt/homebrew/opt/sdl3/lib/libSDL3.0.dylib'
    if any('sdl2-compat' in r for r in _deps(binary)):
        dest = os.path.join(frameworks, 'libSDL3.dylib')
        shutil.copy2(os.path.realpath(sdl3), dest)
        os.chmod(dest, 0o755)
        subprocess.run(['install_name_tool', '-id', '@executable_path/../Frameworks/libSDL3.dylib', dest],
                       check=True, capture_output=True)
        seen['libSDL3.dylib'] = dest
        todo.append((dest, os.path.realpath(sdl3)))
    while todo:
        path, source = todo.pop()
        for ref in _deps(path):
            if not external(ref) or os.path.basename(ref) == os.path.basename(path):
                continue
            real = resolve(ref, source)
            name = os.path.basename(ref)
            if name not in seen:
                dest = os.path.join(frameworks, name)
                shutil.copy2(real, dest)
                os.chmod(dest, 0o755)
                subprocess.run(['install_name_tool', '-id', f'@executable_path/../Frameworks/{name}', dest],
                               check=True, capture_output=True)
                seen[name] = dest
                todo.append((dest, real))
            subprocess.run(['install_name_tool', '-change', ref, f'@executable_path/../Frameworks/{name}', path],
                           check=True, capture_output=True)
    for path in [binary, *seen.values()]:
        left = [r for r in _deps(path) if r.startswith('/opt/') or r.startswith('/usr/local/')]
        if left:
            sys.exit(f'{path} still links {left}')
    return sorted(seen)


def macos():
    out = os.path.join(DIST, 'MissingShip-macos.zip')
    # Stage outside the repo: a MissingShip.app left under dist/ gets indexed by Spotlight and registered with Launch
    # Services under the same bundle id as the development app, so launchers (Raycast, Spotlight) open the wrong one.
    import tempfile
    stage = tempfile.mkdtemp(prefix='missingship-macos-')
    app = os.path.join(stage, 'MissingShip.app')
    shutil.copytree(DEV_APP, app, symlinks=True)
    contents = os.path.join(app, 'Contents')
    binary = os.path.join(contents, 'MacOS', 'MissingShip')
    shutil.copy2(MAC_BINARY, binary)
    libs = _bundle_dylibs(binary, os.path.join(contents, 'Frameworks'))

    res = os.path.join(contents, 'Resources')
    shutil.rmtree(os.path.join(res, 'mods'), ignore_errors=True)
    shutil.copytree(MAC_BUILDER, os.path.join(res, 'builder'), symlinks=True)
    with open(os.path.join(ROOT, 'profile', 'shipofharkinian.json')) as f:
        profile = json.load(f)
    profile.pop('WarpPoints', None)  # keeps the Mac window block (Metal renderer)
    with open(os.path.join(res, 'shipofharkinian.json'), 'w') as f:
        json.dump(profile, f, indent=4)

    plist_path = os.path.join(contents, 'Info.plist')
    with open(plist_path, 'rb') as f:
        plist = plistlib.load(f)
    plist['LSEnvironment'] = {'SHIP_HOME': MAC_HOME}  # the engine expands the leading ~
    plist['CFBundleShortVersionString'] = VERSION
    with open(plist_path, 'wb') as f:
        plistlib.dump(plist, f)

    subprocess.run(['xattr', '-cr', app], check=True)
    # The builder's programs and libraries sit in Resources, where codesign --deep doesn't look: sign each first.
    for path, _ in files_under(os.path.join(res, 'builder')):
        if not os.path.islink(path) and open(path, 'rb').read(4) in (b'\xcf\xfa\xed\xfe', b'\xca\xfe\xba\xbe'):
            subprocess.run(['codesign', '--force', '--sign', '-', path], check=True, capture_output=True)
    subprocess.run(['codesign', '--force', '--deep', '--sign', '-', app], check=True, capture_output=True)
    subprocess.run(['codesign', '--verify', '--deep', '--strict', app], check=True)
    with open(os.path.join(stage, 'README.txt'), 'w') as f:
        f.write(MAC_README)
    if os.path.exists(out):
        os.remove(out)
    subprocess.run(['ditto', '-c', '-k', '--sequesterRsrc', stage, out], check=True)
    shutil.rmtree(stage)
    print('bundled', len(libs), 'libraries:', ' '.join(libs))
    return out


UNBOUND_README = """The Missing Link for SoH: Unbound (macOS)

This builds The Missing Link as a mod for SoH: Unbound from your own files. Nothing from the games is included.

You need
  - SoH: Unbound, started once with your Ocarina of Time USA 1.0 (NTSC 1.0) ROM, so it has made oot.o2r.
  - Kaze's The Missing Link v2.0 patch from https://www.romhacking.net/hacks/5334/ (the .zip as downloaded is fine)
    and that same OoT ROM; or a Missing Link v2.0 ROM you already patched.

Build it (Terminal, in this folder; about a minute)
  MissingShipBuilder/MissingShipBuilder --unbound "/Applications/<your Unbound>.app" <OoT ROM> <patch .zip>
or, with a Missing Link ROM:
  MissingShipBuilder/MissingShipBuilder --unbound "/Applications/<your Unbound>.app" <Missing Link ROM>

It reads oot.o2r from Unbound's folder (~/Library/Application Support/com.shipofharkinian.soh) and writes
mods/MissingLink_Unbound.o2r there. Start Unbound and The Missing Link is loaded. Build it again after updating
Unbound or this package. --oot-o2r and --out choose other places.

For a stock Unbound with code mods (skijer's NEI Unbound 2.0, on SoH 9.3.0 or later), add --stock:
  MissingShipBuilder/MissingShipBuilder --unbound "/Applications/<your Unbound>.app" --stock <OoT ROM> <patch .zip>
It then also writes mods/missinglink.o2r, The Missing Link's behaviour as a code mod (compiled code only, for
Windows, Linux and macOS). It needs an Unbound on SoH 9.3.0 or later: the builder refuses an older one. It is not
signed yet: a build of Unbound that loads only signed mods refuses it, and one that asks shows its name and content
hash the first time; answer "Always load this version".

MissingShipBuilder/unbound.txt names the Unbound build this package was made for, and MissingShipBuilder/codemod.txt
the code mod's commit.
"""


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
    subprocess.run(['xattr', '-cr', top], check=True)
    for path, _ in files_under(top):
        if not os.path.islink(path) and open(path, 'rb').read(4) in (b'\xcf\xfa\xed\xfe', b'\xca\xfe\xba\xbe'):
            subprocess.run(['codesign', '--force', '--sign', '-', path], check=True, capture_output=True)
    if os.path.exists(out):
        os.remove(out)
    subprocess.run(['ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', top, out], check=True)
    shutil.rmtree(stage)
    return out


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
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    os.makedirs(DIST, exist_ok=True)
    if sys.argv[1] == 'unbound-macos':
        check_unbound_builder(MAC_BUILDER)
        path = unbound_macos()
        print(f'{os.path.relpath(path, ROOT)}  {os.path.getsize(path) / 1e6:.1f} MB')
        return
    if sys.argv[1] == 'macos':
        check_builder(MAC_BUILDER)
        path = macos()
        print(f'{os.path.relpath(path, ROOT)}  {os.path.getsize(path) / 1e6:.1f} MB')
        return
    art = artifacts(sys.argv[1])
    soh_o2r = os.path.join(art, 'soh.o2r', 'soh.o2r')
    profile = portable_profile()
    made = []
    for osname, package in (('windows', windows), ('linux', linux)):
        if os.path.isdir(os.path.join(art, f'missingship-{osname}')):
            builder = os.path.join(art, f'missingship-builder-{osname}')
            check_builder(builder)
            made.append(package(os.path.join(art, f'missingship-{osname}'), builder, soh_o2r, profile))
    if not made:
        sys.exit(f'no missingship-windows or missingship-linux artifact in {art}')
    for path in made:
        print(f'{os.path.relpath(path, ROOT)}  {os.path.getsize(path) / 1e6:.1f} MB')


if __name__ == '__main__':
    main()
