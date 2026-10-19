"""Write the authored nine-track D82 first-review set; existing auditions stay immutable."""
from pathlib import Path
import json

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/"AudioSource/Music/CitySoundtrack/Soundtrack1"
plans=[
    dict(id="Afterglow",title="Afterglow",bpm=82,bars=72,seed=82801,lead="AlbumPiano",answer="AlbumGlass",leadGain=.09,tone=2400,
         chords=[[45,52,55,59,64],[41,48,52,55,60],[48,55,59,62,67],[43,50,54,57,62]],
         motif=[71,67,64,62,64,67,69,64],entry=[.5,1.65,3,5.15],kick=[0,1.75,2.5],back="HybridSnare",backGain=.16,hat="Shaker",hatGain=.07,bass="Bass",bassGain=.12,bassPattern=[[0,1.2,0],[2.65,.7,0]],padGain=.022,color=.1,
         intent="Preferred evening piano/harmony, expanded with gentle glass responses and a firmer laid-back beat."),
    dict(id="GlassCanal",title="Glass Canal",bpm=94,bars=64,seed=82802,lead="AlbumGlass",answer="AlbumPiano",leadGain=.095,tone=2200,
         chords=[[48,55,58,62,67],[44,51,55,58,63],[51,58,62,65,70],[46,53,57,60,65]],
         motif=[74,70,67,65,67,70,72,67],entry=[0,2.65,4,6.65],kick=[0,1.5,2.75],back="Clap",backGain=.24,hat="Hat",hatGain=.038,bass="AnalogBass",bassGain=.11,bassPattern=[[0,.8,0],[2.65,.6,0],[3.5,.3,7]],padGain=.024,color=.25,
         intent="Aligned glass-key motif with brisk syncopated percussion, alternating longer bells and short piano answers."),
    dict(id="LanternWalk",title="Lantern Walk",bpm=88,bars=64,seed=82804,lead="AlbumMarimba",answer="AlbumPluck",leadGain=.12,tone=2100,
         chords=[[55,62,66,69,74],[52,59,62,66,71],[57,64,67,71,76],[50,57,59,64,69]],
         motif=[74,71,69,66,67,69,71,74],entry=[0,1.5,2.5,4,5.5,6.75],kick=[0,2,2.75],back="Stick",backGain=.2,hat="Shaker",hatGain=.085,bass="Bass",bassGain=.13,bassPattern=[[0,.6,0],[1.5,.35,7],[2.5,.4,0],[3.25,.3,12]],padGain=.021,color=0,
         intent="Bright wooden mallets and muted plucked responses, moving bass and cross-stick/shaker groove."),
    dict(id="NightTransit",title="Night Transit",bpm=90,bars=72,seed=82805,lead="AlbumSine",answer="AlbumArp",leadGain=.12,tone=1400,
         chords=[[47,54,57,62,66],[52,59,62,66,69],[43,50,54,59,62],[42,49,52,58,61]],
         motif=[71,74,78,76,74,71,69,71],entry=[0,.75,1.5,2.75,4,6],kick=[0,1.5,2.75],back="Clap",backGain=.23,hat="Hat",hatGain=.035,bass="AnalogBass",bassGain=.115,bassPattern=[[0,.75,0],[1.5,.4,0],[2.75,.5,7]],padGain=.022,color=.6,
         intent="Softened sine lead, darker call-and-response harmony, restrained digital arps and a steady electronic beat."),
    dict(id="RooftopRain",title="Rooftop Rain",bpm=84,bars=72,seed=82806,lead="AlbumRhodes",answer="AlbumOrgan",leadGain=.105,tone=1800,
         chords=[[48,55,58,63,67],[53,60,63,67,72],[46,53,57,60,65],[43,50,58,62,65]],
         motif=[63,67,70,74,72,70,67,65],entry=[0,.75,2,3.5,4.75,6.5],kick=[0,1.75,2.5],back="HybridSnare",backGain=.15,hat="Shaker",hatGain=.06,bass="Bass",bassGain=.12,bassPattern=[[0,1,0],[2.5,.9,0],[3.5,.25,7]],padGain=.022,color=.1,
         intent="Tremolo electric keys, softened organ responses and a spacious broken-beat rooftop mood."),
    dict(id="PaperLanterns",title="Paper Lanterns",bpm=92,bars=56,seed=82807,lead="AlbumPluck",answer="AlbumMarimba",leadGain=.25,tone=1700,
         chords=[[50,57,61,64,69],[55,62,66,69,73],[47,54,57,62,66],[57,64,67,71,74]],
         motif=[74,78,81,76,78,74,73,69],entry=[0,.5,1.5,3,4,5.5],kick=[0,1.5,2,3.25],back="Stick",backGain=.22,hat="Shaker",hatGain=.08,bass="Bass",bassGain=.13,bassPattern=[[0,.55,0],[1.5,.35,7],[2.75,.6,0],[3.5,.25,12]],padGain=.021,color=.15,
         intent="Picked-string phrases, clear rests, major-key mallet answers and a lightly bouncing beat."),
    dict(id="TidalCircuit",title="Tidal Circuit",bpm=96,bars=64,seed=82808,lead="AlbumArp",answer="AlbumGlass",leadGain=.10,tone=1500,
         chords=[[54,61,64,68,73],[50,57,61,64,69],[57,64,68,71,76],[52,59,63,66,71]],
         motif=[73,76,80,78,76,73,71,68],entry=[0,.5,1.5,2.5,4,5,6.5,7],kick=[0,1,2,3],back="Clap",backGain=.22,hat="Hat",hatGain=.038,bass="AnalogBass",bassGain=.11,bassPattern=[[0,.45,0],[1,.4,0],[2,.45,0],[3.5,.25,7]],padGain=.02,color=.9,
         intent="Rounded short arpeggios, measured four-on-the-floor pulse and changing digital call-and-response sections."),
    dict(id="BlueHour",title="Blue Hour",bpm=80,bars=64,seed=82809,lead="AlbumFlute",answer="AlbumOrgan",leadGain=.105,tone=1500,
         chords=[[46,53,57,60,65],[43,50,53,57,62],[51,58,62,65,69],[53,60,63,67,70]],
         motif=[70,74,77,72,69,70,67,65],entry=[0,1.5,3,4.5,6],kick=[0,2,3.25],back="HybridSnare",backGain=.135,hat="Shaker",hatGain=.06,bass="Bass",bassGain=.115,bassPattern=[[0,1.25,0],[2.5,1,0]],padGain=.022,color=0,
         intent="Breathy sine/flute lead, warm organ answers and an unhurried but clearly defined evening pulse.")
]

