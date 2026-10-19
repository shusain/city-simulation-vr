"""Stage accepted D58 WAVs outside the Unreal project; never launch an Editor."""
from __future__ import annotations

import sys
from pathlib import Path
sys.path.insert(0, str(next(p for p in Path(__file__).resolve().parents if (p / "CityToolPaths.py").is_file())))
from CityToolPaths import repository_root

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil
import wave

ROOT = repository_root(__file__)
STAGE = ROOT / "AudioSource/Soundscape/Unreal"
MANIFEST = STAGE / "Manifest.json"
ASSET_ROOT = "/Game/CitySim/Audio/Soundscape"
PACKS = {
    "Sirens": ROOT / "Artifacts/D58/Production-20261011-v2",
    "Library": ROOT / "Artifacts/D58/Library-20261012-v8",
}
GROUPS = {
    "Park and water": "Ambience/Park",
    "Industry": "Ambience/Industry",
    "City activity": "Ambience/City",
    "Day and night wildlife": "Ambience/Wildlife",
    "Weather": "Ambience/Weather",
    "Activity and disasters": "Effects/Activity",
    "Notifications": "UI",
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def inside(base, relative):
    # Reject absolute/parent paths instead of permitting manifest path escapes.
    path = Path(relative)
    if path.is_absolute() or ".." in path.parts or ":" in str(path):
        raise ValueError(f"Unsafe relative source path: {relative}")
    resolved = (base / path).resolve()
    if not resolved.is_relative_to(base.resolve()):
        raise ValueError(f"Source escapes staging directory: {relative}")
    return resolved


def pcm_header(path, frames):
    with wave.open(str(path), "rb") as recording:
        actual = (recording.getnchannels(), recording.getframerate(), recording.getsampwidth(), recording.getnframes())
        if actual != (1, 48000, 2, frames) or recording.getcomptype() != "NONE":
            raise ValueError(f"Unexpected PCM format/length: {path} {actual}")
        if len(recording.readframes(frames)) != frames * 2:
            raise ValueError(f"Truncated PCM payload: {path}")


def validate_stage(manifest_path=MANIFEST):
    manifest_path = Path(manifest_path).resolve()
    if manifest_path != MANIFEST.resolve():
        raise ValueError("Only the dedicated D58 source staging directory is supported")
    manifest = read_json(manifest_path)
    entries = manifest["assets"]
    traffic = manifest.get("trafficRevision")
    expected_count = 54 if traffic else 50
    if manifest["schema"] != 1 or manifest["task"] != "D58" or len(entries) != expected_count:
        raise ValueError("Expected the accepted 14-siren plus 36-library source bank")
    identities, packages, paths = set(), set(), set()
    total = 0
    for entry in entries:
        identity, package = entry["id"], entry["package"]
        if not re.fullmatch(r"[A-Za-z]+(?:-[A-Za-z]+)*", identity) or identity in identities:
            raise ValueError(f"Invalid/duplicate audio identity: {identity}")
        if not re.fullmatch(r"/Game/CitySim/Audio/Soundscape/(?:[A-Za-z]+/)+SW_[A-Za-z_]+", package) or package in packages:
            raise ValueError(f"Invalid/duplicate native target: {package}")
        if package.rsplit("/", 1)[-1] != entry["assetName"]:
            raise ValueError(f"Inconsistent native asset name: {identity}")
        path = inside(STAGE, entry["file"])
        if path in paths or path.suffix != ".wav" or sha(path) != entry["sha256"]:
            raise ValueError(f"Missing/changed/duplicate staged source: {identity}")
        pcm_header(path, entry["frames"])
        if (entry["sampleRate"], entry["channels"], entry["bits"]) != (48000, 1, 16):
            raise ValueError(f"Unexpected format declaration: {identity}")
        if abs(entry["seconds"] - entry["frames"] / 48000) > 1e-8:
            raise ValueError(f"Unexpected frame duration: {identity}")
        if entry["loop"] and (entry["loopStartFrame"], entry["loopEndFrameExclusive"]) != (0, entry["frames"]):
            raise ValueError(f"Unexpected loop bounds: {identity}")
        if path.stat().st_size != entry["bytes"]:
            raise ValueError(f"Unexpected source byte count: {identity}")
        identities.add(identity)
        packages.add(package)
        paths.add(path)
        total += entry["bytes"]
    if paths != set((STAGE / "Waves").rglob("*.wav")):
        raise ValueError("Unlisted WAV present in source staging")
    if traffic and {e['id'] for e in entries if e['bank']=='Traffic'} != {'Car-Idle','Car-Driving','Truck-Idle','Truck-Driving'}:
        raise ValueError("Unexpected steady traffic source identities")
    if total != manifest["sourceWAVBytes"] or sum(e["loop"] for e in entries) != (25 if traffic else 21):
        raise ValueError("Incorrect source size or loop count")
    for relative, digest in manifest["acceptanceSnapshotSha256"].items():
        if sha(inside(STAGE, relative)) != digest:
            raise ValueError(f"Changed acceptance snapshot: {relative}")
    return manifest


def stage():
    if MANIFEST.exists():
        validate_stage()
        print("D58_AUDIO_STAGE_ALREADY_VALID; existing sources preserved")
        return
    if STAGE.exists() and any(STAGE.iterdir()):
        raise ValueError("Staging directory contains files without its manifest; preserve and inspect them")
    selection = read_json(ROOT / "AudioSource/Soundscape/selections.json")
    if selection.get("cricketAcceptance", {}).get("reviewedPack") != "Artifacts/D58/Library-20261011-v7":
        raise ValueError("Current accepted cricket source bank does not match")
    entries, provenance, source_files = [], {}, []
    for bank, pack in PACKS.items():
        source_manifest = read_json(pack / "Manifest.json")
        validation = read_json(pack / "Validation.json")
        independent = read_json(pack / "IndependentVerification.json")
        if validation["status"] != "Pass" or independent["status"] != "Pass":
            raise ValueError(f"Source pack lacks passing technical evidence: {bank}")
        if len(source_manifest["assets"]) != (14 if bank == "Sirens" else 36):
            raise ValueError(f"Unexpected accepted bank size: {bank}")
        provenance[bank] = {
            "pack": pack.relative_to(ROOT).as_posix(),
            "manifestSha256": sha(pack / "Manifest.json"),
            "independentVerificationSha256": sha(pack / "IndependentVerification.json"),
        }
        for identity, record in source_manifest["assets"].items():
            source = inside(pack, record["runtime"])
            expected_hash = record["runtimeSha256"]
            if sha(source) != expected_hash or validation["sha256"][record["runtime"]] != expected_hash:
                raise ValueError(f"Accepted runtime source changed: {identity}")
            pcm_header(source, record["frames"])
            folder = "Sirens" if bank == "Sirens" else GROUPS[record["group"]]
            relative = f"Waves/{folder}/{source.name}"
            loop = "loopStartFrame" in record
            entry = {
                "id": identity, "bank": bank, "file": relative,
                "sourcePack": provenance[bank]["pack"], "sourceRuntimeFile": record["runtime"],
                "sha256": expected_hash, "bytes": source.stat().st_size,
                "assetName": source.stem, "package": f"{ASSET_ROOT}/{folder}/{source.stem}",
                "sampleRate": 48000, "channels": 1, "bits": 16,
                "frames": record["frames"], "seconds": record["seconds"], "loop": loop,
                "integratedLUFS": record["integratedLUFS"], "truePeakDBTP": record["truePeakDBTP"],
                "assetVolume": 1.0, "assetPitch": 1.0,
            }
            if loop:
                entry.update(loopStartFrame=0, loopEndFrameExclusive=record["frames"])
            entries.append(entry)
            source_files.append((source, inside(STAGE, relative)))
    if len({e["id"] for e in entries}) != 50 or len({e["package"] for e in entries}) != 50:
        raise ValueError("Audio bank contains colliding identities or native targets")
    # Only after every original is verified do we write outside the .uproject tree.
    STAGE.mkdir(parents=True, exist_ok=True)
    for source, destination in source_files:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    (STAGE / ".gitattributes").write_text("*.wav filter=lfs diff=lfs merge=lfs -text\n", encoding="utf-8")
    snapshots = {}
    for name in ("selections.json", "library-presets.json", "production-presets.json"):
        destination = STAGE / "Acceptance" / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / "AudioSource/Soundscape" / name, destination)
        snapshots[destination.relative_to(STAGE).as_posix()] = sha(destination)
    manifest = {
        "schema": 1, "task": "D58", "createdUTC": datetime.now(timezone.utc).isoformat(),
        "scope": "Accepted source WAV staging only; no Unreal assets, playback or cook integration yet",
        "provenance": "Original seeded SuperCollider synthesis; no third-party recordings or sampled references",
        "sourcePacks": provenance, "acceptanceSnapshotSha256": snapshots,
        "assetRoot": ASSET_ROOT, "sourceWAVBytes": sum(e["bytes"] for e in entries), "assets": entries,
        "nativePolicy": {
            "replacement": "Create new D58 SoundWave packages only; preserve existing D57 MetaSound and all art/maps/config/C++",
            "levels": "Imported volume/pitch stay 1.0; preserve source gains and rain intensity differences",
            "loops": "21 whole-file loop waves; 29 non-looping one-shots/phases. Runtime or MetaSound playback may explicitly control looping",
            "defaults": "Retain engine compression/loading/class/attenuation defaults until native playback/cook/loop quality gates",
            "integration": "No world placement, event routing, MetaSound graph replacement, always-cook rules or package references in this staging slice",
            "window": "Native importer/launcher must refuse another task's recorded exclusive window; do not compete with D83",
        },
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    validate_stage()
    print(f"D58_AUDIO_SOURCE_STAGE_PASS assets=50 loops=21 oneShots=29 bytes={manifest['sourceWAVBytes']}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Verify the staging bank without copying or launching Unreal")
    options = parser.parse_args()
    if options.check:
        manifest = validate_stage()
        print(f"D58_AUDIO_SOURCE_STAGE_CHECK_PASS assets={len(manifest['assets'])} bytes={manifest['sourceWAVBytes']}")
    else:
        stage()


if __name__ == "__main__":
    main()
