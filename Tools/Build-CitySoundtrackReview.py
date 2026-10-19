"""Render/reuse nine requested tracks and assemble a portable soundtrack review playlist."""
from __future__ import annotations
import argparse
from datetime import datetime,timezone
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile
import importlib.util
import numpy as np
from AudioReview import write_review

sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/"AudioSource/Music/CitySoundtrack/Soundtrack1"
module_spec=importlib.util.spec_from_file_location("hybrid",ROOT/"Tools/Render-CityHybridMusic.py")
hybrid=importlib.util.module_from_spec(module_spec)
module_spec.loader.exec_module(hybrid)

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def duration(seconds):
    value=round(seconds)
    return f"{value//60}:{value%60:02d}"

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output",type=Path,default=ROOT/"Artifacts/D82/Soundtrack1")
    parser.add_argument("--album",type=Path,default=SOURCE/"Album.json")
    args=parser.parse_args()
    output=args.output.resolve()
    album=json.loads(args.album.read_text(encoding="utf-8"))
    if len(album["tracks"])!=9 or len({t["id"] for t in album["tracks"]})!=9:
        raise ValueError("Expected nine different track identities")
    for folder in ("MP3","Masters","Work"):
        (output/folder).mkdir(parents=True,exist_ok=True)
    records=[]
    verification=[]
    for number,track in enumerate(album["tracks"],1):
        pack=ROOT/track["reuse"] if "reuse" in track else output/"Work"/track["id"]
        manifest_path=pack/"Manifest.json"
        print(f'D82_ALBUM_START {number}/9 {track["title"]}',flush=True)
        if not manifest_path.exists():
            result=subprocess.run([sys.executable,str(ROOT/"Tools/Render-CityHybridMusic.py"),"--presets",str(ROOT/track["source"]),"--output",str(pack)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            (output/"Work"/(track["id"]+"-Build.log")).write_text(result.stdout+result.stderr,encoding="utf-8")
            if result.returncode:
                raise RuntimeError(f'{track["id"]} render failed: {(result.stdout+result.stderr)[-1800:]}')
        manifest=json.loads(manifest_path.read_text(encoding="utf-8"))
        # Reuse only verified packs; changes to the track preset need a new render destination.
        source_key=track["source"]
        if manifest["sources"].get(source_key)!=sha(ROOT/source_key):
            raise RuntimeError(f'{track["id"]} preset changed; preserve pack and render a new candidate')
        asset=manifest["assets"][0]
        if track.get("approvedMp3Sha256") and track["approvedMp3Sha256"]!=asset["mp3Sha256"]:
            raise RuntimeError("Approved recording identity changed")
        if not 120<=asset["seconds"]<=240 or asset["channels"]!=2 or asset["sampleRate"]!=48000:
            raise RuntimeError("Track outside requested duration/format")
        if abs(asset["levels"]["input_i"]+24)>1 or asset["levels"]["input_tp"]>-5.9:
            raise RuntimeError("Album mix outside established bounds")
        stem=pack/"Stems/Melody.wav"
        melody=hybrid.decode(shutil.which("ffmpeg"),stem)
        melody_rms=float(np.sqrt(np.mean(melody**2)))
        if melody_rms<0.001:
            raise RuntimeError("Melody stem unexpectedly quiet")
        events=manifest["events"]
        signatures=[]
        for pair in range(track["bars"]//2):
            signatures.append(tuple((round(e["beat"]-pair*8,3),e["synth"],round(e["args"]["freq"],3),round(e["args"].get("decay",-1),3)) for e in events if e["args"].get("out")==6 and pair*8<=e["beat"]<(pair+1)*8))
        unique=len(set(signatures))
        if unique<8:
            raise RuntimeError("Insufficient score variation in full track")
        sections=manifest["sections"]
        if sum(s["bars"] for s in sections)!=track["bars"] or len(sections)<6:
            raise RuntimeError("Incomplete arrangement map")
        mp3=pack/"Preview"/(asset["id"]+".mp3")
        wav=pack/"Masters"/(asset["id"]+".wav")
        if sha(mp3)!=asset["mp3Sha256"] or sha(wav)!=asset["masterSha256"]:
            raise RuntimeError("Render identity mismatch")
        filename=f'{number:02d}-{track["id"]}'
        shutil.copyfile(mp3,output/"MP3"/(filename+".mp3"))
        shutil.copyfile(wav,output/"Masters"/(filename+".wav"))
        records.append(dict(track,number=number,duration=duration(asset["seconds"]),seconds=asset["seconds"],mp3=f"MP3/{filename}.mp3",master=f"Masters/{filename}.wav",mp3Sha256=asset["mp3Sha256"],masterSha256=asset["masterSha256"],levels=asset["levels"],sections=sections,renderManifest=manifest_path.relative_to(ROOT).as_posix()))
        verification.append({"id":track["id"],"melodyRms":melody_rms,"melodyVsBedRmsDb":float(20*np.log10(melody_rms/manifest["stemRms"]["Texture"])),"distinctTwoBarPhraseSignatures":unique,"eventCounts":manifest["eventCounts"],"sources":manifest["sources"],"checks":"Native render/finite/frames,2–4m duration,headroom/mix levels,nonquiet melody,multiple sections/phrases and actual output identity"})
        (output/"Progress.json").write_text(json.dumps({"completed":len(records),"tracks":records},indent=2))
        print(f'D82_ALBUM_PASS {number}/9 {track["title"]} {records[-1]["duration"]}',flush=True)
    result=dict(album,createdUTC=datetime.now(timezone.utc).isoformat(),tracks=records,totalSeconds=sum(t["seconds"] for t in records),checks=verification)
    (output/"Manifest.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
    m3u="#EXTM3U\n"+"".join(f'#EXTINF:{round(t["seconds"])},{t["title"]}\n{t["mp3"]}\n' for t in records)
    (output/"CitySimVR-Initial-Soundtrack.m3u").write_text(m3u,encoding="utf-8")
    review=dict(result)
    if album.get("reviewSubset"):
        review["tracks"]=[t for t in records if t["id"] in album["reviewSubset"]]
        review["title"]=album.get("reviewTitle",album["title"])
    write_review(output,review)
    with zipfile.ZipFile(output/"CitySimVR-Initial-Soundtrack-Feedback-MP3.zip","w",compression=zipfile.ZIP_DEFLATED) as archive:
        for name in ["Review.html","ReviewCatalog.json","Manifest.json","CitySimVR-Initial-Soundtrack.m3u"]+[t["mp3"] for t in records]:
            archive.write(output/name,name)
    print(json.dumps({"status":"D82_NINE_TRACKS_READY","minutes":result["totalSeconds"]/60,"tracks":[{"title":t["title"],"duration":t["duration"],"bpm":t["bpm"]} for t in records]},indent=2),flush=True)

if __name__=="__main__":
    main()
