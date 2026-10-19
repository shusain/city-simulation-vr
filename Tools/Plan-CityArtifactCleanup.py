"""Inventory the explicitly reviewed 2026-10-11 cleanup; this script never deletes.

Retain this manifest even after removal so old changelog paths remain explainable.
Actual removal uses Clean-CityArtifacts.ps1 and its complete preflight.
"""

from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
import hashlib
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ARTIFACTS = ROOT / "Artifacts"
OUT = ROOT / "docs/visual-changelog"
MANIFEST = OUT / "cleanup-2026-10-11.json"
CLOSED_LOG_TASKS = {"D40", "D41", "D42", "D43", "D44", "D45", "D47", "D48",
                    "D49", "D50", "D52", "D54", "D55", "D59"}
OBSOLETE_PACKAGES = {"D75WindowsSandbox", "ColorPerfPreview"}
OBSOLETE_HELPERS = {"D75-Start-Desktop.ps1", "D75-Start-PCVR.ps1", "D75-Test-Desktop-Isolated.ps1"}


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def relative(path):
    return path.relative_to(ROOT).as_posix()


def record(path):
    stat = path.stat()
    return dict(path=relative(path), bytes=stat.st_size, sha256=sha(path),
                modified_utc=datetime.fromtimestamp(stat.st_mtime, timezone.utc).isoformat())


