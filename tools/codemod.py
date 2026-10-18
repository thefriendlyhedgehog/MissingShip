"""ML's code mod for every OS, for the builder to hand out beside a --stock layer.

    python3 tools/codemod.py [--mod childshield] [--out <missinglink.o2r>]

The code mod (ML's C behaviour for a stock Unbound with skijer's mod API) lives in MissingShip-Unbound, branch
ml-codemod-930, mod-sdk/mods/missinglink (mspaths.CODEMOD_SRC). It is for SoH 9.3.0 hosts only: the checkout must
hold skijer's 9.3.0 mod API with ML's host hooks (mspaths.CODEMOD_HOST, skijer/Shipwright#33). This takes the
checkout's commit, builds the macOS binary here, takes the Windows and Linux ones from that commit's build-missinglink
CI run (pushed, finished, green), and merges the three with the SDK's merge_mod_packages.py into mspaths.CODEMOD,
with codemod.txt beside it naming the commit. It holds compiled code only: no ROM or game data.

--mod childshield does the same for the branch's other shipped mod (young Link holds the Hylian Shield, ML's rule
alone), into dist/codemod/childshield.o2r with childshield.txt beside it.
"""
import argparse, json, os, subprocess, sys, tempfile, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'pipeline'))
import mspaths  # noqa: E402

REPO = 'thefriendlyhedgehog/MissingShip-Unbound'
WORKFLOW = 'build-missinglink.yml'
MODS = ('missinglink', 'childshield')
PLATFORMS = ('windows_x64', 'linux_x64', 'darwin')


def git(src, *args):
    return subprocess.run(['git', '-C', src, *args], check=True, capture_output=True, text=True).stdout.strip()


def ci_packages(commit, dest, mod):
    """The Windows and Linux packages from this commit's green build-missinglink run."""
    runs = json.loads(subprocess.run(
        ['gh', 'run', 'list', '--repo', REPO, '--workflow', WORKFLOW, '--commit', commit, '--json',
         'databaseId,status,conclusion'], check=True, capture_output=True, text=True).stdout)
    green = [r for r in runs if r['status'] == 'completed' and r['conclusion'] == 'success']
    if not green:
        state = ', '.join(f"{r['status']}/{r['conclusion']}" for r in runs) or 'no run'
        sys.exit(f'no green {WORKFLOW} run for {commit[:10]} ({state}): push the branch and let CI finish')
    run = str(green[0]['databaseId'])
    subprocess.run(['gh', 'run', 'download', run, '--repo', REPO, '--dir', dest], check=True)
    found = [os.path.join(dest, f'{mod}-{p}', f'{mod}.o2r') for p in ('windows_x64', 'linux_x64')]
    for f in found:
        if not os.path.isfile(f):
            sys.exit(f'CI run {run} has no {os.path.relpath(f, dest)}')
    return run, found


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--mod', default='missinglink', choices=MODS)
    ap.add_argument('--out', help='default: mspaths.CODEMOD, or dist/codemod/<mod>.o2r beside it')
    ap.add_argument('--src', default=mspaths.CODEMOD_SRC, help='the MissingShip-Unbound checkout (ml-codemod-930)')
    args = ap.parse_args()
    mod = args.mod
    out = args.out or (mspaths.CODEMOD if mod == 'missinglink' else
                       os.path.join(os.path.dirname(mspaths.CODEMOD), f'{mod}.o2r'))
    src, out = os.path.abspath(args.src), os.path.abspath(out)
    mod_dir = os.path.join('mod-sdk', 'mods', mod)
    if not os.path.isdir(os.path.join(src, mod_dir)):
        sys.exit(f'{src} has no {mod_dir}: name the checkout with --src or MS_CODEMOD_SRC')
    if git(src, 'status', '--porcelain', '--', mod_dir):
        sys.exit(f'{mod_dir} has uncommitted changes in {src}: commit and push them first')
    commit, branch = git(src, 'rev-parse', 'HEAD'), git(src, 'rev-parse', '--abbrev-ref', 'HEAD')
    if subprocess.run(['git', '-C', src, 'merge-base', '--is-ancestor', mspaths.CODEMOD_HOST, commit],
                      capture_output=True).returncode != 0:
        sys.exit(f'{branch} {commit[:10]} lacks skijer\'s 9.3.0 mod API ({mspaths.CODEMOD_HOST[:10]}, '
                 'skijer/Shipwright#33): the code mod is built for SoH 9.3.0 hosts only (ml-codemod-930)')
    if git(src, 'branch', '-r', '--contains', commit) == '':
        sys.exit(f'{commit[:10]} is not pushed: CI builds the Windows and Linux binaries from the pushed branch')

    with tempfile.TemporaryDirectory(prefix='codemod-') as work:
        print(f'code mod at {branch} {commit[:10]}: building the macOS binary')
        build = os.path.join(work, 'build')
        r = subprocess.run([sys.executable, os.path.join(src, 'mod-sdk', 'build_mods.py'), '--mod', mod,
                            '--no-templates', '--build-dir', build], capture_output=True, text=True)
        if r.returncode != 0:
            print((r.stdout + r.stderr)[-4000:], file=sys.stderr)
            sys.exit('building the macOS binary failed')
        mac = os.path.join(build, 'packages', 'Release', f'{mod}.o2r')
        run, ci = ci_packages(commit, os.path.join(work, 'ci'), mod)
        print(f'Windows and Linux binaries from CI run {run}')
        os.makedirs(os.path.dirname(out), exist_ok=True)
        subprocess.run([sys.executable, os.path.join(src, 'mod-sdk', 'tools', 'merge_mod_packages.py'), *ci, mac,
                        '--output', out + '.tmp'], check=True, stdout=subprocess.DEVNULL)
    with zipfile.ZipFile(out + '.tmp') as z:
        binaries = json.loads(z.read('manifest.json')).get('binaries', {})
    missing = [p for p in PLATFORMS if p not in binaries]
    if missing:
        os.remove(out + '.tmp')
        sys.exit(f'the merged package lacks {", ".join(missing)}')
    os.replace(out + '.tmp', out)
    note = 'codemod.txt' if mod == 'missinglink' else f'{mod}.txt'
    with open(os.path.join(os.path.dirname(out), note), 'w') as f:
        f.write(f'{commit} {branch} (CI run {run})\n')
    print(f'{os.path.relpath(out, ROOT)}: {", ".join(sorted(binaries))}, {os.path.getsize(out) / 1e3:.0f} kB')


if __name__ == '__main__':
    main()
