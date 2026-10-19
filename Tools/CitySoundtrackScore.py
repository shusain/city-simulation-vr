"""Authored D82 soundtrack patterns; fixed simulation-independent offline music score."""
import math


def events(spec):
    result=[]
    beat=60/spec["bpm"]
    profile=spec["albumProfile"]
    def hz(midi):
        return 440*2**((midi-69)/12)
    def add(bar,offset,synth,**args):
        if bar*4+offset<spec["bars"]*4:
            result.append({"beat":bar*4+offset,"time":0.1+(bar*4+offset)*beat,"synth":synth,"args":args})
    for section in spec["sections"]:
        for local in range(section["bars"]):
            bar=section["startBar"]+local
            progression=spec["progressions"][section["progression"]]
            chord=progression[(local//2)%len(progression)]
            sparse=section["density"] in ("sparse","break","outro")
            rebuilding=section["density"]=="break" and local>=section["bars"]-2
            kick=section.get("kickBeats",profile["kickBeats"]) if not sparse or rebuilding else [0,2]
            for i,offset in enumerate(kick):
                add(bar,offset,"HybridKick",out=0,amp=(0.33 if offset in (0,2) else 0.25)*section["drums"]*profile.get("kickGain",1),seed=spec["seed"]+bar*100+i)
            for i,offset in enumerate(profile["snareBeats"]):
                add(bar,offset,profile["backbeatSynth"],out=0,amp=profile["backbeatGain"]*section["drums"]*(0.75 if sparse else 1),seed=spec["seed"]+bar*100+10+i)
            hats=profile["hatBeats"][::2] if sparse and not rebuilding else profile["hatBeats"]
            for i,offset in enumerate(hats):
                add(bar,offset,profile["hatSynth"],out=0,amp=profile["hatGain"]*(0.85 if i%2 else 1)*section["drums"],pan=0.18 if i%2 else -0.2,seed=spec["seed"]+bar*100+20+i)
            if local%4==3 and section["density"]=="full":
                add(bar,3.75,profile["backbeatSynth"],out=0,amp=profile["backbeatGain"]*0.3,seed=spec["seed"]+bar*100+40)
            for i,(offset,length,interval) in enumerate(profile["bassPattern"]):
                if sparse and i%2:
                    continue
                if section.get("lift") and local%4==3 and i==len(profile["bassPattern"])-1:
                    interval=12
                root=chord[0]
                while root>40:
                    root-=12
                add(bar,offset,profile["bassSynth"],out=2,freq=hz(root+interval),hold=length*beat,amp=profile["bassGain"]*section["bass"])
            if local%2==0:
                for i,pitch in enumerate(chord):
                    # Distinct source harmonies/voicing; no shared generated buffer for these tracks.
                    pad_pitch=pitch
                    while pad_pitch<48:
                        pad_pitch+=12
                    add(bar,0,"AlbumPad",out=4,freq=hz(pad_pitch),hold=8*beat-0.65,amp=profile["padGain"]*section["texture"],pan=(i-2)*0.24,color=profile["padColor"])
                bank=spec["melodies"][section["melody"]]
                phrase=bank[(local//2)%len(bank)]
                if sparse:
                    phrase=phrase[::2]
                for i,note in enumerate(phrase):
                    offset,pitch,length=note
                    lead=section.get("leadSynth",profile["leadSynth"])
                    add(bar,offset,lead,out=6,freq=hz(pitch),decay=length*beat,amp=profile["leadGain"]*section["lead"]*(0.85 if i%2 else 1)*profile.get("synthGains",{}).get(lead,1),pan=-0.2 if i%2==0 else 0.2,toneHz=section["tone"],**({"seed":spec["seed"]+bar*100+60+i} if lead in ("AlbumPluck","AlbumFlute") else {}))
                    if section.get("layerSynth") and i%section.get("layerEvery",1)==0:
                        add(bar,offset,section["layerSynth"],out=6,freq=hz(pitch),decay=length*beat,
                            amp=profile["leadGain"]*section["lead"]*section["layerGain"],pan=0,
                            toneHz=section["tone"])
                if section.get("lift") and local%4==2:
                    add(bar,7.5,profile["answerSynth"],out=6,freq=hz(chord[2]+12),decay=0.4*beat,amp=profile["leadGain"]*0.35*profile.get("synthGains",{}).get(profile["answerSynth"],1),pan=-0.3,toneHz=profile["tone"],**({"seed":spec["seed"]+bar*100+90} if profile["answerSynth"] in ("AlbumPluck","AlbumFlute") else {}))
    if not all(math.isfinite(event["time"]) for event in result):
        raise ValueError("Nonfinite authored soundtrack time")
    return sorted(result,key=lambda event:event["time"])
