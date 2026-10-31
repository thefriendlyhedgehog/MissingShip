---
name: no-private-repo-links
description: owner 2026-10-18 — public docs never link or name the owner's private repos; link the repo's own releases relatively
metadata:
  type: feedback
---

Owner, 2026-10-18: "dont link to my private repos in a public document."

**Why:** README.md and DEVELOPMENT.md are meant to go public; the engine forks (the engine fork,
-libultraship, -ZAPDTR) and our Unbound fork stay private.
**How to apply:** in README.md, DEVELOPMENT.md, package README.txt texts and release notes, say "the `engine/` fork"
or "our Unbound fork", never a github.com/thefriendlyhedgehog/... URL or org/repo slug for them. Link this repo's
releases as `../../releases`. docs/ROADMAP.md still links MissingShip's own issues (fine once the repo is public).
Relates to [[no-engine-game]].
Since 2026-10-18 the public repo has no `.gitmodules`/engine gitlink; tools read private repo names from the local
checkouts' `origin` remotes (codemod.github_repo, package.fork).

tools/genpatch.py leaves `.github` out of docs/soh-missingship.patch (2026-10-18), because the fork's CI names the private repos. Older public commits of that patch still contain the names; the owner said on 2026-10-18 to leave that history as is, so do not rewrite it.
