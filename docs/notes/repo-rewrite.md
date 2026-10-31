---
name: repo-rewrite
description: MissingShip history rewritten 2026-10-17 (game data) and 2026-10-18 (Claude lines); public since 2026-10-18; backups in AI_Projects/repo-rewrite
metadata:
  type: project
---

Owner asked 2026-10-17 to work on the history rewrite (route chosen over a fresh repo) before public hosting.
Staged in `AI_Projects/repo-rewrite/`: `MissingShip.git` (untouched mirror = backup), `rewrite.git` (git-filter-repo
from `venv/`, paths in `remove-paths.txt`: roms/, home/, mod/, *.z64/n64/v64/o2r/otr, pipeline/ml_code.bin, the
engine binary; a blob callback strips the ml_titlecard_banner.inc hunk from old docs/soh-missingship.patch).
469 MB -> 2.8 MB, 253 commits, HEAD tree identical, `tools/nodata.py --git` PASS, no secrets.

**Done 2026-10-17:** owner OK'd the one-time force-push of main + the 3 builder tags (new main 2af750d; old 2d4c992).
Local checkout reset; docs' SHAs remapped via rewrite.git/filter-repo/commit-map (old SHAs in issue comments and older
memories are pre-rewrite). Old commits stay fetchable by SHA; owner 2026-10-17: no GitHub Support purge needed, as long as nothing public carries
Nintendo code or data when we go public.
**How to apply:** the no-force-push rule ([[push-main-after-commit]]) is back in force. Keep the backup mirror until the
owner says otherwise. Related: [[no-copyrighted-data-in-git]].

**Second rewrite, 2026-10-18** (owner chose it): commit messages lost their Co-Authored-By: Claude lines
(`repo-rewrite/trailers.git`, backup `repo-rewrite/backup-2026-10-18.git`); main and the 4 release tags
force-pushed (main 464d57b + remap e392ea6; tags childshield-1 31cbe6e, builder-1/2/3 28210d8/e77df9d/722d438).
The repo then went **public** (owner asked), nodata --git PASS on the new history. The no-force-push rule is back.

**Private forks, 2026-10-18** (owner: "clean the private repos too"): our ZAPDTR fork, -libultraship, -Shipwright
and -Unbound rewritten with `repo-rewrite/private/strip.py` (filter-repo): only the owner's own commits that are not
reachable from any upstream ref (HarbourMasters, roborich, skijer, Kenix3, the public libultraship fork) lose the
lines; submodule pointers remapped. Upstream commits must never change (GitHub-signed; fast-export strips gpgsig, and
merges from upstream would break): the first attempt did change them and was overwritten. Backups:
`repo-rewrite/private/*-backup.git`. Local checkouts reset; `UNBOUND_COMPARE` is now d5dd164 (same files).
Same day, MissingShip dropped the `engine` submodule: `engine/` is an untracked local clone (see [[no-private-repo-links]]).

