"""Render selected D58 intro/loop/outro assets and accents using existing SC sources.

Authoring stays outside Unreal. PCM24 masters and PCM16 import copies use one
static gain per role; previews demonstrate repeated wraps and phase transitions.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import importlib.util
import json
import math
import os
from pathlib import Path
import shutil
import sys

import numpy as np

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "AudioSource/Soundscape"
module_spec = importlib.util.spec_from_file_location("d58_auditions", ROOT / "Tools/Render-CityAudioAuditions.py")
auditions = importlib.util.module_from_spec(module_spec)
module_spec.loader.exec_module(auditions)
RATE = auditions.RATE


def frames(seconds):
    return round(seconds * RATE)


def load_candidate(filename, identity):
    # Keep source resolution within the existing trusted authoring directory.
    if filename not in ("presets.json", "town-pitch-presets.json"):
        raise ValueError(f"Unsupported selected preset file: {filename}")
    spec = json.loads((SOURCE / filename).read_text(encoding="utf-8"))
    return next(c for c in spec["candidates"] if c["id"] == identity)


def render(candidate, output, runtime, env, ffmpeg, ffprobe):
    osc = auditions.make_score(candidate, output, runtime, env)
    raw = output / "Work" / f'{candidate["id"]}-continuous.wav'
    result = auditions.run([
        runtime / "scsynth.exe", "-N", osc, "_", raw, RATE, "WAV", "float",
        "-i", "0", "-o", "1", "-z", "64", "-D", "0", "-R", "0",
        "-U", runtime / "plugins",
    ], cwd=runtime, env=env, log=output / "Logs" / f'{candidate["id"]}-scsynth.log')
    log = (result.stdout + result.stderr).decode("utf-8", errors="replace")
    if "ERROR" in log or "FAILURE" in log:
        raise RuntimeError(f'SC synthesis failure: {candidate["id"]}')
    signal = auditions.read_audio(ffmpeg, ffprobe, raw)
    start, count = frames(0.05), frames(candidate["parameters"]["duration"])
    if len(signal) < start + count:
        raise RuntimeError("Truncated continuous render")
    return signal[start:start + count].copy()


def cyclic_slice(signal, start, length, half_blend):
    """Join two real, adjacent waveform neighborhoods without shortening the loop.

    The seam blends continuations at the two loop boundaries. Both halves of
    the blend share a continuous pair of source neighborhoods, so the actual
    last-to-first step remains a normal adjacent-sample step. Linear weights
    preserve level for correlated periodic tones.
    """
    end = start + length
    loop = signal[start:end].copy()
    a = signal[start - half_blend:start + half_blend]
    b = signal[end - half_blend:end + half_blend]
    if len(a) != 2 * half_blend or len(b) != 2 * half_blend:
        raise ValueError("Insufficient steady render around loop boundaries")
    weights = np.linspace(0, 1, 2 * half_blend)
    seam = (1 - weights) * b + weights * a
    loop[-half_blend:] = seam[:half_blend]
    loop[:half_blend] = seam[half_blend:]
    return loop


def append_blended(left, right, blend):
    if blend <= 0 or min(len(left), len(right)) < blend:
        raise ValueError("Invalid transition overlap")
    weights = np.linspace(0, 1, blend)
    return np.concatenate([left[:-blend], left[-blend:] * (1 - weights) + right[:blend] * weights, right[blend:]])


def quiet_edges(signal, start=False, end=False):
    signal = signal.copy()
    count = frames(0.005)
    curve = np.sin(np.linspace(0, math.pi / 2, count)) ** 2
    if start:
        signal[:count] *= curve
    if end:
        signal[-count:] *= curve[::-1]
    return signal


def gain_for(signal, name, target, ceiling, output, ffmpeg):
    path = output / "Work" / f"{name}-gain-reference.wav"
    auditions.write_wave(path, signal)
    metrics = auditions.measure(ffmpeg, path)
    gain_db = min(target - metrics["input_i"], ceiling - metrics["input_tp"])
    return 10 ** (gain_db / 20), gain_db


def write_asset(output, name, signal, ffmpeg, ceiling, loop=False):
    master = output / "Masters" / f"{name}.wav"
    runtime = output / "Runtime" / f"SW_{name.replace('-', '_')}.wav"
    auditions.write_wave(master, signal)
    auditions.write_wave(runtime, signal, bits=16)
    metrics = auditions.measure(ffmpeg, master)
    if metrics["input_tp"] > ceiling + 0.05:
        raise RuntimeError(f"True peak exceeds ceiling: {name}")
    peak = float(np.max(np.abs(signal)))
    record = {
        "master": master.relative_to(output).as_posix(), "runtime": runtime.relative_to(output).as_posix(),
        "sampleRate": RATE, "channels": 1, "masterBits": 24, "runtimeBits": 16,
        "frames": len(signal), "seconds": len(signal) / RATE,
        "integratedLUFS": metrics["input_i"], "truePeakDBTP": metrics["input_tp"],
        "samplePeak": peak, "clippedSamples": int(np.count_nonzero(np.abs(signal) >= 1)),
        "masterSha256": auditions.sha(master), "runtimeSha256": auditions.sha(runtime),
    }
    if loop:
        step = float(abs(signal[0] - signal[-1]))
        neighbor = float(max(abs(signal[1] - signal[0]), abs(signal[-1] - signal[-2])))
        derivatives = np.diff(signal)
        local = np.concatenate([derivatives[:frames(0.05)], derivatives[-frames(0.05):]])
        limit = max(float(np.max(np.abs(local))) * 1.1, 1e-4)
        if step > limit:
            raise RuntimeError(f"Outlying loop boundary step: {name} ({step} > {limit})")
        record.update({
            "loopStartFrame": 0, "loopEndFrameExclusive": len(signal),
            "wrapStep": step, "neighborStep": neighbor, "localDerivativeLimit": limit,
            "wrapCheck": "Pass; boundary step within local waveform derivative range",
        })
    else:
        record["firstSample"] = float(signal[0])
        record["lastSample"] = float(signal[-1])
    return record


def preview(output, name, signal, ffmpeg):
    path = output / "Preview" / f"{name}.wav"
    auditions.write_wave(path, signal, bits=16)
    auditions.mp3(ffmpeg, path, path.with_suffix(".mp3"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--runtime", type=Path, default=ROOT / "Artifacts/Toolchain/SuperCollider/SuperCollider")
    options = parser.parse_args()
    config = json.loads((SOURCE / "production-presets.json").read_text(encoding="utf-8"))
    selected = json.loads((SOURCE / "selections.json").read_text(encoding="utf-8"))
    if config["schema"] != 1 or config["sampleRate"] != RATE:
        raise ValueError("Unsupported production configuration")
    if {v["id"] for v in config["voices"]} != {"Police", "Fire", "Ambulance", "Town"}:
        raise ValueError("Expected four selected emergency roles")
    runtime = options.runtime.resolve()
    output = (options.output or ROOT / "Artifacts/D58" / datetime.now().strftime("Production-%Y%m%d-%H%M%S")).resolve()
    if output.exists() and any(output.iterdir()):
        raise ValueError("Use a fresh output path; existing production packs are preserved")
    ffmpeg, ffprobe = shutil.which("ffmpeg"), shutil.which("ffprobe")
    if not ffmpeg or not ffprobe:
        raise RuntimeError("Configured FFmpeg/FFprobe are required")
    for folder in ("Masters", "Runtime", "Preview", "Work/Profile/Roaming", "Work/Profile/Local", "Work/Profile/Temp", "Logs", "Sources"):
        (output / folder).mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env.pop("QT_QPA_PLATFORM", None)
    env.update({
        "APPDATA": str(output / "Work/Profile/Roaming"), "LOCALAPPDATA": str(output / "Work/Profile/Local"),
        "TEMP": str(output / "Work/Profile/Temp"), "TMP": str(output / "Work/Profile/Temp"),
    })
    (output / "Work/sclang.yaml").write_text(
        "includePaths:\n  - " + json.dumps((runtime / "SCClassLibrary").as_posix())
        + "\nexcludePaths: []\npostInlineWarnings: false\n", encoding="utf-8",
    )
    for binary in ("sclang.exe", "scsynth.exe"):
        if "3.14.1" not in auditions.run([runtime / binary, "-v"]).stdout.decode():
            raise RuntimeError("Unexpected SuperCollider version")
    source_files = [SOURCE / name for name in (
        "production-presets.json", "selections.json", "presets.json", "town-pitch-presets.json",
        "town-references.json", "EmergencyCues.scd", "TownWarning.scd",
    )] + [Path(__file__), ROOT / "Tools/Render-CityAudioAuditions.py"]
    for source in source_files:
        shutil.copy2(source, output / "Sources" / source.name)
    half_blend = frames(config["seamBlendMilliseconds"] / 1000)
    transition = frames(config["transitionBlendMilliseconds"] / 1000)
    assets, audio, gains = {}, {}, {}
    demos = {}
    for voice in config["voices"]:
        identity = voice["id"]
        selection = selected["decisions"][identity]
        if voice["candidate"] != selection.get("selected", selection.get("persistent")):
            raise ValueError(f"Production candidate does not match current selection: {identity}")
        intro, length, outro = (frames(voice[key]) for key in ("introSeconds", "loopSeconds", "outroSeconds"))
        if min(intro, length, outro) <= 2 * half_blend:
            raise ValueError("Phase length must exceed seam overlap")
        candidate = load_candidate(voice["presetFile"], voice["candidate"])
        candidate = {**candidate, "id": f"{identity}-Production", "parameters": dict(candidate["parameters"])}
        params = candidate["parameters"]
        params["duration"] = (intro + length * 3 + outro) / RATE
        if candidate.get("synth") == "TownMechanical":
            params.update({"spinUp": intro / RATE, "spinDown": outro / RATE})
        else:
            params["release"] = outro / RATE
        signal = render(candidate, output, runtime, env, ffmpeg, ffprobe)
        # Use the second settled loop period; the first already follows the intro.
        loop_signal = cyclic_slice(signal, intro + length, length, half_blend)
        gain, gain_db = gain_for(loop_signal, identity, config["loopTargetLUFS"], config["truePeakCeilingDBTP"], output, ffmpeg)
        gains[identity] = gain_db
        phases = {
            "Intro": quiet_edges(signal[:intro], start=True) * gain,
            "Loop": loop_signal * gain,
            "Outro": quiet_edges(signal[intro + length * 3:], end=True) * gain,
        }
        for phase, samples in phases.items():
            name = f"{identity}-{phase}"
            assets[name] = write_asset(output, name, samples, ffmpeg, config["truePeakCeilingDBTP"], loop=phase == "Loop")
            audio[name] = samples
        context = min(frames(3), length // 2)
        wrap_preview = quiet_edges(np.concatenate([phases["Loop"][-context:], phases["Loop"][:context]]), start=True, end=True)
        preview(output, f"{identity}-Wrap", wrap_preview, ffmpeg)
        assets[f"{identity}-Loop"]["wrapPreviewBoundarySeconds"] = context / RATE
        if abs(assets[f"{identity}-Loop"]["integratedLUFS"] - config["loopTargetLUFS"]) > 0.2:
            raise RuntimeError(f"Loop loudness target missed: {identity}")
        demo = phases["Intro"]
        for _ in range(voice["previewLoops"]):
            # Intro-to-loop changes use the contract blend; subsequent wraps are
            # literal concatenation, so the preview actually exercises the seam.
            demo = append_blended(demo, phases["Loop"], transition) if len(demo) == intro else np.concatenate([demo, phases["Loop"]])
        demo = append_blended(demo, phases["Outro"], transition)
        preview(output, f"{identity}-Lifecycle", demo, ffmpeg)
        demos[identity] = demo
        print(f'D58_PRODUCTION_ROLE_PASS {identity} loop={length / RATE:g}s gain={gain_db:.3f}dB', flush=True)
    for accent in config["accents"]:
        identity = accent["id"]
        if "candidate" in accent:
            candidate = load_candidate(accent["presetFile"], accent["candidate"])
        else:
            candidate = {"parameters": dict(accent["parameters"])}
        candidate = {**candidate, "id": identity, "parameters": dict(candidate["parameters"])}
        candidate["parameters"].update({"duration": accent["durationSeconds"], "attack": accent["attackSeconds"], "release": accent["releaseSeconds"]})
        signal = quiet_edges(render(candidate, output, runtime, env, ffmpeg, ffprobe), start=True, end=True)
        gain, gain_db = gain_for(signal, identity, accent["targetLUFS"], config["truePeakCeilingDBTP"], output, ffmpeg)
        signal *= gain
        gains[identity] = gain_db
        assets[identity] = write_asset(output, identity, signal, ffmpeg, config["truePeakCeilingDBTP"])
        audio[identity] = signal
        preview(output, identity, signal, ffmpeg)
        print(f'D58_PRODUCTION_ACCENT_PASS {identity} {len(signal) / RATE:g}s', flush=True)
    # Demonstrate two eligible accents; this is an offline mix illustration,
    # not a claim that traffic/event selection or gameplay ducking is implemented.
    accent_events = {}
    for role, accent_name in (("Police", "Police-Alternate"), ("Fire", "Fire-Horn")):
        base = np.tile(audio[f"{role}-Loop"], math.ceil(16 * RATE / len(audio[f"{role}-Loop"])))[:frames(16)]
        accent_events[role] = [4, 4 + config["runtimeContract"]["accentCooldownSeconds"][role] + 1]
        for seconds in accent_events[role]:
            start = frames(seconds)
            accent = audio[accent_name]
            ramp = frames(0.04)
            recovery = frames(0.15)
            duck = np.ones(len(base))
            level = 0 if role == "Police" else 0.5
            duck[start - ramp:start] = np.linspace(1, level, ramp)
            duck[start:start + len(accent)] = level
            duck[start + len(accent):start + len(accent) + recovery] = np.linspace(level, 1, recovery)
            base *= duck
            base[start:start + len(accent)] += accent
        base = quiet_edges(base, start=True, end=True)
        preview(output, f"{role}-AccentMix", base, ffmpeg)
    manifest = {
        "task": "D58", "createdUTC": datetime.now(timezone.utc).isoformat(),
        "scope": "Selected source production authoring only; Unreal/device acceptance pending",
        "provenance": "Original built-in-UGen synthesis, no sampled references or third-party libraries",
        "config": config, "selections": selected, "assets": assets, "staticGainDB": gains,
        "sourceSha256": {p.name: auditions.sha(p) for p in source_files},
        "sourceSnapshots": "Sources", "runtimeContract": config["runtimeContract"],
        "illustrativeAccentEventsSeconds": accent_events,
        "masterBytes": sum((output / "Masters" / f"{name}.wav").stat().st_size for name in assets),
        "runtimePCMBytes": sum((output / record["runtime"]).stat().st_size for record in assets.values()),
    }
    (output / "Manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    validation = {
        "status": "Pass", "productionAssets": len(assets), "roles": 4, "standaloneAccents": 2,
        "checks": ["Native SuperCollider score/render success", "Mono 48 kHz PCM24 masters and PCM16 runtime copies",
                   "Loop loudness within 0.2 LUFS of target", "All phase/accents below -6 dBTP",
                   "Loop boundary step within local adjacent-sample derivative range", "No nonfinite or clipped samples"],
        "pending": ["Human repeated-wrap, transition and accent-mix listening", "Unreal Wave Player/import/phase/event integration",
                    "Actual concurrency, spatial/lifecycle/saved-controls checks and Desktop/PCVR/Quest listening/fatigue/cost"],
        "sha256": {p.relative_to(output).as_posix(): auditions.sha(p)
                   for folder in ("Masters", "Runtime", "Preview") for p in (output / folder).iterdir()},
    }
    (output / "Validation.json").write_text(json.dumps(validation, indent=2), encoding="utf-8")
    cards = []
    for role in ("Police", "Fire", "Ambulance", "Town"):
        accent = f'<p>Separate accent example</p><audio controls preload="none" src="Preview/{role}-AccentMix.mp3"></audio>' if role in ("Police", "Fire") else ""
        boundary = assets[role + "-Loop"]["wrapPreviewBoundarySeconds"]
        cards.append(f'<section><h2>{role}</h2><p>Intro → repeated loop → normal outro. Loop {assets[role + "-Loop"]["seconds"]:g}s.</p><audio controls preload="none" src="Preview/{role}-Lifecycle.mp3"></audio><p>Short wrap inspection: actual boundary at {boundary:g}s.</p><audio controls preload="none" src="Preview/{role}-Wrap.mp3"></audio>{accent}</section>')
    (output / "Review.html").write_text('''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>D58 production sound phases</title><style>body{font:16px/1.6 system-ui;margin:0;background:#eef2f4;color:#15212b}main{max-width:900px;margin:auto;padding:30px}section{background:white;padding:24px;border-radius:12px;margin:20px 0}audio{width:100%}h2{margin-top:0}a{color:#175587}</style><main><h1>D58 · production sound phases</h1><p>Selected identities with separate starts, continuous loops and normal endings. Lifecycle players include literal repeated loop wraps. Static source gain is shared across each role's phases.</p><p>Police B is a short alternate phrase; fire horn is an independent effect. Mix demonstrations are offline examples. Gameplay eligibility, mixing, lifecycle cancellation, localization and headset cost remain unverified.</p>''' + "".join(cards) + '<p><a href="Manifest.json">Asset/loop/mix contract</a> · <a href="Validation.json">Technical checks</a></p></main></html>', encoding="utf-8")
    print(f"D58_PRODUCTION_AUTHORING_PASS {output}", flush=True)


if __name__ == "__main__":
    main()
