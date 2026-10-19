"""D82 ACE-Step texture / authored SuperCollider music, auditions and stem evidence."""
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
import numpy as np

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "AudioSource/Music/CitySoundtrack/Hybrid5"
RATE = 48000
LEAD_IN = 0.1
spec_module = importlib.util.spec_from_file_location("city_music",ROOT/"Tools/Render-CityMusic.py")
music = importlib.util.module_from_spec(spec_module)
spec_module.loader.exec_module(music)
helper = music.helper


def decode(ffmpeg, path, channels=2, filters=None):
    command = [ffmpeg,"-nostdin","-v","error","-i",path]
    if filters:
        command += ["-af",filters]
    command += ["-ar",RATE,"-ac",channels,"-f","f32le","-acodec","pcm_f32le","-"]
    samples = np.frombuffer(helper.run(command).stdout,dtype="<f4").reshape(-1,channels).astype(np.float64)
    if not len(samples) or not np.isfinite(samples).all():
        raise RuntimeError(f"Invalid decoded audio: {path}")
    return samples


def score_events(spec):
    if "albumProfile" in spec:
        module_spec=importlib.util.spec_from_file_location("city_soundtrack_score",ROOT/"Tools/CitySoundtrackScore.py")
        module=importlib.util.module_from_spec(module_spec)
        module_spec.loader.exec_module(module)
        return module.events(spec)
    if "sections" in spec:
        return full_score_events(spec)
    events = []
    rng = random.Random(spec["seed"])
    beat = 60/spec["bpm"]
    def add(bar, offset, synth, **args):
        events.append({"beat":bar*4+offset,"time":LEAD_IN+(bar*4+offset)*beat,"synth":synth,"args":args})
    def hz(midi):
        return 440*2**((midi-69)/12)
    for bar in range(spec["bars"]):
        chord = spec["chords"][(bar//2)%len(spec["chords"])]
        # Four guaranteed kicks, two backbeats and all eight hats in each bar.
        for index,offset in enumerate(spec["kickBeats"]):
            add(bar,offset,"HybridKick",out=0,amp=0.38 if index in (0,2) else 0.29,seed=spec["seed"]+bar*100+index)
        for index,offset in enumerate(spec["snareBeats"]):
            add(bar,offset,"HybridSnare",out=0,amp=0.18,seed=spec["seed"]+bar*100+10+index)
        for index,offset in enumerate(spec["hatBeats"]):
            add(bar,offset,"Hat",out=0,amp=0.046 if index%2==0 else 0.031,pan=-0.22 if index%2==0 else 0.18,seed=spec["seed"]+bar*100+20+index)
        # A small turnaround adds variation without removing the main groove.
        if bar in (3,7):
            add(bar,3.75,"HybridSnare",out=0,amp=0.065,seed=spec["seed"]+bar*100+40)
        for offset,length,interval in [(0,0.65,0),(1.5,0.3,0),(2,0.8,0),(3.25,0.35,7)]:
            add(bar,offset,"AnalogBass",out=2,freq=hz(chord[0]-12+interval),hold=length*beat,amp=0.11)
        root = chord[0]-12
        add(bar,0,"HybridTexture",out=4,buf=0,rate=2**((root-spec["sample"]["referenceMidi"])/12),
            hold=4*beat-0.25,amp=spec["sample"]["gain"]*(0.85 if bar<2 else 1))
        if bar%2==0:
            for index,(offset,pitch) in enumerate(spec["phrases"][(bar//2)%len(spec["phrases"])]):
                add(bar,offset,"HybridBell",out=2,freq=hz(pitch),amp=0.07 if index%2==0 else 0.055,pan=rng.choice([-0.2,0.2]))
    return sorted(events,key=lambda event:event["time"])


def full_score_events(spec):
    events=[]
    beat=60/spec["bpm"]
    def hz(midi):
        return 440*2**((midi-69)/12)
    def add(bar,offset,synth,**args):
        if bar*4+offset < spec["bars"]*4:
            events.append({"beat":bar*4+offset,"time":LEAD_IN+(bar*4+offset)*beat,"synth":synth,"args":args})
    for section in spec["sections"]:
        for local in range(section["bars"]):
            bar=section["startBar"]+local
            chord=spec["progressions"][section["progression"]][(local//2)%4]
            density=section["density"]
            # The lighter bridge keeps a pulse; two bars rebuild before the return.
            reduced=density=="break" and local<6
            ending=density=="outro" and local>=2
            kick=[0,2] if reduced or ending else spec["kickBeats"]
            for i,offset in enumerate(kick):
                add(bar,offset,"HybridKick",out=0,amp=(0.38 if offset in (0,2) else 0.29)*section["drums"],seed=spec["seed"]+bar*100+i)
            for i,offset in enumerate(spec["snareBeats"]):
                add(bar,offset,"HybridSnare",out=0,amp=0.18*section["drums"]*(0.65 if reduced or ending else 1),seed=spec["seed"]+bar*100+10+i)
            hats=spec["hatBeats"][::2] if reduced or ending or density=="sparse" else spec["hatBeats"]
            for i,offset in enumerate(hats):
                add(bar,offset,"Hat",out=0,amp=(0.046 if i%2==0 else 0.031)*section["drums"],pan=-0.22 if i%2==0 else 0.18,seed=spec["seed"]+bar*100+20+i)
            if local%4==3 and not ending and density!="sparse":
                for i,offset in enumerate([3.5,3.75] if local==section["bars"]-1 else [3.75]):
                    add(bar,offset,"HybridSnare",out=0,amp=(0.05+0.015*i)*section["drums"],seed=spec["seed"]+bar*100+40+i)
            pattern=[(0,1.3,0),(2,1.2,0)] if reduced or ending else [(0,0.65,0),(1.5,0.3,0),(2,0.8,0),(3.25,0.35,7)]
            for offset,length,interval in pattern:
                # Occasional octave pickup changes the bass response, preserving grid attacks.
                if section.get("answer") and local%4==3 and offset==3.25:
                    interval=12
                add(bar,offset,"AnalogBass",out=2,freq=hz(chord[0]-12+interval),hold=length*beat,amp=0.11*section["bass"])
            add(bar,0,"HybridTexture",out=4,buf=0,rate=2**((chord[0]-12-spec["sample"]["referenceMidi"])/12),hold=4*beat-0.25,amp=spec["sample"]["gain"]*section["texture"])
            if local%2==0:
                phrase=spec["melodies"][section["melody"]][(local//2)%4]
                if density in ("sparse","break","outro"):
                    phrase=phrase[::2]
                if local>=8 and section["melody"]=="main":
                    # Change the last response of the repeated theme rather than clone it.
                    phrase=phrase[:-1]+[(6,chord[1]+12),(7,phrase[0][1])]
                for i,note in enumerate(phrase):
                    offset,pitch=note[:2]
                    articulation={"decay":note[2]*beat} if len(note)==3 else {}
                    add(bar,offset,"FullPluck" if section.get("pluck") else "FullBell",out=6,freq=hz(pitch),amp=(0.07 if i%2==0 else 0.055)*section["lead"],pan=-0.18 if i%2==0 else 0.18,toneHz=section["tone"],**articulation,**({"seed":spec["seed"]+bar*100+60+i} if section.get("pluck") else {}))
                if section.get("answer") and local%4==2:
                    add(bar,7.5,"FullBell",out=6,freq=hz(chord[2]+12),amp=0.035,pan=-0.3,toneHz=1500)
    return sorted(events,key=lambda event:event["time"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--presets",type=Path,default=SOURCE/"Composition.json")
    parser.add_argument("--runtime",type=Path,default=ROOT/"Artifacts/Toolchain/SuperCollider/SuperCollider")
    args = parser.parse_args()
    pack,runtime = args.output.resolve(),args.runtime.resolve()
    if pack.exists() and any(pack.iterdir()):
        raise RuntimeError("Use an empty audition directory; preserve prior evidence")
    preset=args.presets.resolve()
    source_dir=preset.parent
    spec = json.loads(preset.read_text(encoding="utf-8"))
    full="sections" in spec
    max_bpm=120 if "albumProfile" in spec else 100
    if spec["schema"]!=1 or not 60<=spec["bpm"]<=max_bpm or not 4<=spec["bars"]<=96:
        raise ValueError("Invalid composition settings")
    if full:
        next_bar=0
        for section in spec["sections"]:
            if section["startBar"]!=next_bar or section["bars"]<=0:
                raise ValueError("Sections must cover the timeline without gaps/overlap")
            next_bar+=section["bars"]
        if next_bar!=spec["bars"]:
            raise ValueError("Sections must cover all bars")
        for bank in spec["melodies"].values():
            for phrase in bank:
                for note in phrase:
                    if len(note) not in (2,3) or not 0<=note[0]<8 or not isinstance(note[1],int) or not 0<=note[1]<=127:
                        raise ValueError("Invalid melody event")
                    if len(note)==3 and (not math.isfinite(note[2]) or not 0.125<=note[2]<=4):
                        raise ValueError("Invalid melodic decay in beats")
    sampled=spec["sample"].get("mode","sample")!="synthBed"
    source = (source_dir/spec["sample"]["file"]).resolve() if sampled else None
    if sampled and helper.sha(source)!=spec["sample"]["sha256"]:
        raise RuntimeError("Generated source hash changed")
    ffmpeg,ffprobe = shutil.which("ffmpeg"),shutil.which("ffprobe")
    if not ffmpeg or not ffprobe:
        raise RuntimeError("Use the verified FFmpeg toolchain")
    for folder in ["Work/Profile/Roaming","Work/Profile/Local","Work/Temp","Logs","Masters","Preview","Stems"]:
        (pack/folder).mkdir(parents=True,exist_ok=True)
    shutil.copyfile(preset,pack/"Work/Composition.json")
    settings = spec["sample"]
    prepared,buffer_path,anchor,cents=None,None,None,None
    if sampled:
        original = decode(ffmpeg,source)
        start,end = round(settings["trimStartSeconds"]*RATE),round((settings["trimStartSeconds"]+settings["trimSeconds"])*RATE)
        prepared = decode(ffmpeg,source,filters=f'highpass=f={settings["highpassHz"]},lowpass=f={settings["lowpassHz"]}')[start:end].copy()
        if len(prepared)!=round(settings["trimSeconds"]*RATE):
            raise RuntimeError("Texture trim exceeds source")
        fade = round(0.12*RATE)
        prepared[:fade] *= np.linspace(0,1,fade)[:,None]
        prepared[-fade:] *= np.linspace(1,0,fade)[:,None]
        source_peak = float(np.abs(prepared).max())
        if source_peak<0.001:
            raise RuntimeError("Texture is silent")
        prepared *= settings["preparedPeak"]/source_peak
        buffer_path = pack/"Work/PreparedTexture.wav"
        music.write_wave(buffer_path,prepared,24)
        mono = original.mean(axis=1)
        window=mono[RATE:RATE+24000]*np.hanning(24000)
        spectrum=abs(np.fft.rfft(window))
        frequencies=np.fft.rfftfreq(len(window),1/RATE)
        indices=np.flatnonzero((frequencies>=80)&(frequencies<=150))
        anchor=float(frequencies[indices[np.argmax(spectrum[indices])]])
        expected=440*2**((settings["referenceMidi"]-69)/12)
        cents=1200*math.log2(anchor/expected)
        if abs(cents)>35:
            raise RuntimeError(f"Texture pitch anchor differs from score: {anchor}Hz")
    env = os.environ.copy()
    for key,value in {"APPDATA":"Work/Profile/Roaming","LOCALAPPDATA":"Work/Profile/Local","TEMP":"Work/Temp","TMP":"Work/Temp"}.items():
        env[key]=str(pack/value)
    env.pop("QT_QPA_PLATFORM",None)
    (pack/"Work/sclang.yaml").write_text(f'includePaths:\n  - "{(runtime/"SCClassLibrary").as_posix()}"\nexcludePaths: []\npostInlineWarnings: false\n')
    events=score_events(spec)
    duration=spec["bars"]*4*60/spec["bpm"]
    commands=[]
    for index,event in enumerate(events):
        arguments=", ".join(f'"{key}", {value:.9g}' for key,value in event["args"].items())
        commands.append(f'[{event["time"]:.9f}, [\\s_new, \\D82{event["synth"]}, {1000+index}, 0, 1, {arguments}]]')
    commands.append(f'[{LEAD_IN+duration+0.1:.9f}, [\\c_set, 0, 0]]')
    osc,job=pack/"Work/Hybrid.osc",pack/"Work/Hybrid.scd"
    synth=source_dir/spec.get("synthSource","HybridSynths.scd")
    buffer_commands='[[0.01, [\\b_allocRead, 0, %s, 0, -1]]]'%json.dumps(buffer_path.as_posix()) if sampled else '[]'
    job.write_text('''(
try {
    var score;
    %s.load;
    Archive.archiveDir = %s;
    score = Score(~d82Defs.collect { |definition| [0, [\\d_recv, definition.asBytes]] }
        ++ %s ++ [%s]);
    score.write(%s);
    "D82_HYBRID_SCORE_READY".postln;
    0.exit;
} { |error| error.reportError; 1.exit };
)
'''%(json.dumps(synth.as_posix()),json.dumps((pack/"Work/Profile").as_posix()),buffer_commands,",\n".join(commands),json.dumps(osc.as_posix())))
    result=helper.run([runtime/"sclang.exe","-D","-a","-l",pack/"Work/sclang.yaml","-d",pack/"Work/Profile",job],env=env,cwd=runtime,timeout=90,log=pack/"Logs/Score.log")
    if b"D82_HYBRID_SCORE_READY" not in result.stdout or b"ERROR:" in result.stdout:
        raise RuntimeError("Hybrid score failed")
    channels=8 if full else 6
    raw=pack/f"Work/Hybrid-{channels}-channel.wav"
    result=helper.run([runtime/"scsynth.exe","-N",osc,"_",raw,RATE,"WAV","float","-i",0,"-o",channels,"-z",64,"-D",0,"-R",0,"-U",runtime/"plugins"],env=env,cwd=runtime,timeout=180,log=pack/"Logs/Render.log")
    if any(word in result.stdout+result.stderr for word in [b"FAILURE",b"ERROR"]):
        raise RuntimeError("NRT reported a buffer/synth error")
    info=json.loads(helper.run([ffprobe,"-v","error","-show_streams","-of","json",raw]).stdout)["streams"][0]
    if info["channels"]!=channels or int(info["sample_rate"])!=RATE:
        raise RuntimeError("Unexpected stereo-bus NRT format")
    frames=round(duration*RATE)
    all_stems=decode(ffmpeg,raw,channels)[round(LEAD_IN*RATE):round(LEAD_IN*RATE)+frames]
    if len(all_stems)!=frames:
        raise RuntimeError("Incorrect NRT frame count")
    # Identical endpoint fades preserve phase/timing in all stem combinations.
    all_stems[:480] *= np.linspace(0,1,480)[:,None]
    all_stems[-round(1.2*RATE):] *= np.linspace(1,0,round(1.2*RATE))[:,None]
    drums,authored,texture=[all_stems[:,i:i+2].copy() for i in (0,2,4)]
    melody=all_stems[:,6:8].copy() if full else None
    bass=authored.copy() if full else None
    if full:
        authored+=melody
        if float(np.sqrt(np.mean(melody**2)))<0.001:
            raise RuntimeError("Full melody stem unexpectedly quiet; inspect effects routing/gain")
    hybrid=drums+authored+texture
    groove=drums+authored
    def master(identity,samples):
        peak=float(np.abs(samples).max())
        samples=samples*min(1,0.9/max(peak,1e-8))
        pre=pack/f"Work/{identity}-pre.wav"
        music.write_wave(pre,samples,24)
        levels=helper.measure(ffmpeg,pre)
        gain_db=min(spec["targetLUFS"]-levels["input_i"],spec["truePeakCeilingDBTP"]-levels["input_tp"]-0.1)
        samples*=10**(gain_db/20)
        path=pack/f"Masters/{identity}.wav"
        music.write_wave(path,samples,24)
        mp3=pack/f"Preview/{identity}.mp3"
        helper.mp3(ffmpeg,path,mp3)
        delivered=helper.measure(ffmpeg,mp3)
        # Use actual encoded headroom for one bounded static-gain correction.
        # This only changes the fresh render's own files; earlier packs are immutable.
        if delivered["input_i"]<spec["targetLUFS"]-1:
            correction=min(spec["targetLUFS"]-delivered["input_i"],spec["truePeakCeilingDBTP"]-delivered["input_tp"]-0.1)
            if correction>0:
                samples*=10**(correction/20)
                music.write_wave(path,samples,24)
                adjusted=pack/f"Preview/{identity}-Adjusted.mp3"
                helper.mp3(ffmpeg,path,adjusted)
                adjusted.replace(mp3)
                delivered=helper.measure(ffmpeg,mp3)
        if abs(delivered["input_i"]-spec["targetLUFS"])>1 or delivered["input_tp"]>spec["truePeakCeilingDBTP"]+0.1:
            raise RuntimeError(f"Mix outside audition bounds: {delivered}")
        return {"id":identity,"frames":len(samples),"seconds":len(samples)/RATE,"sampleRate":RATE,"channels":2,"levels":delivered,"masterSha256":helper.sha(path),"mp3Sha256":helper.sha(mp3)}
    identity=spec.get("id","NeonCourtyard")
    records=[master(identity+"-Hybrid",hybrid)]
    if "albumProfile" not in spec:
        records.append(master(identity+"-GrooveOnly",groove))
    if sampled:
        records.append(master("GeneratedTexture-Prepared",prepared))
    shared_gain=min(1,0.9/max(float(np.abs(hybrid).max()),1e-8))
    for name,stem in [("Drums",drums),("AuthoredBassAndBell",authored),("GeneratedTexture",texture)]:
        music.write_wave(pack/f"Stems/{name}.wav",stem*shared_gain,24)
    if full:
        music.write_wave(pack/"Stems/Melody.wav",melody*shared_gain,24)
        music.write_wave(pack/"Stems/Bass.wav",bass*shared_gain,24)
    counts={name:sum(event["synth"]==name for event in events) for name in sorted({event["synth"] for event in events})}
    if not full:
        assert counts["HybridKick"]==spec["bars"]*4 and counts["HybridSnare"]==spec["bars"]*2+2
        assert counts["Hat"]==spec["bars"]*8
    if sampled:
        assert counts["HybridTexture"]==spec["bars"]
    for name,stem in [("Drums",drums),("Authored",authored),("Texture",texture)]:
        if float(np.sqrt(np.mean(stem**2)))<0.0001:
            raise RuntimeError(f"Missing audible-data stem: {name}")
    dependencies=[preset,synth,SOURCE.parent/"DreamSynths.scd",SOURCE.parent/"VarietySynths.scd",Path(__file__)]
    if sampled:
        dependencies.extend([source,(source_dir/settings.get("provenance","GeneratedSource.json")).resolve()])
    if full:
        dependencies.append(SOURCE/"HybridSynths.scd")
        dependencies.extend((source_dir/path).resolve() for path in spec.get("synthDependencies",[]))
    manifest={"task":"D82","schema":1,"createdUTC":datetime.now(timezone.utc).isoformat(),"status":"Full track audition; listening approval pending" if full else "Short hybrid audition; listening approval pending","title":spec["title"],"bpm":spec["bpm"],"bars":spec["bars"],"assets":records,
        "method":f"SuperCollider NRT: authored drums/bass/melody and {'pitched ACE-Step texture' if sampled else 'original oscillator pad'}. {channels//2} stereo buses; offline static mix/gain.",
        "sources":{path.relative_to(ROOT).as_posix():helper.sha(path) for path in dependencies},
        "preparedTexture":{"file":"Work/PreparedTexture.wav","sha256":helper.sha(buffer_path),"trim":settings,"measuredLowPeakHz":anchor,"anchorErrorCents":cents} if sampled else {"mode":"original synthesis; no recorded or generated sample"},
        "scoreSha256":helper.sha(osc),"eventCounts":counts,"events":events,
        "stemRms":{name:float(np.sqrt(np.mean(stem**2))) for name,stem in [("Drums",drums),("Authored",authored),("Texture",texture)]},
        "sections":[dict(section,startSeconds=section["startBar"]*4*60/spec["bpm"]) for section in spec.get("sections",[])],
        "checks":["Native NRT without synth/buffer errors",f"{channels}-channel48kHz frame/finite validation","All main stems nonzero","Authored event counts recorded","Preview LUFS and true-peak checks"],
        "limitations":["No assistant perceptual listening or separate audio-review API","Pitch-anchor analysis is not proof of isolated single-note material","User audition determines musical quality/beat balance","Not imported into native game; no headset claim"]}
    (pack/"Manifest.json").write_text(json.dumps(manifest,indent=2))
    print(json.dumps({"status":"D82_HYBRID_PASS","assets":records,"eventCounts":counts,"stemRms":manifest["stemRms"]},indent=2),flush=True)


if __name__=="__main__":
    main()
