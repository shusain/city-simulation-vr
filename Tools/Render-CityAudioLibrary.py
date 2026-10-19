"""Generate D58's remaining sample library outside Unreal, with review mixes."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import html
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import shutil
import sys

import numpy as np

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "AudioSource/Soundscape"
module_spec = importlib.util.spec_from_file_location("d58_production", ROOT / "Tools/Render-CityAudioProduction.py")
production = importlib.util.module_from_spec(module_spec)
module_spec.loader.exec_module(production)
auditions = production.auditions
RATE = 48000


def shape_signal(raw_signal, sample, seam_half_blend):
    seconds = sample["seconds"] * (3 if sample["loop"] else 1)
    start = production.frames(0.05)
    count = production.frames(seconds)
    if len(raw_signal) < start + count:
        raise RuntimeError(f'Truncated library render: {sample["id"]}')
    signal = raw_signal[start:start + count].copy()
    if sample["loop"]:
        length = production.frames(sample["seconds"])
        return production.cyclic_slice(signal, length, length, seam_half_blend)
    return production.quiet_edges(signal, start=True, end=True)


def render_sample(sample, index, output, runtime, env, ffmpeg, ffprobe, seam_half_blend, reuse=None):
    identity = sample["id"]
    seconds = sample["seconds"] * (3 if sample["loop"] else 1)
    if reuse and (reuse / "Work" / f"{identity}-raw.wav").is_file():
        compiler = reuse / "Logs" / f"{identity}-sclang.log"
        renderer = reuse / "Logs" / f"{identity}-scsynth.log"
        compiler_text, renderer_text = compiler.read_text(), renderer.read_text()
        if f"D58_LIBRARY_SCORE_READY {identity}" not in compiler_text or "ERROR" in compiler_text + renderer_text or "FAILURE" in renderer_text:
            raise RuntimeError(f"Unverified cached source: {identity}")
        cached = reuse / "Work" / f"{identity}-raw.wav"
        raw_signal = auditions.read_audio(ffmpeg, ffprobe, cached)
        shutil.copy2(cached, output / "Work" / cached.name)
        shutil.copy2(compiler, output / "Logs" / compiler.name)
        shutil.copy2(renderer, output / "Logs" / renderer.name)
        return shape_signal(raw_signal, sample, seam_half_blend)
    score = output / "Work" / f"{identity}.osc"
    job = output / "Work" / f"{identity}.scd"
    job.write_text(f'''(
try {{
    var definition, score;
    {json.dumps((SOURCE / "SampleLibrary.scd").as_posix())}.load;
    Archive.archiveDir = {json.dumps((output / "Work/Profile").as_posix())};
    definition = ~d58LibraryMake.value({sample["kind"]}, {sample["variant"]});
    score = Score([
        [0.0, [\\d_recv, definition.asBytes]],
        [0.05, [\\s_new, \\D58Library, 1000, 0, 1, "duration", {seconds}, "seed", {580100 + index}, "enveloped", {0 if sample["loop"] else 1}]],
        [{seconds + 0.05}, [\\n_free, 1000]],
        [{seconds + 0.1}, [\\c_set, 0, 0]]
    ]);
    score.write({json.dumps(score.as_posix())});
    "D58_LIBRARY_SCORE_READY {identity}".postln;
    0.exit;
}} {{ |error| error.reportError; 1.exit; }};
)
''', encoding="utf-8")
    compile_result = auditions.run([
        runtime / "sclang.exe", "-D", "-a", "-l", output / "Work/sclang.yaml",
        "-d", output / "Work/Profile", job,
    ], env=env, cwd=runtime, log=output / "Logs" / f"{identity}-sclang.log")
    log = (compile_result.stdout + compile_result.stderr).decode("utf-8", errors="replace")
    if f"D58_LIBRARY_SCORE_READY {identity}" not in log or "ERROR:" in log:
        raise RuntimeError(f"Library synth compilation failed: {identity}")
    raw = output / "Work" / f"{identity}-raw.wav"
    result = auditions.run([
        runtime / "scsynth.exe", "-N", score, "_", raw, RATE, "WAV", "float",
        "-i", "0", "-o", "1", "-z", "64", "-D", "0", "-R", "0", "-U", runtime / "plugins",
    ], env=env, cwd=runtime, log=output / "Logs" / f"{identity}-scsynth.log")
    log = (result.stdout + result.stderr).decode("utf-8", errors="replace")
    if "ERROR" in log or "FAILURE" in log:
        raise RuntimeError(f"Library synthesis failed: {identity}")
    raw_signal = auditions.read_audio(ffmpeg, ffprobe, raw)
    return shape_signal(raw_signal, sample, seam_half_blend)


def calibrated_gain(signal, identity, target, ceiling, output, ffmpeg):
    # Short/quiet raw taps can sit below the absolute loudness-measurement gate.
    # Measure a safely scaled copy, then fold that scale into the single final
    # static gain. No compressor, limiter or audio dynamics change is applied.
    peak = float(np.max(np.abs(signal)))
    if peak <= 1e-10:
        raise RuntimeError(f"Silent synthesized source: {identity}")
    reference_scale = .2 / peak
    gain, gain_db = production.gain_for(signal * reference_scale, identity, target, ceiling, output, ffmpeg)
    return gain * reference_scale, gain_db + 20 * math.log10(reference_scale)


def review_copy(output, name, signal, ffmpeg):
    path = output / "Preview" / f"{name}.wav"
    auditions.write_wave(path, signal, bits=16)
    auditions.mp3(ffmpeg, path, path.with_suffix(".mp3"))


def mix_bed(target, signal, gain=1):
    repeated = np.tile(signal, math.ceil(len(target) / len(signal)))[:len(target)]
    target += repeated * gain


def mix_event(target, signal, seconds, gain=1):
    start = production.frames(seconds)
    count = min(len(signal), len(target) - start)
    if start < 0 or count <= 0:
        raise ValueError("Mix event outside preview")
    target[start:start + count] += signal[:count] * gain


def groups(config):
    result = {}
    for sample in config["samples"]:
        result.setdefault(sample["group"], []).append(sample)
    return result


def html_review(output, config, assets, comparisons, scenes):
    sections = []
    for group, members in groups(config).items():
        comp = comparisons[group]
        timing = "; ".join(f'{item["id"]} at {item["startSeconds"]:g}s' for item in comp["sequence"])
        entries = []
        for sample in members:
            identity = sample["id"]
            record = assets[identity]
            extra = f'<a href="Preview/{identity}-Repeat.mp3">Two loop cycles</a>' if sample["loop"] else "One-shot"
            entries.append(f'''<article><h3>{html.escape(identity)}</h3><p>{html.escape(sample["intent"])}</p>
<audio controls preload="none" src="Preview/{identity}.mp3"></audio>
<p class="meta">{record["seconds"]:g}s · {record["integratedLUFS"]:.2f} LUFS · {"Loop" if sample["loop"] else "One-shot"}</p>
<p><a href="{record["master"]}">PCM24 WAV</a> · <a href="{record["runtime"]}">PCM16 WAV</a> · {extra}</p></article>''')
        sections.append(f'''<section><h2>{html.escape(group)}</h2><p>Quick category reel, in listed order with quiet gaps.</p>
<audio controls preload="none" src="Preview/{comp["file"]}.mp3"></audio><p class="meta">{html.escape(timing)}</p>
<div class="grid">{"".join(entries)}</div></section>''')
    scene_cards = "".join(f'<article><h3>{html.escape(name)}</h3><p>{html.escape(scene["description"])}</p><audio controls preload="none" src="Preview/{name}.mp3"></audio></article>' for name, scene in scenes.items())
    focus = "".join(f'<article><h3>{html.escape(identity)}</h3><p>{html.escape(assets[identity]["intent"])}</p><audio controls preload="none" src="Preview/{identity}.mp3"></audio></article>' for identity in config.get("revisionFocus", []))
    (output / "Review.html").write_text('''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>D58 ambience and effects review</title><style>body{font:16px/1.6 system-ui;margin:0;background:#eef2f4;color:#15212b}main{max-width:1120px;margin:auto;padding:30px}section{background:white;padding:24px;border-radius:12px;margin:24px 0}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(270px,1fr));gap:24px}article{border-top:1px solid #d9e1e5;padding-top:12px}audio{width:100%}.meta{font-size:13px;color:#536373}h2{margin-top:0}a{color:#175587}</style><main><p class="meta">CITY SIM VR · D58 · SAMPLE REFINEMENT</p><h1>Ambience, activity and notification sounds</h1><p>''' + f'{len(assets)} original synthesized sources. WAV source/import copies and MP3 listening previews. ' + html.escape(config["reviewStatus"]) + '''</p><p>Judge naturalness, useful character, relative level and repetition. Wildlife and crowd textures are synthesized, with no claimed species or recorded voices. All assets are dry mono. Bed targets are quieter than notification targets; these are authoring levels rather than a finished game mix.</p><section><h2>Illustrative layered scenes</h2><p>Offline examples show how the stems could combine. They do not implement actual weather, location, listener or game-state behavior.</p><div class="grid">''' + scene_cards + '</div></section><section><h2>Changed sounds for this review</h2><p>Listen to the requested revisions below. Keep playback volume fixed when comparing sounds.</p><div class="grid">' + focus + '</div></section>' + "".join(sections) + '<p><a href="Manifest.json">Sources, levels, preview timings and future mix policy</a> · <a href="Validation.json">Technical checks</a></p></main></html>', encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--runtime", type=Path, default=ROOT / "Artifacts/Toolchain/SuperCollider/SuperCollider")
    parser.add_argument("--reuse-raw", type=Path, help="Reuse native raw renders only after source/control/log validation")
    parser.add_argument("--revise-rain-only", action="store_true", help="Preserve32 accepted samples byte-for-byte; render only the four rain beds")
    options = parser.parse_args()
    config = json.loads((SOURCE / "library-presets.json").read_text())
    if config["schema"] != 1 or config["sampleRate"] != RATE or not config["samples"]:
        raise ValueError("Unsupported library configuration")
    ids = set()
    for sample in config["samples"]:
        identity = sample["id"]
        if not re.fullmatch(r"[A-Za-z]+(?:-[A-Za-z]+)*", identity) or identity in ids:
            raise ValueError(f"Invalid/duplicate sample ID: {identity}")
        ids.add(identity)
        if not (0 <= sample["kind"] <= 17 and 0 <= sample["variant"] <= 6 and .4 <= sample["seconds"] <= 30 and -40 <= sample["targetLUFS"] <= -20):
            raise ValueError(f"Invalid source controls: {identity}")
        if sample["loop"] and sample["seconds"] != 20:
            raise ValueError("This library's loop cadence assumes 20-second beds")
    runtime = options.runtime.resolve()
    reuse = options.reuse_raw.resolve() if options.reuse_raw else None
    revised = {"Rain-Light", "Rain-Moderate", "Rain-Heavy", "Rain-Downpour"} if options.revise_rain_only else set()
    if revised and not reuse:
        raise ValueError("Scoped rain revision requires a verified previous library")
    if reuse:
        if revised:
            pattern = r"\n\s*7, \{.*?(?=\n\s*8, \{)"
            prior_synth = (reuse / "Sources/SampleLibrary.scd").read_text()
            current_synth = (SOURCE / "SampleLibrary.scd").read_text()
            if re.sub(pattern, "", prior_synth, flags=re.S) != re.sub(pattern, "", current_synth, flags=re.S):
                raise ValueError("Scoped rain revision changed another synthesis branch")
            for name in ("Validation.json", "IndependentVerification.json"):
                if json.loads((reuse / name).read_text())["status"] != "Pass":
                    raise ValueError("Previous library lacks verified evidence")
            old_manifest = json.loads((reuse / "Manifest.json").read_text())
            old_validation = json.loads((reuse / "Validation.json").read_text())
            for relative, digest in old_validation["sha256"].items():
                if auditions.sha(reuse / relative) != digest:
                    raise ValueError(f"Previous library changed: {relative}")
        elif auditions.sha(reuse / "Sources/SampleLibrary.scd") != auditions.sha(SOURCE / "SampleLibrary.scd"):
            raise ValueError("Cached native synth source does not match")
        prior = json.loads((reuse / "Sources/library-presets.json").read_text())
        keys = ("id", "kind", "variant", "loop", "seconds")
        if len(prior["samples"]) != len(config["samples"]) or any(
            any(a[key] != b[key] for key in keys) for a, b in zip(prior["samples"], config["samples"])
        ) or prior["sampleRate"] != RATE:
            raise ValueError("Cached native controls/order/seed inputs do not match")
    output = (options.output or ROOT / "Artifacts/D58" / datetime.now().strftime("Library-%Y%m%d-%H%M%S")).resolve()
    if output.exists() and any(output.iterdir()):
        raise ValueError("Use a new empty path to preserve prior review libraries")
    ffmpeg, ffprobe = shutil.which("ffmpeg"), shutil.which("ffprobe")
    if not ffmpeg or not ffprobe:
        raise RuntimeError("Configured FFmpeg and FFprobe are required")
    for folder in ("Masters", "Runtime", "Preview", "Logs", "Sources", "Work/Profile/Roaming", "Work/Profile/Local", "Work/Profile/Temp"):
        (output / folder).mkdir(parents=True, exist_ok=True)
    sources = [SOURCE / "library-presets.json", SOURCE / "SampleLibrary.scd", SOURCE / "selections.json", Path(__file__), ROOT / "Tools/Render-CityAudioProduction.py", ROOT / "Tools/Render-CityAudioAuditions.py"]
    for source in sources:
        shutil.copy2(source, output / "Sources" / source.name)
    env = os.environ.copy()
    env.pop("QT_QPA_PLATFORM", None)
    env.update({"APPDATA": str(output / "Work/Profile/Roaming"), "LOCALAPPDATA": str(output / "Work/Profile/Local"), "TEMP": str(output / "Work/Profile/Temp"), "TMP": str(output / "Work/Profile/Temp")})
    (output / "Work/sclang.yaml").write_text("includePaths:\n  - " + json.dumps((runtime / "SCClassLibrary").as_posix()) + "\nexcludePaths: []\npostInlineWarnings: false\n")
    for binary in ("sclang.exe", "scsynth.exe"):
        if "3.14.1" not in auditions.run([runtime / binary, "-v"]).stdout.decode():
            raise RuntimeError("Unexpected SuperCollider version")
    assets, audio = {}, {}
    for index, sample in enumerate(config["samples"]):
        identity = sample["id"]
        if revised and identity not in revised:
            record = old_manifest["assets"][identity].copy()
            for relative in (record["master"], record["runtime"]):
                shutil.copy2(reuse / relative, output / relative)
            for suffix in (".wav", ".mp3", "-Repeat.wav", "-Repeat.mp3"):
                path = reuse / "Preview" / (identity + suffix)
                if path.exists():shutil.copy2(path, output / "Preview" / path.name)
            assets[identity] = record
            audio[identity] = auditions.read_audio(ffmpeg, ffprobe, output / record["master"])
            print(f'D58_LIBRARY_SAMPLE_PRESERVED {identity}', flush=True)
            continue
        signal = render_sample(sample, index, output, runtime, env, ffmpeg, ffprobe, production.frames(config["loopSeamHalfBlendMilliseconds"] / 1000), reuse=None if revised else reuse)
        gain, gain_db = calibrated_gain(signal, identity, sample["targetLUFS"], config["truePeakCeilingDBTP"], output, ffmpeg)
        signal *= gain
        record = production.write_asset(output, identity, signal, ffmpeg, config["truePeakCeilingDBTP"], loop=sample["loop"])
        record.update({"group": sample["group"], "intent": sample["intent"], "loop": sample["loop"], "targetLUFS": sample["targetLUFS"], "staticGainDB": gain_db, "seed": 580100 + index})
        matched = abs(record["integratedLUFS"] - sample["targetLUFS"]) <= .3
        peak_limited = (record["integratedLUFS"] < sample["targetLUFS"]
                        and abs(record["truePeakDBTP"] - config["truePeakCeilingDBTP"]) <= .1)
        if not matched and not peak_limited:
            raise RuntimeError(f"Sample normalization failed: {identity}")
        record["normalization"] = "Static gain, loudness matched" if matched else "Static gain, peak-ceiling limited"
        assets[identity], audio[identity] = record, signal
        review_copy(output, identity, production.quiet_edges(signal, start=True, end=True), ffmpeg)
        if sample["loop"]:
            review_copy(output, f"{identity}-Repeat", production.quiet_edges(np.tile(signal, 2), start=True, end=True), ffmpeg)
        print(f'D58_LIBRARY_SAMPLE_PASS {index + 1}/{len(config["samples"])} {identity}', flush=True)
    comparisons = {}
    for index, (group, members) in enumerate(groups(config).items()):
        sequence = [np.zeros(production.frames(.3))]
        positions, cursor = [], .3
        for sample in members:
            identity = sample["id"]
            clip = audio[identity][:production.frames(min(4, sample["seconds"]))]
            clip = production.quiet_edges(clip, start=True, end=True)
            positions.append({"id": identity, "startSeconds": cursor, "seconds": len(clip) / RATE})
            sequence += [clip, np.zeros(production.frames(.7))]
            cursor += len(clip) / RATE + .7
        filename = f"Category-{index + 1}"
        review_copy(output, filename, np.concatenate(sequence), ffmpeg)
        comparisons[group] = {"file": filename, "sequence": positions, "frames": sum(len(s) for s in sequence)}
    scenes = {}
    def scene(name, description, beds, events=()):
        signal = np.zeros(production.frames(20))
        for identity, gain in beds:
            mix_bed(signal, audio[identity], gain)
        for identity, seconds, gain in events:
            mix_event(signal, audio[identity], seconds, gain)
        signal = production.quiet_edges(signal, start=True, end=True)
        # Retain library relative levels. Only attenuate a mix if needed for
        # preview headroom; do not independently loudness-normalize each scene.
        peak = float(np.max(np.abs(signal)))
        attenuation = min(1, (10 ** (-10 / 20)) / max(peak, 1e-12))
        signal *= attenuation
        review_copy(output, name, signal, ffmpeg)
        scenes[name] = {"description": description, "frames": len(signal), "beds": beds, "events": events, "staticAttenuation": attenuation}
    scene("Scene-Park-Day", "Leafy breeze, fountain and varied synthesized daytime bird phrases.", [("Park-Foliage", .8), ("Park-Fountain", .6)], [("Bird-Chirp-A", 2, .65), ("Bird-Chirp-B", 7, .5), ("Bird-Chirp-C", 13, .65)])
    scene("Scene-Park-Night", "Quiet foliage with rougher, irregular night-insect phrases.", [("Park-Foliage", .65), ("Night-Crickets-Light", .8)])
    scene("Scene-Industry", "Accepted idle hum, intermittent tool/rattle bed and distinct occasional metal clanks.", [("Industry-Idle", .6), ("Industry-Working", .65), ("City-Distant", .2)], [("Industry-Clank", 3, .6), ("Industry-Clank", 10, .45), ("Industry-Clank", 16, .55)])
    scene("Scene-Weather", "Moderate rain under gusts, with reduced continuous noise and the original thunder events.", [("Wind-Gusts", .45), ("Rain-Moderate", .65), ("Storm-Roar", .15)], [("Thunder-Distant", 3, .7), ("Thunder-Near", 12, .7)])
    scene("Scene-City", "Restrained city/market bed without a wandering bass tone; engine/tire passes rise and fall smoothly.", [("City-Distant", .8), ("Market-Murmur", .5)], [("Car-Pass", 2, .5), ("Truck-Pass", 7, .5), ("Car-Pass", 14, .4)])
    scene("Scene-Activity", "Quiet crackling fire with separate example construction, impact and quake events.", [("Fire-Crackle", .6)], [("Construction-Tap", 2, .5), ("Demolition-Impact", 6, .5), ("Earthquake-Rumble", 12, .65)])
    manifest = {
        "task": "D58", "createdUTC": datetime.now(timezone.utc).isoformat(), "scope": "Remaining sample source library for review; no Unreal integration",
        "config": config, "assets": assets, "categoryReels": comparisons, "scenes": scenes,
        "sourceSha256": {p.name: auditions.sha(p) for p in sources}, "sourceSnapshots": "Sources",
        "sirens": "Existing Production-20261011-v2 source pack user-accepted; not regenerated or modified",
        "validatedNativeRawReuse": str(reuse) if reuse else None,
        "revisedOnly": sorted(revised), "preservedRuntimeSources": 32 if revised else 0,
        "runtimePCMBytes": sum((output / record["runtime"]).stat().st_size for record in assets.values()),
    }
    (output / "Manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    validation = {
        "status": "Pass", "sampleCount": len(assets), "loopCount": sum(s["loop"] for s in config["samples"]), "oneShotCount": sum(not s["loop"] for s in config["samples"]),
        "checks": ["Native SuperCollider source/render success", "Mono 48 kHz PCM24 masters and PCM16 import copies", "Exact sample lengths", "Finite unclipped samples and -9 dBTP headroom", "Static gain matches loudness within 0.3 LUFS or explicitly stops at the peak ceiling", "Whole-file loop wraps and quiet one-shot edges"],
        "pending": ["Human naturalness/timbre/density/relative-level/repetition review", "Actual Unreal import/playback/state/lifecycle/spatial/mix/controls integration", "Desktop/PCVR/Quest localization, comfort/fatigue and measured voice/memory/CPU cost"],
        "sha256": {p.relative_to(output).as_posix(): auditions.sha(p) for folder in ("Masters", "Runtime", "Preview") for p in (output / folder).iterdir()},
    }
    (output / "Validation.json").write_text(json.dumps(validation, indent=2), encoding="utf-8")
    html_review(output, config, assets, comparisons, scenes)
    print(f"D58_REMAINING_LIBRARY_PASS {output}", flush=True)


if __name__ == "__main__":
    main()