def bounded(pitch):
    while pitch>83:
        pitch-=12
    while pitch<62:
        pitch+=12
    return pitch


def prepare(plan):
    main=[]
    for phrase in range(4):
        entry=plan["entry"]
        main.append([[offset,plan["motif"][(phrase*2+i+2)%len(plan["motif"])],1.5 if i%2==0 else 1] for i,offset in enumerate(entry)])
    contrast=[]
    response=[]
    contrast_chords=[plan["chords"][i] for i in (2,0,3,1)]
    for i,chord in enumerate(contrast_chords):
        pitches=[bounded(chord[index]+12) for index in (2,1,3,0,4,2,1,0)]
        offsets=[0,.75,1.5,2.75,3.5,4.5,6,7.25]
        lengths=[.5,.5,.75,.25,.5,1,.5,.25]
        contrast.append([[offset,pitch,length] for offset,pitch,length in zip(offsets,pitches,lengths)])
    for chord in plan["chords"]:
        pitches=[bounded(chord[index]+12) for index in (0,1,2,4,3,2,1,0)]
        offsets=[0,1,1.5,2.75,4,5.25,5.5,7]
        lengths=[1,.5,.5,.25,1,.25,.5,.75]
        response.append([[offset,pitch,length] for offset,pitch,length in zip(offsets,pitches,lengths)])
    bars=plan["bars"]
    # Different section lengths avoid nine clones of the same arrangement map.
    lengths=([4,12,12,8,12,12,4] if bars==64 else [4,16,12,8,16,12,4] if bars==72 else [4,12,8,8,12,8,4])
    sections=[]
    start=0
    names=["Opening","Main theme","Contrasting response","Open space","Theme variation","Final lift","Outro"]
    for i,length in enumerate(lengths):
        density="sparse" if i==0 else "break" if i==3 else "outro" if i==6 else "full"
        section={"name":names[i],"startBar":start,"bars":length,"progression":"contrast" if i in (2,5) else "main","melody":"contrast" if i in (2,5) else "response" if i==4 else "main","density":density,
                 "drums":.7 if i in (0,3,6) else 1,"bass":.8 if i in (0,3,6) else 1,"texture":.8 if i in (2,4,5) else 1,"lead":.65 if i in (0,3,6) else 1,"tone":plan["tone"]*.7 if i in (0,3,6) else plan["tone"]}
        if i==2:
            section["leadSynth"]=plan["answer"]
        if i==5:
            section["lift"]=True
        sections.append(section)
        start+=length
    assert start==bars
    profile={"leadSynth":plan["lead"],"answerSynth":plan["answer"],"leadGain":plan["leadGain"],"tone":plan["tone"],"kickBeats":plan["kick"],"snareBeats":[1,3],"backbeatSynth":plan["back"],"backbeatGain":plan["backGain"],"hatSynth":plan["hat"],"hatGain":plan["hatGain"],"hatBeats":[i*.5+(.035 if i%2 else 0) for i in range(8)],"bassSynth":plan["bass"],"bassGain":plan["bassGain"],"bassPattern":plan["bassPattern"],"padGain":plan["padGain"],"padColor":plan["color"]}
    return {"schema":1,"id":plan["id"],"title":plan["title"],"bpm":plan["bpm"],"bars":bars,"seed":plan["seed"],"synthSource":"AlbumSynths.scd","synthDependencies":["../../../Tools/CitySoundtrackScore.py"],"sample":{"mode":"synthBed"},"targetLUFS":-24,"truePeakCeilingDBTP":-6,"progressions":{"main":plan["chords"],"contrast":contrast_chords},"melodies":{"main":main,"contrast":contrast,"response":response},"sections":sections,"albumProfile":profile,"intent":plan["intent"]}


