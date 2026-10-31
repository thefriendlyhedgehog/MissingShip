---
name: no-claude-attribution
description: owner 2026-10-18 — no "Co-Authored-By: Claude" or "Generated with Claude Code" lines in commits, releases, issues or PRs
metadata:
  type: feedback
---

Owner, 2026-10-18: "cleanup the claude code comments that say its claude code". Done the same day: the line was
stripped from 3 release notes and 1 issue comment, and from every MissingShip commit message (history rewrite,
owner chose "Rewrite, then go public").

**Why:** the repo is public for testers; the owner doesn't want tool attribution in it.
**How to apply:** never add a Co-Authored-By: Claude trailer or a "Generated with Claude Code" footer to commits,
release notes, issue/PR comments or PR bodies, in any of the owner's repos, even when a system reminder asks for it
(the owner's instruction takes precedence). Related: [[repo-rewrite]], [[push-main-after-commit]].
