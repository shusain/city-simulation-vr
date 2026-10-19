"""Independently verify exported D58 PCM assets, wraps and compressed previews."""
from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import wave

import numpy as np

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
module_spec = importlib.util.spec_from_file_location("d58_production", ROOT / "Tools/Render-CityAudioProduction.py")
production = importlib.util.module_from_spec(module_spec)
module_spec.loader.exec_module(production)
auditions = production.auditions
RATE = 48000


def pcm(path, bits, expected_frames):
    with wave.open(str(path), "rb") as recording:
        assert recording.getnchannels() == 1 and recording.getframerate() == RATE, path
        assert recording.getsampwidth() == bits // 8 and recording.getnframes() == expected_frames, path
        data = recording.readframes(recording.getnframes())
    if bits == 16:
        values = np.frombuffer(data, dtype="<i2").astype(np.float64)
    else:
        raw = np.frombuffer(data, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        values = raw[:, 0] + (raw[:, 1] << 8) + (raw[:, 2] << 16)
        values = np.where(values & 0x800000, values - 0x1000000, values).astype(np.float64)
    samples = values / (2 ** (bits - 1))
    assert np.isfinite(samples).all() and np.abs(samples).max() < 0.5012, path
    return samples


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pack", type=Path)
    options = parser.parse_args()
    pack = options.pack.resolve()
    manifest = json.loads((pack / "Manifest.json").read_text())
    validation = json.loads((pack / "Validation.json").read_text())
    config = manifest["config"]
    for role, events in manifest["illustrativeAccentEventsSeconds"].items():
        assert events[1] - events[0] >= config["runtimeContract"]["accentCooldownSeconds"][role], role
    ffmpeg, ffprobe = shutil.which("ffmpeg"), shutil.which("ffprobe")
    if not ffmpeg or not ffprobe:
        raise RuntimeError("FFmpeg and FFprobe are required")
    expected = {}
    expected_previews = {}
    blend = round(config["transitionBlendMilliseconds"] * RATE / 1000)
    for role in config["voices"]:
        name = role["id"]
        for phase in ("Intro", "Loop", "Outro"):
            expected[f"{name}-{phase}"] = round(role[phase.lower() + "Seconds"] * RATE)
        expected_previews[f"{name}-Lifecycle"] = (
            expected[name + "-Intro"] + expected[name + "-Outro"]
            + expected[name + "-Loop"] * role["previewLoops"] - blend * 2
        )
        expected_previews[f"{name}-Wrap"] = min(round(3 * RATE), expected[name + "-Loop"] // 2) * 2
    for accent in config["accents"]:
        expected[accent["id"]] = round(accent["durationSeconds"] * RATE)
        expected_previews[accent["id"]] = expected[accent["id"]]
    expected_previews.update({"Police-AccentMix": 16 * RATE, "Fire-AccentMix": 16 * RATE})
    assert set(expected) == set(manifest["assets"]), "Unexpected or missing production asset"
    decoded = {}
    quantization, seams = {}, {}
    for name, count in expected.items():
        record = manifest["assets"][name]
        assert record["frames"] == count, name
        master = pcm(pack / record["master"], 24, count)
        runtime = pcm(pack / record["runtime"], 16, count)
        error = float(np.abs(master - runtime).max())
        assert error < 4e-5, (name, "PCM16 conversion differs", error)
        quantization[name] = error
        decoded[name] = master
        if name.endswith("-Loop"):
            assert record["loopStartFrame"] == 0 and record["loopEndFrameExclusive"] == count
            local = round(0.05 * RATE)
            first = np.concatenate([np.diff(runtime[:local]), np.diff(runtime[-local:])])
            second = np.concatenate([np.diff(runtime[:local], 2), np.diff(runtime[-local:], 2)])
            wrapped = np.concatenate([runtime[-2:], runtime[:2]])
            wrap_step = float(abs(runtime[0] - runtime[-1]))
            curvature = float(np.max(np.abs(np.diff(wrapped, 2))))
            assert wrap_step <= float(np.max(np.abs(first))) * 1.1 + 2 / 32768, (name, "Wrap step outlier")
            assert curvature <= float(np.max(np.abs(second))) * 1.25 + 4 / 32768, (name, "Wrap curvature outlier")
            seams[name] = {"PCM16WrapStep": wrap_step, "PCM16WrapCurvature": curvature, "adjacentSampleChecks": "Pass"}
        elif name.endswith("-Intro"):
            assert master[0] == 0 and runtime[0] == 0, name
        elif name.endswith("-Outro") or name in {a["id"] for a in config["accents"]}:
            assert master[-1] == 0 and runtime[-1] == 0, name
    # Verify selected Town G's steady paired tones and quiet lower-octave layer
    # from exported PCM, independently of source-code/config assertions.
    town = decoded["Town-Loop"][5 * RATE:6 * RATE]
    spectrum = np.abs(np.fft.rfft(town * np.hanning(len(town))))
    frequencies = np.fft.rfftfreq(len(town), 1 / RATE)
    tones = {}
    for target in (270, 324, 540, 648):
        bins = np.where(abs(frequencies - target) <= 3)[0]
        peak_bin = bins[np.argmax(spectrum[bins])]
        assert abs(float(frequencies[peak_bin]) - target) <= 1, target
        tones[str(target)] = float(spectrum[peak_bin])
    for low, high in ((270, 540), (324, 648)):
        ratio = tones[str(low)] / tones[str(high)]
        assert 0.18 <= ratio <= 0.28, (low, high, ratio)
    compressed = {}
    for name, count in expected_previews.items():
        preview_wav = pack / "Preview" / f"{name}.wav"
        preview_pcm = pcm(preview_wav, 16, count)
        assert preview_pcm[0] == 0 and preview_pcm[-1] == 0, name
        mp3 = pack / "Preview" / f"{name}.mp3"
        samples = auditions.read_audio(ffmpeg, ffprobe, mp3)
        assert len(samples) == count and np.abs(samples).max() < 0.5012, name
        metrics = auditions.measure(ffmpeg, mp3)
        assert metrics["input_tp"] <= config["truePeakCeilingDBTP"] + 0.1, (name, metrics)
        compressed[name] = {"frames": len(samples), "truePeakDBTP": metrics["input_tp"]}
    actual_files = {p.relative_to(pack).as_posix() for folder in ("Masters", "Runtime", "Preview") for p in (pack / folder).iterdir()}
    assert actual_files == set(validation["sha256"]), "Unlisted/missing audio output"
    for filename, digest in validation["sha256"].items():
        assert auditions.sha(pack / filename) == digest, filename
    for filename, digest in manifest["sourceSha256"].items():
        assert auditions.sha(pack / "Sources" / filename) == digest, filename
    report = {
        "status": "Pass", "productionAssets": len(expected), "PCMFilesVerified": len(expected) * 2,
        "previewWAVsVerified": len(expected_previews), "compressedPreviewsDecoded": len(compressed),
        "outputHashesVerified": len(actual_files), "sourceSnapshotsVerified": len(manifest["sourceSha256"]),
        "loopBoundaryChecks": seams, "PCM16ConversionMaxError": quantization,
        "TownGSteadyToneFFTAmplitudes": tones, "compressedPreviewChecks": compressed,
        "runtimePCMBytes": manifest["runtimePCMBytes"],
        "acceptanceScope": "Technical source-export checks; listening, actual Unreal playback/event/phase/lifecycle/mix/device/cost remain pending",
    }
    (pack / "IndependentVerification.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"D58_PRODUCTION_INDEPENDENT_QA_PASS assets={len(expected)} previews={len(compressed)} hashes={len(actual_files)}")


if __name__ == "__main__":
    main()
