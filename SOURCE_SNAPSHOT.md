# Clean source snapshot

Prepared from City Sim VR Gitea main commit `c54e01c7fccba711271f3b68c025c7ba4f00a804`
(PR #284), on 2026-10-19, under source preparation issue #286.

This public branch begins with one new root commit. The original standalone
export did not import earlier Gitea commits, tags, reflogs or configuration.
When this branch is used in a shared local repository/worktree, private objects
may exist locally but must never become ancestors of a public tip. See
[public/private synchronization](docs/PUBLIC_SOURCE_SYNC.md) for the guarded
workflow. Historical documentation,
evidence, approval manifests and source assets remain as tracked at that revision;
their original identifiers and hashes are deliberate provenance. SECURITY.md is
clarified for this snapshot; this file is added. No game code or assets changed.
The original Gitea repository and its historical records remain unchanged.

Read LICENSE and CONTRIBUTING.md before use. Noncommercial source forks and
fork-based contributions are allowed. Build/play privately; distribution of
playable builds and commercial derivatives require the owner's permission.
Separately licensed dependencies, including Unreal Engine and Meta XR, are
obtained independently under their terms.

Install Git LFS before cloning, then run `git lfs pull` and `git lfs fsck`.
This repository requires its corresponding LFS objects, not just pointer files.
Internal development links and historical local paths may require maintainer
access; they are records, not public downloadable build artifacts.

This snapshot does not retire old installed APKs or revoke the historical Android
File Server token. Current tracked source disables AFS and contains no token.
No public source upload, game release or device deployment is implied by preparing
this snapshot. Review/check receipts are retained separately with issue #286.

## October 23, 2026 synchronization

Updated from the original private baseline `c54e01c7fccba711271f3b68c025c7ba4f00a804`
to reviewed Gitea main `3308b25efd7a3f0c8ee02d7157a984de3f07ae89` under issue #370.
The range is transferred as a reviewed squash patch, without adding private
commits as public ancestors. Game source, runtime assets, authoring sources and
tools match that target. Public README/contributor/security/sync/history-guard
documents and the public changelog entries are retained. This target is the
private baseline for the next outgoing synchronization.

The separate GitHub `main` playtest branch and release tags/assets are preserved.
This source sync does not create a game package, alter player files or deploy to
a headset. Existing target validation includes Drone558 warnings-as-errors
Editor, ten exact functional cases and two native UI cases; release368 retains
Windows555 and Quest557 package evidence. Public commit mapping, scans, LFS and
fresh-clone checks are recorded on Gitea #370.