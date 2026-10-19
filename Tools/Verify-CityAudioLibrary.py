"""Verify D58 sample-library PCM exports, loop wraps and all review players."""
from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import sys

import numpy as np

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
module_spec = importlib.util.spec_from_file_location("d58_pcm_verifier", ROOT / "Tools/Verify-CityAudioProduction.py")
verify = importlib.util.module_from_spec(module_spec)
module_spec.loader.exec_module(verify)
auditions = verify.auditions
RATE = 48000


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pack", type=Path)
    options = parser.parse_args()
    pack = options.pack.resolve()
    manifest = json.loads((pack / "Manifest.json").read_text())
    validation = json.loads((pack / "Validation.json").read_text())
    config = manifest["config"]
    expected = {s["id"]: s for s in config["samples"]}
    assert set(manifest["assets"]) == set(expected) and len(expected) == len(config["samples"])
    assert len({r["masterSha256"] for r in manifest["assets"].values()}) == len(expected)
    ffmpeg, ffprobe = shutil.which("ffmpeg"), shutil.which("ffprobe")
    if not ffmpeg or not ffprobe:
        raise RuntimeError("FFmpeg and FFprobe are required")
    previews = {}
    seams = {}
    for identity, sample in expected.items():
        record = manifest["assets"][identity]
        count = round(sample["seconds"] * RATE)
        assert record["frames"] == count
        master = verify.pcm(pack / record["master"], 24, count)
        runtime = verify.pcm(pack / record["runtime"], 16, count)
        assert np.abs(master - runtime).max() < 4e-5, identity
        assert np.sqrt(np.mean(master ** 2)) > 1e-4, (identity, "Effect is silent")
        assert np.abs(master).max() < 10 ** (-8.9 / 20), identity
        previews[identity] = count
        if sample["loop"]:
            assert record["loopStartFrame"] == 0 and record["loopEndFrameExclusive"] == count
            local = round(.05 * RATE)
            slopes = np.concatenate([np.diff(runtime[:local]), np.diff(runtime[-local:])])
            curvatures = np.concatenate([np.diff(runtime[:local], 2), np.diff(runtime[-local:], 2)])
            wrap = np.concatenate([runtime[-2:], runtime[:2]])
            step = float(abs(runtime[0] - runtime[-1]))
            curvature = float(np.max(np.abs(np.diff(wrap, 2))))
            assert step <= np.max(np.abs(slopes)) * 1.1 + 2 / 32768, (identity, "Wrap step outlier")
            assert curvature <= np.max(np.abs(curvatures)) * 1.25 + 4 / 32768, (identity, "Wrap curvature outlier")
            seams[identity] = {"wrapStepPCM16": step, "wrapCurvaturePCM16": curvature, "status": "Pass"}
            previews[f"{identity}-Repeat"] = count * 2
        else:
            assert master[0] == 0 and master[-1] == 0 and runtime[0] == 0 and runtime[-1] == 0, identity
        measured = auditions.measure(ffmpeg, pack / record["runtime"])
        assert measured["input_tp"] <= config["truePeakCeilingDBTP"] + .1, identity
        if record["normalization"] == "Static gain, peak-ceiling limited":
            assert measured["input_i"] < sample["targetLUFS"] and abs(measured["input_tp"] - config["truePeakCeilingDBTP"]) <= .15, identity
        else:
            assert abs(measured["input_i"] - sample["targetLUFS"]) <= .35, identity
    for reel in manifest["categoryReels"].values():
        previews[reel["file"]] = reel["frames"]
        starts = [entry["startSeconds"] for entry in reel["sequence"]]
        assert starts == sorted(starts), reel
    for name, scene in manifest["scenes"].items():
        previews[name] = scene["frames"]
    preview_checks = {}
    for name, count in previews.items():
        wav = verify.pcm(pack / "Preview" / f"{name}.wav", 16, count)
        assert wav[0] == 0 and wav[-1] == 0, name
        mp3_path = pack / "Preview" / f"{name}.mp3"
        decoded = auditions.read_audio(ffmpeg, ffprobe, mp3_path)
        assert len(decoded) == count and np.isfinite(decoded).all() and np.abs(decoded).max() < .5012, name
        peak = auditions.measure(ffmpeg, mp3_path)["input_tp"]
        assert peak <= -8.0, (name, "Review encoding exceeded preview headroom", peak)
        preview_checks[name] = {"frames": count, "truePeakDBTP": peak}
    actual = {p.relative_to(pack).as_posix() for folder in ("Masters", "Runtime", "Preview") for p in (pack / folder).iterdir()}
    assert actual == set(validation["sha256"])
    for name, digest in validation["sha256"].items():
        assert auditions.sha(pack / name) == digest, name
    for name, digest in manifest["sourceSha256"].items():
        assert auditions.sha(pack / "Sources" / name) == digest, name
    report = {
        "status": "Pass", "uniqueSamples": len(expected), "PCMAssetFilesChecked": len(expected) * 2,
        "loopWrapsChecked": len(seams), "oneShotsChecked": len(expected) - len(seams),
        "decodedReviewMP3s": len(previews), "outputHashesChecked": len(actual),
        "sourceSnapshotsChecked": len(manifest["sourceSha256"]),
        "runtimePCMBytes": manifest["runtimePCMBytes"], "loopChecks": seams, "previewChecks": preview_checks,
        "scope": "Source quality/encoding checks only. Sample selection/naturalness/mix and actual Unreal/device/cost remain pending.",
    }
    (pack / "IndependentVerification.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"D58_LIBRARY_INDEPENDENT_QA_PASS samples={len(expected)} loops={len(seams)} previews={len(previews)} hashes={len(actual)}")


if __name__ == "__main__":
    main()
