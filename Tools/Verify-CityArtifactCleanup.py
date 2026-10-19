"""Verify the cleanup against its pre-removal inventory and protected hashes."""

from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/visual-changelog"


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def verify():
    manifest_path = OUT / "cleanup-2026-10-11.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    assert manifest["status"] == "removed-awaiting-verification"
    candidates = {e["path"] for e in manifest["deletion_candidates"]}
    remaining_targets = [p for p in sorted(candidates) if (ROOT / p).exists()]
    missing_unplanned = [p for p in manifest["all_artifact_paths_before"]
                        if p not in candidates and not (ROOT / p).is_file()]
    changed_protected = [e["path"] for e in manifest["protected_files"]
                         if not (ROOT / e["path"]).is_file() or sha(ROOT / e["path"]) != e["sha256"]]
    concurrent_evidence = json.loads((OUT / "concurrent-work-evidence.json").read_text())
    reviewed = {e["path"]: e for e in concurrent_evidence["reviewedConcurrentChanges"]}
    assert not candidates.intersection(reviewed)
    unexpected_changes = [p for p in changed_protected if p not in reviewed or not (ROOT / p).is_file()]
    protected_by_path = {e["path"]: e for e in manifest["protected_files"]}
    concurrent_updates = [dict(**reviewed[p], before_sha256=protected_by_path[p]["sha256"],
                               after_sha256=sha(ROOT / p)) for p in changed_protected if p in reviewed]
    gallery = json.loads((OUT / "gallery.json").read_text(encoding="utf-8"))
    changed_gallery = [e["source"] for e in gallery["images"]
                       if sha(ROOT / e["source"]) != e["source_sha256"] or
                       sha(OUT / e["output"]) != e["output_sha256"]]
    journal = [json.loads(line) for line in
               (ROOT / "Artifacts/Maintenance-20261011/deletion-journal.jsonl").read_text(encoding="utf-8-sig").splitlines()]
    assert len(journal) == len(candidates) == manifest["removed_files"]
    assert {e["path"] for e in journal} == candidates
    assert sum(e["bytes"] for e in journal) == manifest["total_bytes"] == manifest["removed_bytes"]
    browser = json.loads((ROOT / "Artifacts/Maintenance-20261011/gallery-verification.json").read_text())
    assert browser["milestones"] == len(gallery["milestones"])
    assert browser["decodedImages"] == len(gallery["images"])
    report = dict(task="D84", date="2026-10-11", removed_files=len(candidates),
                  removed_bytes=manifest["removed_bytes"], protected_hashes_checked=len(manifest["protected_files"]),
                  noncandidate_artifact_paths_checked=len(manifest["all_artifact_paths_before"]) - len(candidates),
                  remaining_targets=remaining_targets, missing_unplanned_artifacts=missing_unplanned,
                  unchanged_protected_hashes=len(manifest["protected_files"]) - len(changed_protected),
                  changed_protected_files=changed_protected, reviewed_concurrent_updates=concurrent_updates,
                  unexplained_protected_changes=unexpected_changes, changed_gallery_files=changed_gallery,
                  gallery_milestones=len(gallery["milestones"]), gallery_captures=len(gallery["images"]),
                  gallery_browser_checks=browser)
    report["pass"] = not any([remaining_targets, missing_unplanned, unexpected_changes, changed_gallery])
    (OUT / "cleanup-verification-2026-10-11.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if not report["pass"]:
        raise RuntimeError(f"Preservation check differs; inspect cleanup verification: {report}")
    manifest["status"] = "verified-complete"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    verify()
