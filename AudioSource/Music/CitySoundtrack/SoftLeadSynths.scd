// Original Night Transit lead revision. Preserve the earlier audition library.
(
(PathName(thisProcess.nowExecutingPath).pathOnly +/+ "VarietySynths.scd").load;
~d82Defs = ~d82Defs.reject { |definition| definition.name == \D82Ribbon };
~d82Defs = ~d82Defs ++ [
    SynthDef(\D82Ribbon, { |out=0, freq=440, hold=0.2, amp=0.09, pan=0|
        var env, tone, stereo, frequency;
        env = EnvGen.kr(Env.linen(0.04,hold,0.42,1,-4), doneAction:2);
        frequency = freq*(1+SinOsc.kr(4.5,0,0.001));
        tone = SinOsc.ar(frequency)*0.82 + LFTri.ar(frequency*1.001)*0.18;
        tone = LPF.ar(tone,1400);
        // -6dB relative lead gain before whole-track mastering; quieter than the previous pulse voice.
        stereo = Pan2.ar(tone*env*amp*0.5,pan);
        Out.ar(out,FreeVerb2.ar(stereo[0],stereo[1],0.16,0.66,0.65));
    })
];
)
