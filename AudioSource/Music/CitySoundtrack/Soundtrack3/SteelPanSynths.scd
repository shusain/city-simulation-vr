// Original steel-pan-style synthesis blended with the existing wooden mallet.
(
(PathName(thisProcess.nowExecutingPath).pathOnly +/+ "../Soundtrack1/AlbumSynths.scd").load;
~d82Defs = ~d82Defs ++ [
    SynthDef(\D82SteelPan, { |out=6, freq=440, amp=0.12, decay=0.8, pan=0, toneHz=2400|
        var env, strike, mod, fundamental, upper, tone, stereo;
        env=EnvGen.kr(Env.perc(0.004,decay,1,-4),doneAction:2);
        strike=EnvGen.kr(Env.perc(0.001,0.11,1,-5));
        mod=SinOsc.ar(freq*2,0,freq*0.28*strike);
        fundamental=SinOsc.ar(freq+mod)*0.72;
        upper=(SinOsc.ar(freq*2.006)*0.2)+(SinOsc.ar(freq*3.012)*0.08);
        tone=fundamental+(upper*(0.35+(strike*0.65)))
            +(SinOsc.ar(freq*4.07)*strike*0.07);
        stereo=Pan2.ar(LPF.ar(tone,toneHz.min(2600))*env*amp,pan);
        Out.ar(out,FreeVerb2.ar(stereo[0],stereo[1],0.16,0.58,0.76));
    })
];
)
