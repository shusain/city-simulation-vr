"""D82 original sample-free SuperCollider NRT music and review derivatives."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import importlib.util
import json
import math
import os
from pathlib import Path
import random
import shutil
import sys
import wave
import numpy as np

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "AudioSource/Music/CitySoundtrack"
RATE = 48000
helper_spec = importlib.util.spec_from_file_location("city_audio_render", ROOT / "Tools/Render-CityAudioAuditions.py")
helper = importlib.util.module_from_spec(helper_spec)
helper_spec.loader.exec_module(helper)

def write_wave(path, samples, bits):
    if samples.ndim != 2 or samples.shape[1] != 2 or not np.isfinite(samples).all() or np.abs(samples).max() >= 1:
        raise RuntimeError(f"Invalid stereo audio: {path}")
    values = np.rint(samples * (2**(bits-1)-1)).astype("<i4")
    payload = values.view(np.uint8).reshape(-1, 4)[:, :3].copy().tobytes() if bits == 24 else values.astype("<i2").tobytes()
    with wave.open(str(path), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(bits//8)
        output.setframerate(RATE)
        output.writeframes(payload)

def events_for(track):
    if track.get("arrangement", "classic") != "classic":
        return variety_events_for(track)
    rng = random.Random(track["seed"])
    beat = 60/track["bpm"]
    events = []
    def note(time, synth, **args):
        events.append((0.05 + time*beat, synth, args))
    def frequency(midi):
        return 440*2**((midi-69)/12)
    for bar in range(track["bars"]):
        chord = track["chords"][(bar//2)%4]
        start = bar*4
        # Intro/outro and a short central breakdown give each score breathing room.
        if bar % 2 == 0 and bar < 30:
            for i, pitch in enumerate(chord):
                note(start, "Pad", freq=frequency(pitch), hold=8*beat-1.3,
                     amp=0.065, pan=(i-2)*0.3, seed=track["seed"]+bar*10+i)
        if 2 <= bar < 30:
            for offset, length in [(0, 1.2), (2.65, 0.7)]:
                note(start+offset, "Bass", freq=frequency(chord[0]-12), hold=length*beat, amp=0.13)
        if 4 <= bar < 28 and not 16 <= bar < 20:
            for offset in [0, 1.7 if bar%2 else 2.5]:
                note(start+offset, "Kick", amp=0.19)
            for offset in [1.04, 3.04]:
                note(start+offset, "Snare", amp=0.095, seed=track["seed"]+bar*30+int(offset*10))
            for step in range(8):
                if rng.random() < 0.16:
                    continue
                offset = step*0.5 + (0.06 if step%2 else 0) + rng.uniform(-0.018, 0.018)
                note(start+offset, "Hat", amp=rng.uniform(0.016, 0.025), pan=rng.uniform(-0.35, 0.35),
                     seed=track["seed"]+bar*100+step)
        if 4 <= bar < 30 and bar%2 == 0:
            motif = track["melody"]
            for i, offset in enumerate(track.get("melodyBeats", [0.5, 1.65, 3.0, 5.15])):
                pitch = motif[(bar//2+i)%len(motif)]
                note(start+offset, "Key", freq=frequency(pitch), amp=0.11 if i%2 else 0.13,
                     pan=[-0.25,0.25,0.1,-0.1][i], seed=track["seed"]+bar*10+i)
    return sorted(events, key=lambda e:e[0])

def variety_events_for(track):
    """Two authored arrangements with separate rhythm, voicing and instrument identities."""
    rng = random.Random(track["seed"])
    beat = 60/track["bpm"]
    lantern = track["arrangement"] == "lantern"
    events = []
    def note(time, synth, **args):
        events.append((0.05+time*beat, synth, args))
    def frequency(midi):
        return 440*2**((midi-69)/12)
    for bar in range(track["bars"]):
        start = bar*4
        chord = track["chords"][(bar//2)%4]
        if bar%2 == 0 and bar < 30:
            for i,pitch in enumerate(chord):
                note(start,"WarmChord",freq=frequency(pitch),hold=8*beat-0.4,
                     amp=0.026 if lantern else 0.038,pan=(i-2)*0.27)
        if lantern and 2 <= bar < 30:
            # A picked accompaniment, separate from the higher wooden lead.
            for i,offset in enumerate([0,0.75,1.5,2.5,3.25]):
                pitch = chord[[0,2,3,1,4][i]] + (12 if bar%4 == 3 and i == 4 else 0)
                note(start+offset,"Pluck",freq=frequency(pitch),amp=0.23 if i%2 == 0 else 0.17,
                     pan=-0.32 if i%2 == 0 else 0.26,seed=track["seed"]+bar*10+i)
        if 2 <= bar < 30:
            bass_notes = [(0,0),(1.5,7),(2.5,0),(3.25,12)] if lantern else [(0,0),(1.5,0),(2.75,7)]
            for offset,interval in bass_notes:
                note(start+offset,"Bass" if lantern else "AnalogBass",freq=frequency(chord[0]-12+interval),
                     hold=(0.35 if lantern else 0.5)*beat,amp=0.115)
        if 4 <= bar < 28 and not 16 <= bar < 20:
            for offset in ([0,2.5] if lantern else [0,1.5,2.75]):
                note(start+offset,"Kick",amp=0.14 if lantern else 0.19)
            for i,offset in enumerate([1,3]):
                note(start+offset,"Stick" if lantern else "Clap",amp=0.095,
                     seed=track["seed"]+bar*20+i)
            for i in range(8):
                if rng.random() < 0.2:
                    continue
                offset=i*0.5 + (0.035 if i%2 else 0)
                note(start+offset,"Shaker",amp=rng.uniform(0.035,0.06),pan=0.24 if i%2 else -0.2,
                     seed=track["seed"]+bar*100+i)
        if 4 <= bar < 30 and bar%2 == 0 and not 16 <= bar < 20:
            phrase = track["phrases"][(bar//2)%len(track["phrases"])]
            for i,(offset,pitch) in enumerate(phrase):
                note(start+offset,"Marimba" if lantern else "Ribbon",freq=frequency(pitch),
                     amp=(0.13 if lantern else 0.075)*(0.9 if i%2 else 1),pan=-0.13 if i%2 else 0.16)
            # Final section answers the motif once with a different register/texture.
            if 24 <= bar < 28:
                note(start+7,"Pluck" if lantern else "Marimba",freq=frequency(chord[2]+12),
                     amp=0.14 if lantern else 0.08,pan=-0.3,**({"seed":track["seed"]+bar} if lantern else {}))
    return sorted(events,key=lambda event:event[0])

def render(track, pack, runtime, env, spec, ffmpeg, ffprobe, synth_path):
    identity = track["id"]
    duration = track["bars"]*4*60/track["bpm"]
    osc = pack / f"Work/{identity}.osc"
    job = pack / f"Work/{identity}.scd"
    lines = []
    for index, (time, kind, args) in enumerate(events_for(track)):
        arguments = ", ".join(f'"{key}", {value:.9g}' for key,value in args.items())
        lines.append(f'[{time:.9f}, [\\s_new, \\D82{kind}, {1000+index}, 0, 1, {arguments}]]')
    lines.append(f'[{duration+0.1:.9f}, [\\c_set, 0, 0]]')
    job.write_text('''(
try {
    var score;
    %s.load;
    Archive.archiveDir = %s;
    score = Score(~d82Defs.collect { |definition| [0.0, [\\d_recv, definition.asBytes]] } ++ [%s]);
    score.write(%s);
    "D82_SCORE_READY".postln;
    0.exit;
} { |error| error.reportError; 1.exit };
)
''' % (json.dumps(synth_path.as_posix()), json.dumps((pack/"Work/Profile").as_posix()),
        ",\n".join(lines), json.dumps(osc.as_posix())), encoding="utf-8")
    result = helper.run([runtime/"sclang.exe", "-D", "-a", "-l", pack/"Work/sclang.yaml",
                         "-d", pack/"Work/Profile", job], env=env, cwd=runtime, timeout=90,
                        log=pack/f"Logs/{identity}-score.log")
    if b"D82_SCORE_READY" not in result.stdout or b"ERROR:" in result.stdout or not osc.is_file():
        raise RuntimeError(f"Score failed: {identity}")
    raw = pack/f"Work/{identity}-float.wav"
    result = helper.run([runtime/"scsynth.exe", "-N", osc, "_", raw, RATE, "WAV", "float", "-i", 0, "-o", 2,
                        "-z", 64, "-D", 0, "-R", 0, "-U", runtime/"plugins"], env=env, cwd=runtime,
                       timeout=180, log=pack/f"Logs/{identity}-render.log")
    diagnostics = result.stdout + result.stderr
    if b"FAILURE" in diagnostics or b"ERROR" in diagnostics:
        raise RuntimeError(f"NRT failed: {identity}")
    info = json.loads(helper.run([ffprobe,"-v","error","-show_streams","-of","json",raw]).stdout)["streams"][0]
    if info["channels"] != 2 or int(info["sample_rate"]) != RATE:
        raise RuntimeError("Unexpected NRT format")
    decoded = helper.run([ffmpeg,"-nostdin","-v","error","-i",raw,"-f","f32le","-acodec","pcm_f32le","-"]).stdout
    samples = np.frombuffer(decoded,dtype="<f4").reshape(-1,2).astype(np.float64)
    frames = round(duration*RATE)
    samples = samples[round(0.05*RATE):round(0.05*RATE)+frames].copy()
    if len(samples) != frames or not np.isfinite(samples).all():
        raise RuntimeError("Invalid render length/data")
    samples[:RATE] *= np.linspace(0,1,RATE)[:,None]
    tail = RATE*3
    samples[-tail:] *= np.linspace(1,0,tail)[:,None]
    pre = pack/f"Work/{identity}-pre.wav"
    write_wave(pre,samples,24)
    measured = helper.measure(ffmpeg,pre)
    gain_db = min(spec["targetLUFS"]-measured["input_i"], spec["truePeakCeilingDBTP"]-measured["input_tp"]-0.05)
    samples *= 10**(gain_db/20)
    master = pack/f"Masters/{identity}.wav"
    production = pack/f"Unreal/{identity}.wav"
    write_wave(master,samples,24)
    write_wave(production,samples,16)
    helper.mp3(ffmpeg,master,pack/f"Preview/{identity}.mp3")
    final = helper.measure(ffmpeg,production)
    if final["input_tp"] > spec["truePeakCeilingDBTP"]+0.05 or abs(final["input_i"]-spec["targetLUFS"]) > 1:
        raise RuntimeError(f"Music mix outside delivery bounds: {final}")
    return {"id":identity,"title":track["title"],"seconds":frames/RATE,"frames":frames,"channels":2,
            "sampleRate":RATE,"bits":16,"integratedLUFS":final["input_i"],"truePeakDBTP":final["input_tp"],
            "loudnessRangeLU":final["input_lra"],"file":f"{identity}.wav","sha256":helper.sha(production),
            "masterSha256":helper.sha(master),"scoreSha256":helper.sha(osc),
            "package":f"/Game/CitySim/Audio/Music/SW_Music_{identity}"}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--runtime",type=Path,default=ROOT/"Artifacts/Toolchain/SuperCollider/SuperCollider")
    parser.add_argument("--presets",type=Path,default=SOURCE/"Presets.json")
    args = parser.parse_args()
    pack, runtime = args.output.resolve(), args.runtime.resolve()
    if pack.exists() and any(pack.iterdir()):
        raise RuntimeError("Render output must be empty; preserve previous auditions")
    preset_path = args.presets.resolve()
    spec = json.loads(preset_path.read_text(encoding="utf-8"))
    if spec["schema"] != 1 or spec["sampleRate"] != RATE or not 1 <= len(spec["tracks"]) <= 8:
        raise ValueError("Invalid preset version/count")
    synth_name = spec.get("synthSource", "DreamSynths.scd")
    if synth_name not in {"DreamSynths.scd", "VarietySynths.scd", "SoftLeadSynths.scd"}:
        raise ValueError("Unknown instrument source")
    synth_path = SOURCE/synth_name
    for track in spec["tracks"]:
        if not track["id"].isalnum() or not 60 <= track["bpm"] <= 100 or track["bars"] != 32:
            raise ValueError("Invalid track preset")
        beats = track.get("melodyBeats", [0.5, 1.65, 3.0, 5.15])
        if len(beats) != 4 or not all(isinstance(value, (int, float)) and math.isfinite(value) and 0 <= value < 8 for value in beats):
            raise ValueError("Expected four finite melody beats in each two-bar phrase")
        arrangement = track.get("arrangement", "classic")
        if arrangement not in {"classic", "lantern", "transit"}:
            raise ValueError("Unknown arrangement")
        if arrangement != "classic":
            if synth_name not in {"VarietySynths.scd", "SoftLeadSynths.scd"} or len(track.get("phrases", [])) != 4:
                raise ValueError("Variety arrangement needs its instruments and four phrases")
            for phrase in track["phrases"]:
                for offset,pitch in phrase:
                    if not 0 <= offset < 8 or not isinstance(pitch,int) or not 0 <= pitch <= 127:
                        raise ValueError("Invalid authored phrase")
    for folder in ["Work/Profile/Roaming","Work/Profile/Local","Work/Temp","Logs","Masters","Preview","Unreal"]:
        (pack/folder).mkdir(parents=True,exist_ok=True)
    ffmpeg, ffprobe = shutil.which("ffmpeg"), shutil.which("ffprobe")
    if not ffmpeg or not ffprobe:
        raise RuntimeError("Use verified FFmpeg/FFprobe environment")
    env = os.environ.copy()
    for key,value in {"APPDATA":"Work/Profile/Roaming","LOCALAPPDATA":"Work/Profile/Local","TEMP":"Work/Temp","TMP":"Work/Temp"}.items():
        env[key] = str(pack/value)
    env.pop("QT_QPA_PLATFORM",None)
    (pack/"Work/sclang.yaml").write_text(f'includePaths:\n  - "{(runtime/"SCClassLibrary").as_posix()}"\nexcludePaths: []\npostInlineWarnings: false\n')
    records = []
    for track in spec["tracks"]:
        records.append(render(track,pack,runtime,env,spec,ffmpeg,ffprobe,synth_path))
        print(f'D82_RENDER_PASS {track["id"]}',flush=True)
    manifest = {"schema":1,"createdUTC":datetime.now(timezone.utc).isoformat(),"task":"D82",
                "status":"Draft originals; listening approval pending", "method":"SuperCollider 3.14.1 NRT; oscillator/seeded-noise instruments; static gain; no samples, voices or third-party music",
                "presetSource":preset_path.relative_to(ROOT).as_posix(),
                "sources":{name:helper.sha(path) for name,path in {
                    preset_path.name:preset_path,"DreamSynths.scd":SOURCE/"DreamSynths.scd",synth_name:synth_path}.items()},"assets":records}
    if synth_name == "SoftLeadSynths.scd":
        manifest["sources"]["VarietySynths.scd"] = helper.sha(SOURCE/"VarietySynths.scd")
    (pack/"Manifest.json").write_text(json.dumps(manifest,indent=2),encoding="utf-8")
    cards = "".join(f'<section><h2>{t["title"]}</h2><p>{t["intent"]}</p><audio controls preload="none" src="Preview/{t["id"]}.mp3"></audio></section>' for t in spec["tracks"])
    (pack/"Review.html").write_text(f'<!doctype html><html lang="en"><meta charset="utf-8"><title>D82 dreamy music auditions</title><style>body{{font:17px/1.6 system-ui;background:#172039;color:#e2e8fc;max-width:800px;margin:auto;padding:32px}}section{{padding:20px;margin:20px 0;background:#25304a;border-radius:12px}}audio{{width:100%}}</style><h1>Dreamy ambient / electronic beats</h1><p>{len(spec["tracks"])} draft original scores. Listen for mood, repetition and distraction. Game default music volume is25%; these previews use the full rendered level (-24 LUFS).</p>{cards}',encoding="utf-8")

if __name__ == "__main__":
    main()
