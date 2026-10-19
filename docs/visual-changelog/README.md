# Visual changelog

[Open the gallery](index.html) or [view the overview sheet](overview.jpg). Each milestone also has a labeled comparison sheet under `sheets/`. The gallery supports task/feature search and opening individual images at a larger size.

This is a curated visual index of existing Blender and native preview evidence, consolidated on 2026-10-11. It is not a second task queue. [DEVELOPMENT_TASKS.md](../DEVELOPMENT_TASKS.md) remains authoritative for active/future work and pending device checks; [CHANGELOG.md](../CHANGELOG.md) retains completed implementation and verification details. Desktop/PIE and Blender images do not establish headset rendering, performance or comfort. Reports/UI fixtures can contain synthetic values.

`gallery.json` records every capture's original path, dimensions, byte size and SHA256, plus the retained derivative's SHA256. Every selected original remains available for reproduction; compact JPEG copies and comparison sheets make the visual history convenient to browse. Active D71/D79/D83 previews are dated snapshots; their originals, authoring sources and integration evidence are preserved.

## Historical artifact retirement — D84

[cleanup-2026-10-11.json](cleanup-2026-10-11.json) is the exact removal manifest: paths, sizes, hashes, reasons, protected files and supersession evidence. [cleanup-verification-2026-10-11.json](cleanup-verification-2026-10-11.json) records the final preservation checks. Historical changelog entries remain historical: if an old log/package/preview path is absent, look it up in the manifest instead of treating it as missing active evidence. Removed log marker counts are diagnostic metadata, not fresh test-pass claims.

Completed: 286 files / 2,284,372,274 bytes (2.13 GiB gross) removed. All 24,423 other inventoried artifact paths remain. Of 3,962 protected hashes, 3,958 match; four changes belong to the concurrently working Blender/audio chats and are recorded with exact evidence in [concurrent-work-evidence.json](concurrent-work-evidence.json). There are no unexplained protected changes. The gallery has 23 milestones and 67 captures; its images, task search/reset and 1440/390px layouts pass verification.

Retired categories are two obsolete Windows packaging outputs (`Artifacts/D75WindowsSandbox` and the unreferenced `Artifacts/ColorPerfPreview`), three path-only isolated D75 helpers, closed-round automatic log backups/redundant console transcripts, and unselected intermediate D59/D49 captures. The maintained Windows/Quest archives include D75 and supersede its isolated helpers; original D75 implementation/package identities and all final captures remain. Color-profile benchmark reports, inputs, comparisons and primary logs remain.

All player saves and backups, current packages, the VR-template smoke regression, public D73 QA payloads, toolchain/vendor inputs, performance/research baselines and primary verification summaries/logs are retained. Active audio/art work, D72/D80 evidence, accepted source assets and pending device checks are retained. No chat is archived or removed by this cleanup.

## Reproduction and future cleanup

### Music/generated-artifact retirement - D118, 2026-10-14

[Exact removal manifest](cleanup-2026-10-14-music.json), [preservation verification](cleanup-verification-2026-10-14-music.json) and [current packaged music proof](music-package-verification-2026-10-14.json) record the authorized603-file cleanup: superseded D82/D82-D92 review packages/launchers, unselected/temporary audio WAV/FLAC/stem/pre-master files, old audition ZIPs, completed initializer/Python/Unreal-test caches, six obsolete ignored music binaries and87 generated destruction imports. Removed17,701,359,568 logical bytes (16.486GiB); this is not measured free-space recovery.

All51,813 other inventoried artifact files preserve size/mtime and6266 protected hashes match, with no unexpected missing/changed protected files. Six preexisting long Playtests/QA paths were excluded and never targeted. Approved original masters/previews/source/native music, final Soundtrack3 album/download, editable art/audio, compact archived previews, feedback/manifests/logs/captures/decoder reports, all player files, current/public/smoke packages and profiling/foundation evidence remain. Maintained Windows header/audio-bulk hashes match all nine approved tracks; latest Quest identity/retained header receipt matches. Current packages are unchanged. Historical commands referring to retired binary/raw paths are dated evidence rather than current gates. Full inventory/protection/journal and one-time guarded scripts are under Artifacts/Maintenance-20261014-Music; do not replay or extend the completed plan.

### Completed-task retirement - D101, 2026-10-13

[Exact removal manifest](cleanup-candidates-2026-10-13.json) and [preservation verification](cleanup-verification-2026-10-13.json) record the user-approved375-file cleanup: seven superseded D89/D92/previous-default Windows bundles and four obsolete review launchers,8008099066 bytes (7.46GiB gross). All42980 other inventoried artifact paths remain and2604 saved-city/gallery hashes match. No unplanned missing files or changed protected hashes. Six preexisting long Playtests/QA paths were excluded from the initial size inventory and never targeted.

Primary logs, JSON receipts, screenshots, saves/profiles, current/public/Quest/smoke packages, active D94/audio/D88 work, profiling baselines and named acceptance/gallery evidence remain. Historical changelog/ENVIRONMENT binary paths may now be retired; consult this manifest. The complete inventory, exact deletion journal and one-time guarded scripts are in Artifacts/Maintenance-20261013. Do not replay or extend a completed manifest. Blender/fountain source backup is on Gitea in841b6af; the tracked [fountain sheet](studies/D100/PreviewSheet-01.png) and ArtSource/Blender/Studies/CityParkFountainPreview.blend survive local artifact retirement.

`Tools/Build-VisualChangelog.py` rebuilds the visual gallery using the bundled Python/Pillow and existing captures. Do not extend old dated deletion manifests. For a future cleanup, read the current task plan, reassess live references and supersession, create a new manifest, and verify protected inputs before and after removal. Age alone does not make an artifact disposable.

### Extended retirement - D102, 2026-10-13

[Exact removal manifest](cleanup-candidates-2026-10-13-extended.json) and [preservation verification](cleanup-verification-2026-10-13-extended.json) record the user-approved1765-file cleanup. Removed800 old QA extraction payload files,17 shared hash-fixture ZIP links,494 generated test Intermediate files,156 superseded UI screenshots and298 temporary Blender before/after exports. Original release archives and latest .4 extraction00b6d6f1 remain; frozen old scripts, normalized comparison receipts, primary logs, final captures, saves/configs, current/source/profiling/active evidence and every gallery source remain.

All41611 other inventoried artifact paths remain and2764 protected hashes match; no remaining targets, unexpected missing files or changed protected hashes. Ordinary single-link file content totals15751174086 bytes (14.67GiB). The additional7644745967 logical bytes of ZIP hard links removed only extra names; the public archive data still exists. These totals are not measured physical recovery. The inventory/proof/protection snapshots and exact journal/guarded one-time scripts are under Artifacts/Maintenance-20261013-Extended. Six preexisting long Playtests/QA paths were excluded from the initial inventory and never targeted. Completed manifests cannot be replayed.

The dated `Plan-CityArtifactCleanup.py`, `Clean-CityArtifacts.ps1` and `Verify-CityArtifactCleanup.py` preserve this maintenance procedure. The cleaner defaults to preflight and only removes the exact reviewed files with `-Apply`; a completed manifest cannot be replayed. It rejects paths outside Artifacts, reparse-point ancestry and changed hashes. It uses individual file removals and removes only empty directories.
