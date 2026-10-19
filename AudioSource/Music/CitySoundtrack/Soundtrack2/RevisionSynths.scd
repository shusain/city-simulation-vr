// Only requested revisions are rendered. Earlier masters remain immutable.
(
(PathName(thisProcess.nowExecutingPath).pathOnly +/+ "../Soundtrack1/AlbumSynths.scd").load;
~d82Defs = ~d82Defs ++ [
    SynthDef(\D82ForwardPiano, { |out=6, freq=440, amp=0.1, decay=1, pan=0, toneHz=2400|
        var env, mod, tone, stereo;
        env=EnvGen.kr(Env.perc(0.005,decay,1,-4),doneAction:2);
        mod=SinOsc.ar(freq*2,0,freq*0.5*EnvGen.kr(Env.perc(0.001,0.4)));
        tone=(SinOsc.ar(freq+mod)*0.82)+(SinOsc.ar(freq*1.001)*0.18);
        stereo=Pan2.ar(LPF.ar(tone,toneHz)*env*amp,pan);
        // Direct attack and restrained room; no displaced stereo echo.
        Out.ar(out,FreeVerb2.ar(stereo[0],stereo[1],0.12,0.62,0.76));
    })
];
)
