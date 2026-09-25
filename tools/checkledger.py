"""MS-003 guard: every file the engine patch touches must be named in docs/ENGINE_CHANGES.md.

A file counts as covered when the ledger mentions its file stem (`z_en_cow`) or its actor/overlay name without the
`ovl_` prefix (`En_Cow`). Prints the uncovered files and exits 1 if there are any. Usage: checkledger.py
"""
import os, re, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ledger = open(os.path.join(ROOT, 'docs', 'ENGINE_CHANGES.md')).read()
patch = open(os.path.join(ROOT, 'docs', 'soh-missingship.patch')).read()
files = re.findall(r'^diff --git a/(\S+)', patch, flags=re.M)
missing = []
for f in files:
    stem = os.path.splitext(os.path.basename(f))[0]
    owner = os.path.basename(os.path.dirname(f))
    owner = owner[4:] if owner.startswith('ovl_') else owner
    if stem not in ledger and owner not in ledger:
        missing.append(f)
for f in missing:
    print('not in ENGINE_CHANGES.md:', f)
print(f'{len(files)} patched files, {len(missing)} missing from the ledger')
sys.exit(1 if missing else 0)
