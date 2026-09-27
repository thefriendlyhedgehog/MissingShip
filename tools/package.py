"""Package MissingShip for Windows and Linux from a generate-builds run on the engine fork (MS-080, MS-081).

    python3 tools/package.py <run id>      # downloads the run's artifacts with gh
    python3 tools/package.py <artifact dir>
    python3 tools/package.py macos         # the local macOS build (engine/build-ml), on this Mac

For macOS, build the engine first (cmake --build engine/build-ml --target soh) at the commit being released.

CI builds the engine (soh.o2r, the Windows exe, the Linux AppImage). This adds what only this repo has -- the ML mod
archive and a portable settings profile -- and writes dist/MissingShip-windows.zip and dist/MissingShip-linux.tar.gz.
Neither package holds oot.o2r: SoH makes it from the player's own OoT ROM on first launch (or copy one in).

Where the game looks for its files: on Windows, the folder it is started from; on Linux, SHIP_HOME, which the
MissingShip.sh launcher points at its own folder.
"""
import io, json, os, plistlib, shutil, subprocess, sys, tarfile, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(ROOT, 'dist')
FORK = 'thefriendlyhedgehog/MissingShip-Shipwright'
MOD = os.path.join(ROOT, 'mod', 'MissingLink_Scenes.o2r')
DEV_APP = os.path.join(ROOT, 'MissingShip.app')
MAC_BINARY = os.path.join(ROOT, 'engine', 'build-ml', 'soh', 'soh-macos')
MAC_HOME = '~/Library/Application Support/MissingShip'
VERSION = '0.1-beta'

README = """MissingShip -- The Missing Link on the Ship of Harkinian engine ({os})

First launch
  {launch}
  SoH asks for your own Ocarina of Time ROM and builds oot.o2r from it (once). You can instead copy an oot.o2r
  made by Ship of Harkinian into this folder.

Files
  mods/MissingLink_Scenes.o2r   The Missing Link (keep it in mods/)
  soh.o2r                       Ship of Harkinian's own assets
  shipofharkinian.json          Settings (created or updated by the game)
  Save/                         Save files (created by the game)
{extra}"""

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


def windows(src, soh_o2r, profile):
    out = os.path.join(DIST, 'MissingShip-windows.zip')
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        for path, rel in files_under(src):
            name = 'MissingShip.exe' if rel == 'soh.exe' else rel
            if name.lower() in ('readme.txt', 'readme.md'):
                continue
            z.write(path, f'MissingShip/{name}')
        if not any(n.endswith('/soh.o2r') for n in z.namelist()):
            z.write(soh_o2r, 'MissingShip/soh.o2r')
        z.write(MOD, 'MissingShip/mods/MissingLink_Scenes.o2r')
        z.writestr('MissingShip/shipofharkinian.json', profile)
        z.writestr('MissingShip/README.txt', README.format(os='Windows', launch='Run MissingShip.exe.', extra=''))
    return out


def linux(src, soh_o2r, profile):
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
        add(t, 'mods/MissingLink_Scenes.o2r', path=MOD)
        add(t, 'shipofharkinian.json', profile)
        add(t, 'README.txt', README.format(os='Linux', launch='Run ./MissingShip.sh (not the AppImage directly).', extra=LINUX_EXTRA).encode())
    return out


MAC_README = """MissingShip -- The Missing Link on the Ship of Harkinian engine (macOS, Apple Silicon)

First launch
  1. Move MissingShip.app to Applications (or anywhere you like).
  2. macOS blocks apps from unidentified developers the first time: open MissingShip once, then go to
     System Settings > Privacy & Security and click "Open Anyway" next to MissingShip.
  3. MissingShip asks for your own Ocarina of Time ROM and builds oot.o2r from it (once).

Where your files are
  ~/Library/Application Support/MissingShip   settings, saves, logs, oot.o2r and the installed mod
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
    stage = os.path.join(DIST, 'macos')
    shutil.rmtree(stage, ignore_errors=True)
    app = os.path.join(stage, 'MissingShip.app')
    shutil.copytree(DEV_APP, app, symlinks=True)
    contents = os.path.join(app, 'Contents')
    binary = os.path.join(contents, 'MacOS', 'MissingShip')
    shutil.copy2(MAC_BINARY, binary)
    libs = _bundle_dylibs(binary, os.path.join(contents, 'Frameworks'))

    res = os.path.join(contents, 'Resources')
    os.makedirs(os.path.join(res, 'mods'), exist_ok=True)
    shutil.copy2(MOD, os.path.join(res, 'mods', 'MissingLink_Scenes.o2r'))
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
    subprocess.run(['codesign', '--force', '--deep', '--sign', '-', app], check=True, capture_output=True)
    subprocess.run(['codesign', '--verify', '--deep', '--strict', app], check=True)
    with open(os.path.join(stage, 'README.txt'), 'w') as f:
        f.write(MAC_README)
    if os.path.exists(out):
        os.remove(out)
    subprocess.run(['ditto', '-c', '-k', '--sequesterRsrc', stage, out], check=True)
    print('bundled', len(libs), 'libraries:', ' '.join(libs))
    return out


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    os.makedirs(DIST, exist_ok=True)
    if sys.argv[1] == 'macos':
        path = macos()
        print(f'{os.path.relpath(path, ROOT)}  {os.path.getsize(path) / 1e6:.1f} MB')
        return
    art = artifacts(sys.argv[1])
    soh_o2r = os.path.join(art, 'soh.o2r', 'soh.o2r')
    profile = portable_profile()
    made = []
    if os.path.isdir(os.path.join(art, 'missingship-windows')):
        made.append(windows(os.path.join(art, 'missingship-windows'), soh_o2r, profile))
    if os.path.isdir(os.path.join(art, 'missingship-linux')):
        made.append(linux(os.path.join(art, 'missingship-linux'), soh_o2r, profile))
    if not made:
        sys.exit(f'no missingship-windows or missingship-linux artifact in {art}')
    for path in made:
        print(f'{os.path.relpath(path, ROOT)}  {os.path.getsize(path) / 1e6:.1f} MB')


if __name__ == '__main__':
    main()
