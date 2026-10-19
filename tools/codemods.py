"""Copy the code mods' sources into codemods/, the public folder skijer's sign-mod workflow builds and signs them from.

    python3 tools/codemods.py           copy them from the code mod checkout's ml-codemod-930
    python3 tools/codemods.py --check   exit 1 if codemods/ or UNBOUND_REF differs from that branch

codemods/<mod>/ is mod-sdk/mods/<mod> as committed on ml-codemod-930 (mspaths.CODEMOD_SRC), and UNBOUND_REF names
the Unbound commit the workflow builds them against (mspaths.CODEMOD_HOST, skijer's build players run). The branch
stays the source: edit and commit there, then copy. A signature request names a MissingShip commit and the folder
`codemods` (DEVELOPMENT.md, Releases).
"""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', 'pipeline'))
from mspaths import ROOT, CODEMOD_SRC, CODEMOD_HOST
import shutil, subprocess, sys

BRANCH = 'ml-codemod-930'
MODS = ['missinglink', 'childshield']
OUT = _os.path.join(ROOT, 'codemods')
REF = _os.path.join(ROOT, 'UNBOUND_REF')


def git(*args):
    return subprocess.run(['git', '-C', CODEMOD_SRC, *args], check=True, capture_output=True).stdout


def wanted():
    """{path under ROOT: bytes} for every file codemods/ and UNBOUND_REF should hold."""
    files = {'UNBOUND_REF': (CODEMOD_HOST + '\n').encode()}
    for mod in MODS:
        prefix = f'mod-sdk/mods/{mod}/'
        for path in git('ls-tree', '-r', '--name-only', BRANCH, prefix).decode().splitlines():
            files[f'codemods/{mod}/{path[len(prefix):]}'] = git('show', f'{BRANCH}:{path}')
    return files


def present():
    files = {}
    if _os.path.exists(REF):
        files['UNBOUND_REF'] = open(REF, 'rb').read()
    for base, _, names in _os.walk(OUT):
        for name in names:
            path = _os.path.join(base, name)
            files[_os.path.relpath(path, ROOT)] = open(path, 'rb').read()
    return files


def main():
    want = wanted()
    have = present()
    differ = sorted(p for p in set(want) | set(have) if want.get(p) != have.get(p))
    if '--check' in sys.argv:
        if differ:
            print(f'codemods out of date: {len(differ)} file(s), e.g. {differ[0]} (run tools/codemods.py)')
            return 1
        print(f'codemods match {BRANCH} ({len(want) - 1} files)')
        return 0
    shutil.rmtree(OUT, ignore_errors=True)
    for path, data in want.items():
        _os.makedirs(_os.path.dirname(_os.path.join(ROOT, path)), exist_ok=True)
        with open(_os.path.join(ROOT, path), 'wb') as f:
            f.write(data)
    print(f'codemods: {len(want) - 1} files from {BRANCH} ({len(differ)} changed)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
