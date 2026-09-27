"""Package MissingShip for Windows and Linux from a generate-builds run on the engine fork (MS-080, MS-081).

    python3 tools/package.py <run id>      # downloads the run's artifacts with gh
    python3 tools/package.py <artifact dir>

CI builds the engine (soh.o2r, the Windows exe, the Linux AppImage). This adds what only this repo has -- the ML mod
archive and a portable settings profile -- and writes dist/MissingShip-windows.zip and dist/MissingShip-linux.tar.gz.
Neither package holds oot.o2r: SoH makes it from the player's own OoT ROM on first launch (or copy one in).

Where the game looks for its files: on Windows, the folder it is started from; on Linux, SHIP_HOME, which the
MissingShip.sh launcher points at its own folder.
"""
import io, json, os, subprocess, sys, tarfile, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(ROOT, 'dist')
FORK = 'thefriendlyhedgehog/MissingShip-Shipwright'
MOD = os.path.join(ROOT, 'mod', 'MissingLink_Scenes.o2r')

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


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    os.makedirs(DIST, exist_ok=True)
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
