// D82 rhythmic refinement: retain tone/FX, control each melodic decay independently.
(
(PathName(thisProcess.nowExecutingPath).pathOnly +/+ "../Hybrid5/HybridSynths.scd").load;
~d82Defs = ~d82Defs ++ [
    SynthDef(\D82FullBell, { |out=6, freq=440, amp=0.065, pan=0, toneHz=1800, decay=1.3|
        var env, dry, stereo, echo;
        env = EnvGen.kr(Env.perc(0.014,decay,1,-5), doneAction:2);
        dry = LPF.ar((SinOsc.ar(freq)*0.85)+(LFTri.ar(freq*2)*0.15),toneHz)*env*amp*0.3;
        stereo = Pan2.ar(dry,pan);
        // Gentle stereo modulation and filtered eighth/dotted-eighth echoes.
        echo = LPF.ar(DelayC.ar(stereo.reverse,0.6,[0.333333,0.5]),1400)*0.12;
        stereo = stereo + (DelayC.ar(stereo,0.04,0.018+SinOsc.kr([0.31,0.37],0,0.003))*0.08) + echo;
        Out.ar(out,FreeVerb2.ar(stereo[0],stereo[1],0.18,0.64,0.72));
    }),
    SynthDef(\D82FullPluck, { |out=6, freq=440, amp=0.065, pan=0, toneHz=1700, seed=1, decay=1.5|
        var env, dry, stereo;
        RandSeed.ir(1,seed);
        env = EnvGen.kr(Env.perc(0.006,decay,1,-4),doneAction:2);
        dry = Pluck.ar(PinkNoise.ar(0.4),Impulse.kr(0),0.04,freq.reciprocal,1.7,0.65);
        stereo = Pan2.ar(LPF.ar(LeakDC.ar(dry),toneHz)*env*amp*2,pan);
        stereo = stereo + (LPF.ar(DelayC.ar(stereo.reverse,0.6,[0.333333,0.5]),1300)*0.14);
        Out.ar(out,FreeVerb2.ar(stereo[0],stereo[1],0.2,0.66,0.74));
    })
];
)
