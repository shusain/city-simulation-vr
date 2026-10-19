"""Apply the submitted D82 feedback to two fresh candidates; reuse seven approvals."""
import copy
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
OLD=ROOT/"Artifacts/D82/Soundtrack1"
SOURCE=ROOT/"AudioSource/Music/CitySoundtrack/Soundtrack2"


def section(name,length,start,melody="main",progression="main",density="full",synth=None,lift=False):
    value={"name":name,"startBar":start,"bars":length,"progression":progression,"melody":melody,
           "density":density,"drums":0.8 if density!="full" else 1,"bass":0.85 if density!="full" else 1,
           "texture":0.75 if density=="full" else 0.9,"lead":0.85 if density!="full" else 1,"tone":2400}
    if synth:value["leadSynth"]=synth
    if lift:value["lift"]=True
    return value


def main():
    targets=[SOURCE/name for name in ("Afterglow.json","LanternWalk.json","Album.json")]
    if any(path.exists() for path in targets):
        raise RuntimeError("Preserve existing revision source; edit deliberately")
    catalog=json.loads((OLD/"ReviewCatalog.json").read_text())
    submissions=[(json.loads(p.read_text()),p) for p in (OLD/"Feedback"/catalog["reviewId"]).glob("*.json")]
    feedback,receipt=max(submissions,key=lambda pair:pair[0]["submittedUTC"])
    original=json.loads((OLD/"Manifest.json").read_text())
    decisions={e["id"]:e for e in feedback["entries"]}
    for track in original["tracks"]:
        assert decisions[track["id"]]["sha256"]==track["mp3Sha256"]
        assert hashlib.sha256((OLD/track["mp3"]).read_bytes()).hexdigest()==track["mp3Sha256"]
    assert {k for k,e in decisions.items() if e["decision"]=="changes"}=={"Afterglow","LanternWalk"}
    SOURCE.mkdir(parents=True,exist_ok=True)
    revised={}
    for identity in ("Afterglow","LanternWalk"):
        spec=json.loads((ROOT/f"AudioSource/Music/CitySoundtrack/Soundtrack1/{identity}.json").read_text())
        spec["synthSource"]="RevisionSynths.scd"
        spec["synthDependencies"]=["../../../Tools/CitySoundtrackScore.py","../Soundtrack1/AlbumSynths.scd"]
        spec["feedbackSource"]=receipt.relative_to(ROOT).as_posix()
        spec["feedbackNotes"]=decisions[identity]["notes"]
        spec["seed"]+=1000
        if identity=="Afterglow":
            spec["bpm"],spec["bars"]=112,76
            profile=spec["albumProfile"]
            profile.update(leadSynth="ForwardPiano",leadGain=.105,kickGain=.75,kickBeats=[0,1.5,2,3.5],
                           bassPattern=[[0,.8,0],[2,.7,0],[3.5,.3,7]],padGain=.019)
            # All attacks use the eighth-note grid; primary phrases start with bass/kick.
            pitches=[[71,67,64,62,64,67],[69,67,64,60,62,64],[71,74,72,67,64,67],[69,67,66,62,64,66]]
            spec["melodies"]["main"]=[[[offset,pitch,length] for offset,pitch,length in zip([0,1,2,4,5,6],line,[.75,.5,1,.75,.5,1])] for line in pitches]
            spec["melodies"]["contrast"]=[[[round(n[0]*2)/2,n[1],min(n[2],.75)] for n in phrase] for phrase in spec["melodies"]["contrast"]]
            spec["melodies"]["response"]=[[[round(n[0]*2)/2,n[1],min(n[2],.75)] for n in phrase] for phrase in spec["melodies"]["response"]]
            layouts=[("Immediate piano",4,"main","main","sparse",None,False),
                     ("Driving theme",16,"main","main","full",None,False),
                     ("Glass dialogue",16,"contrast","contrast","full","AlbumGlass",False),
                     ("Four-bar breath",4,"main","main","break",None,False),
                     ("Developed piano",24,"response","main","full",None,True),
                     ("Evening descent",12,"main","contrast","outro",None,False)]
            spec["intent"]="Clear piano attacks locked to the beat, shorter replies and a more energetic112BPM groove; one short break and extended developing piano section."
        else:
            spec["bpm"],spec["bars"]=104,72
            spec["albumProfile"]["synthGains"]={"AlbumPluck":11.2}
            spec["albumProfile"].update(kickGain=.8,padGain=.018)
            # Keep the liked mallet/harmony material; bring the response forward explicitly.
            layouts=[("Wooden opening",8,"main","main","sparse",None,False),
                     ("Plucked first answer",8,"response","main","full","AlbumPluck",False),
                     ("Mallet theme",16,"main","main","full",None,False),
                     ("Short turnaround",4,"main","contrast","break",None,False),
                     ("Pluck and mallet replies",16,"contrast","contrast","full","AlbumPluck",True),
                     ("Mallet lift",8,"response","main","full",None,True),
                     ("Lanterns recede",12,"main","main","outro",None,False)]
            spec["intent"]="Liked wooden mallets and major harmony retained, withpluck source calibrated11.2x from its very quiet baseline, clearer presence and a104BPM walking groove; pluck-first answers and a shorter turnaround."
        spec["sections"]=[]
        start=0
        for name,length,melody,progression,density,synth,lift in layouts:
            spec["sections"].append(section(name,length,start,melody,progression,density,synth,lift))
            start+=length
        assert start==spec["bars"]
        target=SOURCE/(identity+".json")
        target.write_text(json.dumps(spec,indent=2))
        revised[identity]=spec
    tracks=[]
    for old in original["tracks"]:
        track={k:old[k] for k in ("id","title","bpm","bars","source","intent","method")}
        if old["id"] in revised:
            spec=revised[old["id"]]
            track.update(bpm=spec["bpm"],bars=spec["bars"],source=(SOURCE/(old["id"]+".json")).relative_to(ROOT).as_posix(),intent=spec["intent"],decision="Listening pending on revised recording")
        else:
            assert decisions[old["id"]]["decision"]=="approve"
            track.update(reuse=str(Path(old["renderManifest"]).parent).replace("\\","/"),decision="User-approved exact recording",approvedMp3Sha256=old["mp3Sha256"])
        tracks.append(track)
    album={"schema":1,"task":"D82","date":"2026-10-13","title":"City Sim VR — Soundtrack revision2",
           "status":"Seven exact approvals preserved; two revised candidates for listening",
           "tracks":tracks,"reviewSubset":["Afterglow","LanternWalk"],"reviewTitle":"City Sim VR — Two revised tracks",
           "feedbackSource":receipt.relative_to(ROOT).as_posix(),"overallFeedback":feedback["overall"],
           "reviewLimits":"Assistant numeric/score review; user listening determines revised quality."}
    (SOURCE/"Album.json").write_text(json.dumps(album,indent=2))
    print(json.dumps({"revised":list(revised),"approvedUnchanged":7,"tempos":[112,104],"feedbackSource":album["feedbackSource"]}))


if __name__=="__main__":main()
