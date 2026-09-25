"""Pre-push checks for MissingShip (MS-008). Run before every push; the git hook in .githooks/ runs it for you.

    python3 tools/check.py            fast checks (a few seconds)
    python3 tools/check.py --build    also build the engine (slow)

Each check prints PASS, FAIL or SKIP (a missing optional input, e.g. the debug ROM). Exits 1 if anything failed.

  compile    every script in tools/ and pipeline/ byte-compiles
  engine     the engine fork has no uncommitted or unpushed work (docs/*.patch is generated from commits), and
             engine/ is at the commit MissingShip pins
  patches    docs/*.patch match the engine fork                     (tools/genpatch.py --check)
  ledger     every patched file is named in ENGINE_CHANGES.md        (tools/checkledger.py)
  exits      every ML exit remaps to the right SoH entrance          (tools/checkexits.py)
  hashes     every resource reference in the mod archive resolves, except the textures the pipeline skips on
             purpose (pipeline/unresolved.json, MS-015)             (tools/checkhashes.py)
  inventory  ML_ACTOR_INVENTORY.md matches the ROMs                  (tools/survey_actors.py --check; needs the debug ROM)
  build      the engine builds                                       (--build only)
"""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', 'pipeline'))
from mspaths import ROOT, PIPE, SHIPWRIGHT, ML_ROM, DEBUG_ROM, OOT_O2R
import collections, glob, json, re, subprocess, sys

TOOLS = _os.path.join(ROOT, 'tools')
results = []

def report(name, status, detail=''):
    results.append(status)
    print(f'{status:4}  {name:9}  {detail}')

def run(*args, cwd=ROOT):
    p = subprocess.run([sys.executable, *args], cwd=cwd, capture_output=True, text=True)
    return p.returncode, (p.stdout + p.stderr).strip()

def last_line(text):
    return text.splitlines()[-1] if text else ''

def git(repo, *args):
    return subprocess.run(['git', '-C', repo, *args], capture_output=True, text=True).stdout.strip()

# compile ------------------------------------------------------------------------------------------------------
bad = []
for f in sorted(glob.glob(_os.path.join(ROOT, 'tools', '*.py')) + glob.glob(_os.path.join(PIPE, '*.py'))):
    try: compile(open(f).read(), f, 'exec')      # in memory: no .pyc written
    except SyntaxError as e: bad.append(f'{_os.path.basename(f)}:{e.lineno}: {e.msg}')
report('compile', 'FAIL' if bad else 'PASS', '; '.join(bad) or 'all scripts compile')

# engine -------------------------------------------------------------------------------------------------------
if not _os.path.isdir(_os.path.join(SHIPWRIGHT, '.git')) and not _os.path.isfile(_os.path.join(SHIPWRIGHT, '.git')):
    report('engine', 'SKIP', f'no engine checkout at {SHIPWRIGHT} (set MS_SHIPWRIGHT)')
    engine = False
else:
    engine = True
    problems = []
    for repo, label in ((SHIPWRIGHT, 'engine'), (_os.path.join(SHIPWRIGHT, 'libultraship'), 'libultraship'),
                        (_os.path.join(SHIPWRIGHT, 'ZAPDTR'), 'ZAPDTR')):
        dirty = [l for l in git(repo, 'status', '--porcelain').splitlines() if l.strip()]
        if dirty: problems.append(f'{label}: {len(dirty)} uncommitted change(s), e.g. {dirty[0].strip()}')
        ahead = git(repo, 'rev-list', '--count', '@{u}..HEAD')
        if ahead.isdigit() and int(ahead): problems.append(f'{label}: {ahead} unpushed commit(s)')
        branch = git(repo, 'branch', '--show-current')
        if branch != 'missingship': problems.append(f'{label}: on branch "{branch or "detached"}", expected missingship')
    if _os.path.realpath(SHIPWRIGHT) == _os.path.realpath(_os.path.join(ROOT, 'engine')):
        pin = git(ROOT, 'submodule', 'status', 'engine')      # '+' = checked-out commit differs from the pinned one
        if pin.startswith('+'):
            problems.append('engine/ is not at the commit MissingShip pins: commit the pointer (git add engine)')
    report('engine', 'FAIL' if problems else 'PASS', '; '.join(problems) or 'fork clean, pushed and pinned (missingship)')