def plan():
    if MANIFEST.exists():
        raise RuntimeError("Cleanup manifest already exists; do not overwrite its original inventory")
    gallery = json.loads((OUT / "gallery.json").read_text(encoding="utf-8"))
    gallery_sources = {i["source"] for i in gallery["images"]}
    # Do not retire a path referred to by a current plan, maintained tool or formal profiling report.
    references = []
    for base in [ROOT / "Tools", ROOT / "docs"]:
        for path in base.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in {".md", ".py", ".ps1", ".json", ".cpp", ".h"}:
                continue
            if path == Path(__file__) or path.name in {"Build-VisualChangelog.py", "Clean-CityArtifacts.ps1", "CHANGELOG.md"}:
                continue
            if OUT in path.parents or "archive" in path.parts:
                continue
            references.append(path.read_text(encoding="utf-8", errors="replace").lower())
    live_references = "\n".join(references)
    for package in OBSOLETE_PACKAGES:
        # D75 is explicitly superseded in the live card; ColorPerfPreview has no live references.
        if package == "ColorPerfPreview" and package.lower() in live_references:
            raise RuntimeError("Unexpected live ColorPerfPreview reference")
    if "isolated d75 helpers are superseded" not in live_references:
        raise RuntimeError("D75 supersession evidence missing")
    final_exe = ARTIFACTS / "PCVRSandbox/Windows/CitySimVR/Binaries/Win64/CitySimVR.exe"
    expected = "9b723b4e4b080d62b2bea438dcd15f8f1339388107b9b545bf3ca9fadc796eb9"
    if sha(final_exe) != expected:
        raise RuntimeError("Current Windows package differs from the verified D79/D71 replacement")
    tracked = set(subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT).decode().split("\0"))
    candidates = []
    skipped = []
    cutoff = datetime(2026, 10, 9, tzinfo=timezone.utc).timestamp()
    for path in ARTIFACTS.rglob("*"):
        if not path.is_file():
            continue
        rel = relative(path)
        parts = path.relative_to(ARTIFACTS).parts
        category = reason = None
        if parts[0] in OBSOLETE_PACKAGES:
            category = "superseded-package"
            reason = ("D75 isolated package/helpers explicitly superseded by maintained PCVRSandbox in the D75 card"
                      if parts[0] == "D75WindowsSandbox" else
                      "Unreferenced old color-profile packaging scratch; final captures/reports and all benchmark baselines retained")
        elif len(parts) == 1 and path.name in OBSOLETE_HELPERS:
            category = "superseded-launch-helper"
            reason = "Path-only D75 helper superseded by maintained Tools launch/test helpers"
        elif parts[0] == "Preview" and len(parts) == 2 and path.suffix == ".png":
            if path.name.startswith(("D59-Readability-", "D59-Refinement-", "D49-EntryMenu-")):
                category = "superseded-preview"
                reason = "Earlier closed HUD/menu round; selected milestones consolidated, final native captures retained"
        elif parts[0] in {"Diagnostics", "Preview"} and len(parts) == 2 and path.suffix == ".log":
            task = re.match(r"^(D\d+)-", path.name)
            if task and task[1] in CLOSED_LOG_TASKS and path.stat().st_mtime < cutoff:
                if re.search(r"-backup-\d{4}[.-]\d{2}[.-]\d{2}", path.name):
                    category = "old-log-backup"
                    reason = "Automatic dated log backup from a closed implementation round; primary logs retained"
                elif path.name.endswith("-console.log"):
                    primary = path.with_name(path.name.replace("-console.log", ".log"))
                    if primary.is_file():
                        category = "redundant-console-log"
                        reason = "Console transcript from a closed round; corresponding full primary log retained"
        if category is None:
            continue
        if rel in tracked:
            raise RuntimeError(f"Tracked file in cleanup plan: {rel}")
        if rel in gallery_sources or (category not in {"superseded-package", "superseded-launch-helper"}
                                      and path.name.lower() in live_references):
            skipped.append(dict(path=rel, reason="Retained gallery source or live reference"))
            continue
        entry = dict(**record(path), category=category, reason=reason)
        if path.suffix == ".log":
            content = path.read_text(encoding="utf-8", errors="replace")
            entry["log_lines"] = len(content.splitlines())
            entry["reported_test_results"] = re.findall(r"Tests? (?:Succeeded|Failed).*", content)[-6:]
            entry["diagnostic_markers"] = dict(error=len(re.findall(r"\berror\b", content, re.I)),
                                                 fatal=len(re.findall(r"\bfatal\b", content, re.I)))
            entry["markers_are_not_a_pass_verdict"] = True
        candidates.append(entry)
    candidates.sort(key=lambda e: e["path"])
    # Byte-for-byte guard for every saved-city copy; no age-based save removal.
    save_paths = []
    for path in ARTIFACTS.rglob("*"):
        if path.is_file() and path.suffix.lower() in {".json", ".bak", ".sav"}:
            if any("save" in part.lower() or part.lower() == "cities" for part in path.parts):
                save_paths.append(path)
    for path in (ROOT / "CitySimVR/Saved").rglob("*"):
        if path.is_file() and any(part.lower() in {"savegames", "cities"} for part in path.parts):
            save_paths.append(path)
    guard_paths = set(save_paths)
    for name in ["D72-Dimensions", "D72-Terrain", "D72-ExportGuard", "D80", "D83", "D58"]:
        guard_paths.update(p for p in (ARTIFACTS / name).rglob("*") if p.is_file())
    for path in (ARTIFACTS / "Benchmarks").rglob("*"):
        if path.is_file() and path.suffix.lower() in {".json", ".csv"}:
            guard_paths.add(path)
    guard_paths.update([final_exe, ARTIFACTS / "QuestSandbox/Android_ASTC/CitySimVR-arm64.apk",
                        ARTIFACTS / "D79-MetalD71/verification.json"])
    guard_paths.update(ROOT / rel for rel in gallery_sources)
    # Full source/content hashes are separate from docs being edited by other chats.
    for base in ["ArtSource", "AudioSource", "CitySimVR/Source", "CitySimVR/Content"]:
        guard_paths.update(p for p in (ROOT / base).rglob("*") if p.is_file())
    protected = [record(p) for p in sorted(guard_paths) if p.is_file()]
    candidate_paths = {e["path"] for e in candidates}
    assert not candidate_paths.intersection(e["path"] for e in protected)
    counts = Counter(e["category"] for e in candidates)
    result = dict(task="D84", date="2026-10-11", status="planned", scope="Explicit historical artifact cleanup",
                  total_files=len(candidates), total_bytes=sum(e["bytes"] for e in candidates),
                  categories=dict(counts), deletion_candidates=candidates,
                  retained_live_references=skipped, protected_files=protected,
                  supersession_evidence=["docs/DEVELOPMENT_TASKS.md D75: maintained archives and standard launchers supersede isolated D75",
                                         "Artifacts/PCVRSandbox current D79/D71 executable hash verified",
                                         "ColorPerfPreview has no maintained plan/tool/profiling references"],
                  retained_policy=["All player saves/backups", "Active art/audio work and accepted source inputs",
                                   "D72/D80 implementation evidence", "All profiling baselines and research packages",
                                   "Current packages, D73 public/extracted QA payloads, smoke regression and toolchain",
                                   "Primary test/build/runtime logs and JSON summaries", "Every gallery source capture"],
                  all_artifact_paths_before=[relative(p) for p in ARTIFACTS.rglob("*") if p.is_file()])
    MANIFEST.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(dict(files=result["total_files"], bytes=result["total_bytes"], categories=dict(counts),
                          protected_files=len(protected), skipped_live_references=len(skipped))))


if __name__ == "__main__":
    plan()
