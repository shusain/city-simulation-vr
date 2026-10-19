// D82 initial soundtrack: original instruments, fixed presentation buses, no recordings.
(
(PathName(thisProcess.nowExecutingPath).pathOnly +/+ "../Hybrid5/HybridSynths.scd").load;
~d82Defs = ~d82Defs ++ [
    SynthDef(\D82AlbumPiano, { |out=6, freq=440, amp=0.1, decay=1.4, pan=0, toneHz=2500|
        var env, mod, tone, stereo;
        env=EnvGen.kr(Env.perc(0.009,decay,1,-5),doneAction:2);
        mod=SinOsc.ar(freq*2,0,freq*0.65*EnvGen.kr(Env.perc(0.001,0.5)));
        tone=(SinOsc.ar(freq+mod)*0.78)+SinOsc.ar(freq*1.002,0,0.22);
        stereo=Pan2.ar(LPF.ar(tone,toneHz)*env*amp,pan);
        stereo=stereo+(LPF.ar(DelayC.ar(stereo.reverse,0.5,[0.31,0.43]),1600)*0.12);
        Out.ar(out,FreeVerb2.ar(stereo[0],stereo[1],0.2,0.72,0.7));
    }),
    SynthDef(\D82AlbumGlass, { |out=6, freq=440, amp=0.11, decay=1, pan=0, toneHz=2300|
        var env, tone, stereo;
        env=EnvGen.kr(Env.perc(0.008,decay,1,-5),doneAction:2);
        tone=(SinOsc.ar(freq)*0.78)+(SinOsc.ar(freq*2.01)*0.16)+(SinOsc.ar(freq*3.98)*0.06);
        stereo=Pan2.ar(LPF.ar(tone,toneHz)*env*amp,pan);
        stereo=stereo+(LPF.ar(DelayC.ar(stereo.reverse,0.5,[0.26,0.39]),1700)*0.12);
        Out.ar(out,FreeVerb2.ar(stereo[0],stereo[1],0.22,0.7,0.68));
    }),
    SynthDef(\D82AlbumMarimba, { |out=6, freq=440, amp=0.14, decay=0.8, pan=0, toneHz=2100|
        var env, tone, stereo;
        env=EnvGen.kr(Env.perc(0.003,decay,1,-5),doneAction:2);
        tone=SinOsc.ar(freq)+(SinOsc.ar(freq*4.03)*EnvGen.kr(Env.perc(0.001,0.1))*0.2);
        stereo=Pan2.ar(LPF.ar(tone,toneHz)*env*amp,pan);
        Out.ar(out,FreeVerb2.ar(stereo[0],stereo[1],0.15,0.55,0.8));
    }),
    SynthDef(\D82AlbumPluck, { |out=6, freq=440, amp=0.16, decay=0.9, pan=0, toneHz=1800, seed=1|
        var env, tone, stereo;
        RandSeed.ir(1,seed);
        env=EnvGen.kr(Env.perc(0.006,decay,1,-4),doneAction:2);
        tone=Pluck.ar(PinkNoise.ar(0.4),Impulse.kr(0),0.05,freq.reciprocal,1.7,0.6);
        stereo=Pan2.ar(LPF.ar(LeakDC.ar(tone),toneHz)*env*amp*2,pan);
        Out.ar(out,FreeVerb2.ar(stereo[0],stereo[1],0.18,0.6,0.76));
    }),
    SynthDef(\D82AlbumSine, { |out=6, freq=440, amp=0.12, decay=1, pan=0, toneHz=1400|
        var env, tone, stereo, pitch;
        env=EnvGen.kr(Env.perc(0.03,decay,1,-4),doneAction:2);
        pitch=freq*(1+SinOsc.kr(4.5,0,0.001));
        tone=(SinOsc.ar(pitch)*0.85)+(LFTri.ar(pitch*1.001)*0.15);
        stereo=Pan2.ar(LPF.ar(tone,toneHz)*env*amp*0.65,pan);
        Out.ar(out,FreeVerb2.ar(stereo[0],stereo[1],0.2,0.66,0.7));
    }),
    SynthDef(\D82AlbumRhodes, { |out=6, freq=440, amp=0.13, decay=1.1, pan=0, toneHz=1900|
        var env, tone, stereo, mod;
        env=EnvGen.kr(Env.perc(0.014,decay,1,-4),doneAction:2);
        mod=SinOsc.ar(freq*1.5,0,freq*0.22*EnvGen.kr(Env.perc(0.003,0.6)));
        tone=(SinOsc.ar(freq+mod)*0.8)+(SinOsc.ar(freq*2)*0.2);
        stereo=Pan2.ar(LPF.ar(tone,toneHz)*env*amp*(0.92+SinOsc.kr(3.2,0,0.08)),pan);
        Out.ar(out,FreeVerb2.ar(stereo[0],stereo[1],0.24,0.7,0.72));
    }),
    SynthDef(\D82AlbumArp, { |out=6, freq=440, amp=0.11, decay=0.5, pan=0, toneHz=1500|
        var env, tone, stereo;
        env=EnvGen.kr(Env.perc(0.012,decay,1,-5),doneAction:2);
        tone=(LFTri.ar(freq)*0.4)+(SinOsc.ar(freq)*0.6);
        stereo=Pan2.ar(LPF.ar(tone,toneHz)*env*amp,pan);
        stereo=stereo+(LPF.ar(DelayC.ar(stereo.reverse,0.5,[0.22,0.33]),1000)*0.12);
        Out.ar(out,FreeVerb2.ar(stereo[0],stereo[1],0.16,0.6,0.74));
    }),
    SynthDef(\D82AlbumFlute, { |out=6, freq=440, amp=0.12, decay=1.2, pan=0, toneHz=1600, seed=1|
        var env, tone, stereo;
        RandSeed.ir(1,seed);
        env=EnvGen.kr(Env.perc(0.055,decay,1,-4),doneAction:2);
        tone=(SinOsc.ar(freq*(1+SinOsc.kr(4.1,0,0.001)))*0.8)
            +(SinOsc.ar(freq*2)*0.12)+(BPF.ar(PinkNoise.ar,freq,0.3)*0.08);
        stereo=Pan2.ar(LPF.ar(tone,toneHz)*env*amp,pan);
        Out.ar(out,FreeVerb2.ar(stereo[0],stereo[1],0.25,0.76,0.72));
    }),
    SynthDef(\D82AlbumOrgan, { |out=6, freq=440, amp=0.095, decay=1.3, pan=0, toneHz=1700|
        var env, tone, stereo;
        env=EnvGen.kr(Env.linen(0.08,decay*0.5,decay*0.5,1,-4),doneAction:2);
        tone=(SinOsc.ar(freq)*0.68)+(SinOsc.ar(freq*2)*0.22)+(SinOsc.ar(freq*3)*0.1);
        stereo=Pan2.ar(LPF.ar(tone,toneHz)*env*amp,pan);
        stereo=stereo+(DelayC.ar(stereo,0.04,0.018+SinOsc.kr([0.27,0.33],0,0.002))*0.12);
        Out.ar(out,FreeVerb2.ar(stereo[0],stereo[1],0.23,0.75,0.76));
    }),
    SynthDef(\D82AlbumPad, { |out=4, freq=220, hold=4, amp=0.03, pan=0, color=0|
        var env, tone, stereo;
        env=EnvGen.kr(Env.linen(0.6,hold,1.3,1,-4),doneAction:2);
        tone=Mix(SinOsc.ar(freq*[0.998,1.002,2.001],[0,0.7,1.2],[0.4,0.4,0.2]));
        tone=LPF.ar(tone+(LFTri.ar(freq)*color*0.12),900+(color*250));
        stereo=Pan2.ar(tone*env*amp,pan+SinOsc.kr(0.06,0,0.04));
        Out.ar(out,FreeVerb2.ar(stereo[0],stereo[1],0.25,0.75,0.75));
    })
];
)
