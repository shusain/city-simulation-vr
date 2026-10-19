"""Render D58 SuperCollider auditions without Unreal or an audio device.

Uses the bundled Python/NumPy, portable SuperCollider and installed FFmpeg.
WAV masters are gain-matched using measured integrated LUFS (static gain only).
MP3s and A/B/repeat files are review derivatives, never production references.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import html
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import wave

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "AudioSource/Soundscape"
RATE = 48000
ALLOWED = {
    "duration", "centerHz", "depthHz", "sweepHz", "shape", "second", "third", "fifth",
    "detune", "rotorHz", "rotorDepth", "hornHz", "hornGain", "hornEvery", "hornDuty",
    "cutoffHz", "attack", "release",
}
MECHANICAL_ALLOWED = {
    "duration", "nominalHz", "lowerRatio", "spinUp", "spinDown", "sweepHz", "wailDepth",
    "secondaryRatio", "secondaryGain", "brightness", "rotationHz", "rotationDepth",
    "airGain", "motorGain", "cutoffHz", "seed", "lowerOctaveGain",
}
SYNTHS = {
    "Emergency": ("EmergencyCues.scd", ALLOWED),
    "TownMechanical": ("TownWarning.scd", MECHANICAL_ALLOWED),
}


def grouped_candidates(spec):
    groups = {}
    for candidate in spec["candidates"]:
        groups.setdefault(candidate["group"], []).append(candidate)
    return groups


def comparison_name(group, members):
    suffix = "AB" if [c["id"] for c in members] == [f"{group}-A", f"{group}-B"] else "Sequence"
    return f"{group}-{suffix}"


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(args, *, env=None, cwd=None, timeout=60, log=None):
    try:
        result = subprocess.run(
            [str(a) for a in args], cwd=cwd, env=env, stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout,
            creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0,
        )
    except subprocess.TimeoutExpired as error:
        if log:
            Path(log).write_text(
                ((error.stdout or b"") + (error.stderr or b"")).decode("utf-8", errors="replace")
                + f"\nD58_PROCESS_TIMEOUT {timeout}s\n", encoding="utf-8",
            )
        raise RuntimeError(f"{Path(args[0]).name} timed out; see {log or 'process diagnostics'}") from error
    output = (result.stdout + result.stderr).decode("utf-8", errors="replace")
    if log:
        Path(log).write_text(output, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"{Path(args[0]).name} failed ({result.returncode}): {output[-3000:]}")
    return result


def measure(ffmpeg, path):
    result = run([
        ffmpeg, "-nostdin", "-hide_banner", "-i", path, "-af",
        "loudnorm=I=-24:TP=-6:LRA=11:print_format=json", "-f", "null", "-",
    ])
    text = result.stderr.decode("utf-8", errors="replace")
    block = re.search(r'\{\s*"input_i".*?\}', text, re.S)
    if not block:
        raise RuntimeError(f"FFmpeg did not return loudness metrics: {text[-2000:]}")
    values = json.loads(block.group())
    values = {key: float(values[key]) for key in ("input_i", "input_tp", "input_lra", "input_thresh")}
    if not all(math.isfinite(v) for v in values.values()):
        raise RuntimeError(f"Nonfinite loudness measurement for {path}")
    return values


def read_audio(ffmpeg, ffprobe, path):
    info = json.loads(run([
        ffprobe, "-v", "error", "-show_streams", "-of", "json", path,
    ]).stdout)["streams"]
    if len(info) != 1 or info[0]["channels"] != 1 or int(info[0]["sample_rate"]) != RATE:
        raise RuntimeError(f"Expected one mono {RATE} Hz stream: {path}")
    decoded = run([
        ffmpeg, "-nostdin", "-v", "error", "-i", path, "-f", "f32le",
        "-acodec", "pcm_f32le", "-",
    ]).stdout
    audio = np.frombuffer(decoded, dtype="<f4").astype(np.float64)
    if not len(audio) or not np.isfinite(audio).all():
        raise RuntimeError(f"Empty or nonfinite render: {path}")
    return audio


def write_wave(path, audio, bits=24):
    if not np.isfinite(audio).all() or np.max(np.abs(audio)) >= 1:
        raise RuntimeError(f"Nonfinite or clipped output: {path}")
    with wave.open(str(path), "wb") as recording:
        recording.setnchannels(1)
        recording.setsampwidth(bits // 8)
        recording.setframerate(RATE)
        quantized = np.rint(audio * (2 ** (bits - 1) - 1)).astype("<i4")
        if bits == 24:
            payload = quantized.view(np.uint8).reshape(-1, 4)[:, :3].copy().tobytes()
        else:
            payload = quantized.astype("<i2").tobytes()
        recording.writeframes(payload)


def mp3(ffmpeg, source, destination):
    run([
        ffmpeg, "-nostdin", "-v", "error", "-i", source, "-c:a", "libmp3lame",
        "-b:a", "192k", "-map_metadata", "-1", destination,
    ])


def validate_presets(spec):
    if spec["schema"] != 1 or spec["sampleRate"] != RATE:
        raise ValueError("Unsupported preset schema/sample rate")
    ids = set()
    groups = {}
    for candidate in spec["candidates"]:
        identity = candidate["id"]
        if not re.fullmatch(r"(Police|Fire|Ambulance|Town)-[A-Z]", identity) or identity in ids:
            raise ValueError(f"Invalid or duplicate candidate ID: {identity}")
        ids.add(identity)
        if candidate["group"] != identity.split("-")[0]:
            raise ValueError(f"Mismatched group: {identity}")
        groups.setdefault(candidate["group"], []).append(identity)
        params = candidate["parameters"]
        kind = candidate.get("synth", "Emergency")
        if kind not in SYNTHS:
            raise ValueError(f"Unknown synth: {kind}")
        if set(params) - SYNTHS[kind][1] or not all(
            isinstance(v, (int, float)) and math.isfinite(v) for v in params.values()
        ):
            raise ValueError(f"Unknown or nonfinite synth parameter: {identity}")
        if not (4 <= params["duration"] <= 30 and 0 < params.get("sweepHz", 0.08) <= 10):
            raise ValueError(f"Invalid duration/cadence: {identity}")
        if kind == "Emergency":
            if not (params["centerHz"] > params["depthHz"] > 0):
                raise ValueError(f"Invalid frequency range: {identity}")
            if params["shape"] not in (0, 1, 2):
                raise ValueError(f"Invalid sweep shape: {identity}")
            if params.get("attack", 0.12) + params.get("release", 0.4) >= params["duration"]:
                raise ValueError(f"Envelope exceeds duration: {identity}")
        else:
            if not (150 <= params["nominalHz"] <= 1000 and 0 < params["lowerRatio"] < 1):
                raise ValueError(f"Invalid motor frequency range: {identity}")
            if not (0 < params["spinUp"] and 0 < params["spinDown"]
                    and params["spinUp"] + params["spinDown"] < params["duration"]):
                raise ValueError(f"Invalid motor rise/coast timing: {identity}")
            for key in ("wailDepth", "rotationDepth", "secondaryGain", "brightness", "airGain", "motorGain", "lowerOctaveGain"):
                if not (0 <= params.get(key, 0) <= 1):
                    raise ValueError(f"Invalid {key}: {identity}")
    if not (2 <= len(ids) <= 16) or any(len(members) < 2 for members in groups.values()):
        raise ValueError("Expected 2-16 candidates with at least two per comparison group")
    if not (-30 <= spec["targetLUFS"] <= -18 and -12 <= spec["truePeakCeilingDBTP"] <= -3):
        raise ValueError("Audition levels outside supported range")


def make_score(candidate, output, runtime, env):
    identity = candidate["id"]
    osc = output / "Work" / f"{identity}.osc"
    job = output / "Work" / f"{identity}.scd"
    params = candidate["parameters"]
    args = ", ".join(f'"{key}", {value}' for key, value in params.items())
    # JSON string quoting is valid SC string syntax here; paths use forward slashes.
    source = json.dumps((SOURCE / SYNTHS[candidate.get("synth", "Emergency")][0]).as_posix())
    osc_literal = json.dumps(osc.as_posix())
    job.write_text(f'''(
try {{
    var score;
    {source}.load;
    if(~d58Emergency.isNil) {{ Error("D58 SynthDef did not load").throw }};
    Archive.archiveDir = {json.dumps((output / "Work/Profile").as_posix())};
    score = Score([
        [0.0, [\\d_recv, ~d58Emergency.asBytes]],
        [0.05, [\\s_new, \\D58Emergency, 1000, 0, 1, {args}]],
        [{params["duration"] + 0.05}, [\\n_free, 1000]],
        [{params["duration"] + 0.1}, [\\c_set, 0, 0]]
    ]);
    score.write({osc_literal});
    "D58_SCORE_READY {identity}".postln;
    0.exit;
}} {{ |error|
    error.reportError;
    1.exit;
}};
)
''', encoding="utf-8")
    result = run([
        runtime / "sclang.exe", "-D", "-a", "-l", output / "Work/sclang.yaml",
        "-d", output / "Work/Profile", job,
    ], cwd=runtime, env=env, log=output / "Logs" / f"{identity}-sclang.log")
    text = (result.stdout + result.stderr).decode("utf-8", errors="replace")
    if f"D58_SCORE_READY {identity}" not in text or "ERROR:" in text or not osc.is_file():
        raise RuntimeError(f"SC score failed: {identity}; see its sclang log")
    return osc


def review_html(output, spec, metrics):
    cards = []
    for group, members in grouped_candidates(spec).items():
        entries = []
        for c in members:
            identity = c["id"]
            m = metrics[identity]
            entries.append(f'''<div class="candidate"><h3>{html.escape(c["label"])}</h3>
<p>{html.escape(c["intent"])}</p><audio controls preload="none" src="Preview/{identity}.mp3"></audio>
<p class="meta">{m["seconds"]:g}s · {m["integratedLUFS"]:.2f} LUFS · {m["truePeakDBTP"]:.2f} dBTP</p>
<p><a href="Masters/{identity}.wav">24-bit mono WAV</a> · <a href="Preview/{identity}-Repeat.mp3">Three repeats</a></p></div>''')
        starts = []
        position = 0.4
        for candidate in members:
            starts.append(f'{candidate["id"]} at {position:g}s')
            position += candidate["parameters"]["duration"] + 1
        timing = html.escape("; ".join(starts))
        compare = comparison_name(group, members)
        cards.append(f'''<section><h2>{group}</h2><p>In listed order, with one second of silence between candidates.
{timing}.</p><audio controls preload="none" src="Compare/{compare}.mp3"></audio>
<div class="pair">{"".join(entries)}</div></section>''')
    (output / "Review.html").write_text(f'''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>D58 emergency sound auditions</title>
<style>body{{font:16px/1.6 system-ui,sans-serif;background:#eef2f4;color:#15212b;margin:0}}
main{{max-width:1000px;margin:auto;padding:36px 24px}}h1{{font-size:32px;line-height:1.2}}h2{{margin-top:0}}
section{{background:white;border:1px solid #d4dde2;border-radius:14px;padding:24px;margin:24px 0}}
.pair{{display:grid;grid-template-columns:1fr 1fr;gap:24px}}audio{{width:100%}}.meta{{color:#536373;font-size:14px}}
a{{color:#175587}}@media(max-width:650px){{.pair{{grid-template-columns:1fr}}}}</style><main>
<p class="meta">CITY SIM VR · D58 · OFFLINE AUDITION</p><h1>{html.escape(spec.get("title", "Emergency sound identities"))}</h1>
<p>{len(spec["candidates"])} original dry mono candidates. Static loudness matching targets {spec["targetLUFS"]:g} LUFS
with at least {-spec["truePeakCeilingDBTP"]:g} dB true-peak headroom. Use a comfortable listening volume.</p>
<p>Review: choose a candidate, neither, or a combination for each role. Consider whether each sound is recognizable,
distinct from the other roles, and comfortable after several repeats. Mechanical textures are synthetic drafts.</p>
<p>These are enveloped audition phrases, not seamless production loops. No Unreal import, game-state hooks,
spatial processing or headset/performance acceptance is established by this pack.</p>{"".join(cards)}
<p class="meta">Original project synthesis; no external recordings or sample libraries.
<a href="Manifest.json">Provenance and measurements</a> · <a href="Validation.json">Validation</a></p></main></html>''', encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, default=ROOT / "Artifacts/Toolchain/SuperCollider/SuperCollider")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--presets", type=Path, default=SOURCE / "presets.json")
    parser.add_argument("--ffmpeg", default=shutil.which("ffmpeg"))
    options = parser.parse_args()
    runtime = options.runtime.resolve()
    ffmpeg = options.ffmpeg
    ffprobe = shutil.which("ffprobe") or (str(Path(ffmpeg).with_name("ffprobe.exe")) if ffmpeg else None)
    if not ffmpeg or not ffprobe:
        raise RuntimeError("FFmpeg and FFprobe must be installed or provided on PATH")
    presets_path = options.presets.resolve()
    spec = json.loads(presets_path.read_text(encoding="utf-8"))
    validate_presets(spec)
    output = (options.output or ROOT / "Artifacts/D58" / datetime.now().strftime("Audition-%Y%m%d-%H%M%S")).resolve()
    if output.exists() and any(output.iterdir()):
        raise RuntimeError(f"Output is not empty; use a new path to preserve prior review packs: {output}")
    for folder in ("Work/Profile/Roaming", "Work/Profile/Local", "Work/Profile/Temp", "Logs", "Masters", "Preview", "Compare"):
        (output / folder).mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env.update({
        "APPDATA": str(output / "Work/Profile/Roaming"),
        "LOCALAPPDATA": str(output / "Work/Profile/Local"),
        "TEMP": str(output / "Work/Profile/Temp"), "TMP": str(output / "Work/Profile/Temp"),
    })
    # Windows sclang initializes Qt even for daemon jobs. Its shipped offscreen
    # backend crashes during class compilation; use its normal Windows backend.
    # CREATE_NO_WINDOW above keeps all CLI children hidden.
    env.pop("QT_QPA_PLATFORM", None)
    (output / "Work/sclang.yaml").write_text(
        "includePaths:\n  - " + json.dumps((runtime / "SCClassLibrary").as_posix())
        + "\nexcludePaths: []\npostInlineWarnings: false\n",
        encoding="utf-8",
    )
    for binary in ("sclang.exe", "scsynth.exe"):
        version = run([runtime / binary, "-v"]).stdout.decode("utf-8", errors="replace")
        if "3.14.1" not in version:
            raise RuntimeError(f"Unexpected SuperCollider version: {version}")
    metrics = {}
    audio_by_id = {}
    for c in spec["candidates"]:
        identity = c["id"]
        osc = make_score(c, output, runtime, env)
        raw = output / "Work" / f"{identity}-raw.wav"
        result = run([
            runtime / "scsynth.exe", "-N", osc, "_", raw, RATE, "WAV", "float",
            "-i", "0", "-o", "1", "-z", "64", "-D", "0", "-R", "0",
            "-U", runtime / "plugins",
        ], cwd=runtime, env=env, log=output / "Logs" / f"{identity}-scsynth.log")
        text = (result.stdout + result.stderr).decode("utf-8", errors="replace")
        if "ERROR" in text or "FAILURE" in text:
            raise RuntimeError(f"SC synthesis error for {identity}; see scsynth log")
        audio = read_audio(ffmpeg, ffprobe, raw)
        duration = c["parameters"]["duration"]
        count = round(duration * RATE)
        start = round(0.05 * RATE)
        if len(audio) < start + count:
            raise RuntimeError(f"Truncated SC render: {identity}")
        audio = audio[start:start + count].copy()
        # Guaranteed quiet endpoints for audition phrases; production loops are a later slice.
        fade = round(0.005 * RATE)
        curve = np.sin(np.linspace(0, math.pi / 2, fade)) ** 2
        audio[:fade] *= curve
        audio[-fade:] *= curve[::-1]
        unnormalized = output / "Work" / f"{identity}-unnormalized.wav"
        write_wave(unnormalized, audio)
        initial = measure(ffmpeg, unnormalized)
        gain_db = min(
            spec["targetLUFS"] - initial["input_i"],
            spec["truePeakCeilingDBTP"] - initial["input_tp"],
        )
        audio *= 10 ** (gain_db / 20)
        master = output / "Masters" / f"{identity}.wav"
        write_wave(master, audio)
        final = measure(ffmpeg, master)
        peak = float(np.max(np.abs(audio)))
        if abs(final["input_i"] - spec["targetLUFS"]) > 0.2:
            raise RuntimeError(f"Loudness target missed for {identity}: {final['input_i']}")
        if final["input_tp"] > spec["truePeakCeilingDBTP"] + 0.05:
            raise RuntimeError(f"True-peak ceiling exceeded: {identity}")
        if peak >= 0.5012 or abs(audio[0]) > 1e-8 or abs(audio[-1]) > 1e-8:
            raise RuntimeError(f"Peak/endpoint check failed: {identity}")
        spectral_peaks = []
        for seconds in np.linspace(1, duration - 1, 12):
            segment = audio[round(seconds * RATE):round(seconds * RATE) + 4096]
            spectrum = np.abs(np.fft.rfft(segment * np.hanning(len(segment))))
            spectral_peaks.append(float(np.fft.rfftfreq(len(segment), 1 / RATE)[np.argmax(spectrum)]))
        if max(spectral_peaks) - min(spectral_peaks) < 100:
            raise RuntimeError(f"Expected changing siren spectrum: {identity}")
        preview = output / "Preview" / f"{identity}.wav"
        write_wave(preview, audio, bits=16)
        mp3(ffmpeg, preview, preview.with_suffix(".mp3"))
        repeat = np.concatenate([audio, np.zeros(RATE // 4), audio, np.zeros(RATE // 4), audio])
        repeat_path = output / "Preview" / f"{identity}-Repeat.wav"
        write_wave(repeat_path, repeat, bits=16)
        mp3(ffmpeg, repeat_path, repeat_path.with_suffix(".mp3"))
        metrics[identity] = {
            "seconds": duration, "sampleRate": RATE, "channels": 1, "masterBits": 24,
            "integratedLUFS": final["input_i"], "truePeakDBTP": final["input_tp"],
            "samplePeak": peak, "rms": float(np.sqrt(np.mean(audio ** 2))),
            "staticGainDB": gain_db, "clippedSamples": int(np.count_nonzero(np.abs(audio) >= 1)),
            "sampledSpectralPeaksHz": spectral_peaks, "quietEndpoints": True,
            "masterSha256": sha(master), "master": master.relative_to(output).as_posix(),
        }
        audio_by_id[identity] = audio
        print(f"D58_RENDERED {identity} {final['input_i']:.2f} LUFS {final['input_tp']:.2f} dBTP", flush=True)
    if len({m["masterSha256"] for m in metrics.values()}) != len(metrics):
        raise RuntimeError("Two audition masters are identical")
    for group, members in grouped_candidates(spec).items():
        sequence = [np.zeros(round(RATE * 0.4))]
        for index, candidate in enumerate(members):
            if index:
                sequence.append(np.zeros(RATE))
            sequence.append(audio_by_id[candidate["id"]])
        compare = output / "Compare" / f"{comparison_name(group, members)}.wav"
        write_wave(compare, np.concatenate(sequence), bits=16)
        mp3(ffmpeg, compare, compare.with_suffix(".mp3"))
    manifest = {
        "task": "D58", "createdUTC": datetime.now(timezone.utc).isoformat(),
        "scope": "Offline emergency auditions; no Unreal integration or device acceptance",
        "synthesis": "SuperCollider Score OSC files rendered by scsynth non-realtime mode",
        "normalization": "Measured integrated LUFS, static gain only; no compression or limiting",
        "sourceSha256": {p.name: sha(p) for p in (
            presets_path, Path(__file__),
            SOURCE / "selections.json",
            *sorted({SOURCE / SYNTHS[c.get("synth", "Emergency")][0] for c in spec["candidates"]}),
            *([SOURCE / "town-references.json"] if any(c.get("synth") == "TownMechanical" for c in spec["candidates"]) else []),
        )},
        "toolVersions": {
            "supercollider": run([runtime / "scsynth.exe", "-v"]).stdout.decode().strip(),
            "ffmpeg": run([ffmpeg, "-version"]).stdout.decode().splitlines()[0],
            "numpy": np.__version__,
        },
        "provenance": spec["provenance"], "candidates": spec["candidates"], "metrics": metrics,
        "selection": spec.get("selection", {g: "Pending listening review" for g in grouped_candidates(spec)}),
        "listeningFeedback": json.loads((SOURCE / "selections.json").read_text(encoding="utf-8")),
    }
    (output / "Manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    files = {p.relative_to(output).as_posix(): sha(p) for folder in ("Masters", "Preview", "Compare") for p in (output / folder).iterdir()}
    validation = {
        "status": "Pass", "candidateCount": len(metrics), "derivativeFileCount": len(files) - len(metrics),
        "checks": ["Distinct candidate masters in declared comparison groups", "Mono 48 kHz exact phrase durations",
                   "Finite samples, zero clipping and quiet endpoints", "24-bit PCM masters",
                   "Integrated loudness within 0.2 LUFS of target", "True peak at or below -6 dBTP",
                   "Changing siren spectra", "Successful sclang score and scsynth output; no error markers"],
        "pending": ["Human recognizability, identity selection and repetition/fatigue review",
                    "Production loop/end behavior and Unreal event/spatial/mix integration",
                    "Desktop/PCVR/Quest listening and lifecycle checks; measured audio cost"],
        "sha256": files,
    }
    (output / "Validation.json").write_text(json.dumps(validation, indent=2), encoding="utf-8")
    review_html(output, spec, metrics)
    print(f"D58_AUDITION_PASS {output}", flush=True)


if __name__ == "__main__":
    main()