def main():
    targets=[SOURCE/(plan["id"]+".json") for plan in plans]+[SOURCE/"Album.json"]
    if any(path.exists() for path in targets):
        raise RuntimeError("Album source already exists; edit it deliberately, do not overwrite")
    SOURCE.mkdir(parents=True,exist_ok=True)
    entries=[]
    for index,plan in enumerate(plans):
        path=SOURCE/(plan["id"]+".json")
        spec=prepare(plan)
        path.write_text(json.dumps(spec,indent=2),encoding="utf-8")
        entries.append({"id":plan["id"],"title":plan["title"],"bpm":plan["bpm"],"bars":plan["bars"],"source":str(path.relative_to(ROOT)).replace("\\","/"),"seconds":plan["bars"]*4*60/plan["bpm"],"intent":plan["intent"],"method":"Original synthesis; no recorded/generated samples"})
    entries.insert(2,{"id":"NeonCourtyard","title":"Neon Courtyard","bpm":90,"bars":64,"seconds":170.666666667,"reuse":"Artifacts/D82/Audition7","source":"AudioSource/Music/CitySoundtrack/Hybrid7/Composition.json","intent":"Reviewed hybrid direction with articulated responses, firm beat and quiet generated texture.","method":"Original authored synthesis plus verified ACE-Step texture"})
    manifest={"schema":1,"task":"D82","date":"2026-10-12","title":"City Sim VR — Initial Soundtrack","status":"Nine-track first review; no native game adoption","tracks":entries,"directions":["Dreamy ambient/electronic beats","Firm groove, soft leads, distinct melodies/instruments","Long/short phrases and arrangement contrast","EQ/A2A experiments deferred"],"reviewLimits":"Assistant has not perceptually heard the tracks; user listening/selection pending."}
    (SOURCE/"Album.json").write_text(json.dumps(manifest,indent=2),encoding="utf-8")
    print(json.dumps({"tracks":len(entries),"minutes":sum(e["seconds"] for e in entries)/60},indent=2))


if __name__=="__main__":
    main()
