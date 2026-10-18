"""Regenerate docs/*.patch from the engine fork (MS-007).

The engine lives in our private SoH fork, checked out at engine/ (branch `missingship`, based on SoH tag
9.2.3), with its libultraship and ZAPDTR submodules pointing at our libultraship fork / our ZAPDTR fork (branch
`missingship`). The patches in docs/ are generated from the committed fork. They are not the source of truth; they
exist for review, for tools/checkledger.py, and for applying the changes to a stock SoH checkout.

    soh-missingship.patch           9.2.3..missingship in the fork, minus submodules, .gitmodules and ML_DATA
    libultraship-missingship.patch  the libultraship commit SoH 9.2.3 pins..missingship
    zapd-missingship.patch          the ZAPDTR commit SoH 9.2.3 pins..missingship

Only committed work is exported: commit in the fork first. Usage: genpatch.py [--check]
(--check exits 1 if a patch in docs/ is out of date, without writing).
"""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', 'pipeline'))
from mspaths import ROOT, SHIPWRIGHT
import subprocess, sys

# A git hook runs us with GIT_DIR etc. exported (an absolute .git/worktrees/<name> in a worktree), which would make
# `git -C engine ...` read MissingShip instead of the fork. Drop git's repo-location vars so every git call finds its
# repo from -C.
for _v in subprocess.run(['git', 'rev-parse', '--local-env-vars'], capture_output=True, text=True).stdout.split():
    _os.environ.pop(_v, None)

BASE = '9.2.3'
BRANCH = 'missingship'
DOCS = _os.path.join(ROOT, 'docs')
# Fork files that hold ML's art itself (generated from its ROM), left out of the patches: this repo carries no game
# data (MS-156; tools/nodata.py --git checks the history). The fork has them; the patch names them in no hunk.
ML_DATA = ['soh/src/code/ml_titlecard_banner.inc']

def git(repo, *args):
    if args[0] == 'diff':   # full blob hashes: abbreviations grow with repo size, so they'd differ between clones
        args = ('diff', '--full-index', *args[1:])
    return subprocess.run(['git', '-C', repo, *args], check=True, capture_output=True, text=True).stdout

def pinned(sub):
    """The commit the SoH base tag pins for a submodule."""
    return git(SHIPWRIGHT, 'rev-parse', f'{BASE}:{sub}').strip()

patches = {
    'soh-missingship.patch': git(SHIPWRIGHT, 'diff', BASE, BRANCH, '--', '.', ':!ZAPDTR', ':!libultraship', ':!.gitmodules',
                                 *(f':!{p}' for p in ML_DATA)),
    'libultraship-missingship.patch': git(_os.path.join(SHIPWRIGHT, 'libultraship'), 'diff', pinned('libultraship'), BRANCH),
    'zapd-missingship.patch': git(_os.path.join(SHIPWRIGHT, 'ZAPDTR'), 'diff', pinned('ZAPDTR'), BRANCH),
}
stale = []
for name, text in patches.items():
    path = _os.path.join(DOCS, name)
    old = open(path).read() if _os.path.exists(path) else None
    if old != text:
        stale.append(name)
        if '--check' not in sys.argv:
            open(path, 'w').write(text)
    print(f'{name}: {text.count("diff --git")} files' + (' (updated)' if name in stale and '--check' not in sys.argv
                                                         else ' (out of date)' if name in stale else ' (up to date)'))
if '--check' in sys.argv and stale:
    sys.exit(1)
