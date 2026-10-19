# Clean source snapshot

Prepared from City Sim VR Gitea main commit `c54e01c7fccba711271f3b68c025c7ba4f00a804`
(PR #284), on 2026-10-19, under source preparation issue #286.

This repository begins with one new root commit. Earlier Gitea commits, tags,
reflogs, configuration and credentials were not imported. Historical documentation,
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