# patches / ledger ---------------------------------------------------------------------------------------------
if engine:
    rc, out = run(_os.path.join(TOOLS, 'genpatch.py'), '--check')
    stale = [l.split(':')[0] for l in out.splitlines() if 'out of date' in l]
    report('patches', 'FAIL' if rc else 'PASS', f'stale: {", ".join(stale)} (run tools/genpatch.py)' if rc else 'docs/*.patch match the fork')
else:
    report('patches', 'SKIP', 'needs the engine checkout')
rc, out = run(_os.path.join(TOOLS, 'checkledger.py'))
report('ledger', 'FAIL' if rc else 'PASS', last_line(out))

# exits / hashes -----------------------------------------------------------------------------------------------
if not _os.path.exists(ML_ROM) or not engine:
    report('exits', 'SKIP', 'needs the ML ROM and the engine checkout')
else:
    rc, out = run(_os.path.join(TOOLS, 'checkexits.py'))
    report('exits', 'FAIL' if rc else 'PASS', last_line(out) if not rc else '; '.join(l for l in out.splitlines() if 'exit' in l)[:300])

mod = _os.path.join(PIPE, 'ml.otr')
if not (_os.path.exists(mod) and _os.path.exists(OOT_O2R)):
    report('hashes', 'SKIP', 'needs pipeline/ml.otr and oot.o2r')
else:
    rc, out = run(_os.path.join(TOOLS, 'checkhashes.py'), mod, '')
    found = collections.Counter(m.group(1) for m in re.finditer(r'^UNRESOLVED \S*?/(\w+_room_\d+)DL', out, flags=re.M))
    other = [l for l in out.splitlines() if l.startswith('UNRESOLVED') and not re.search(r'/\w+_room_\d+DL', l)]
    skipped = json.load(open(_os.path.join(PIPE, 'unresolved.json')))
    allowed = collections.Counter({room: len(v) for room, v in skipped.items()})
    extra = (found - allowed) + collections.Counter({l: 1 for l in other})
    total = sum(found.values()) + len(other)
    if extra:
        report('hashes', 'FAIL', f'unexpected unresolved references: {dict(extra)}')
    else:
        report('hashes', 'PASS', last_line(out).replace(' unresolved', f' unresolved, all {total} skipped on purpose (MS-015)'
                                                         if total else ' unresolved'))

# inventory ----------------------------------------------------------------------------------------------------
if not (_os.path.exists(DEBUG_ROM) and _os.path.exists(ML_ROM)):
    report('inventory', 'SKIP', f'needs the debug ROM at {_os.path.relpath(DEBUG_ROM, ROOT)} (or MS_DEBUG_ROM)')
else:
    rc, out = run(_os.path.join(TOOLS, 'survey_actors.py'), '--check')
    report('inventory', 'FAIL' if rc else 'PASS', last_line(out))

# build --------------------------------------------------------------------------------------------------------
if '--build' in sys.argv:
    build = _os.path.join(SHIPWRIGHT, 'build-ml')
    if not engine or not _os.path.isdir(build):
        report('build', 'SKIP', f'no configured build at {build} (cmake -S . -B build-ml)')
    else:
        p = subprocess.run(['cmake', '--build', build, '--target', 'soh', '-j8'], capture_output=True, text=True)
        errors = [l for l in (p.stdout + p.stderr).splitlines() if 'error:' in l]
        report('build', 'FAIL' if p.returncode else 'PASS', (errors[0][:300] if errors else f'exit {p.returncode}')
               if p.returncode else 'engine builds')

failed = results.count('FAIL')
print(f'\n{len(results)} checks: {results.count("PASS")} passed, {failed} failed, {results.count("SKIP")} skipped')
sys.exit(1 if failed else 0)
