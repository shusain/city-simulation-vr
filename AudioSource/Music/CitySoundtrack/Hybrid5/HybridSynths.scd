// D82 hybrid audition: generated texture buffer plus original authored instruments.
(
(PathName(thisProcess.nowExecutingPath).pathOnly +/+ "../VarietySynths.scd").load;
~d82Defs = ~d82Defs ++ [
    SynthDef(\D82HybridTexture, { |out=4, buf=0, rate=1, hold=2, amp=0.4|
        var tone, env;
        env = EnvGen.kr(Env.linen(0.25, hold, 0.4, 1, -4), doneAction:2);
        tone = PlayBuf.ar(2, buf, BufRateScale.kr(buf)*rate, loop:0);
        Out.ar(out, tone * env * amp);
    }),
    SynthDef(\D82HybridKick, { |out=0, amp=0.38, seed=1|
        var body, click, env;
        RandSeed.ir(1, seed);
        env = EnvGen.kr(Env.perc(0.001,0.38,1,-6), doneAction:2);
        body = SinOsc.ar(EnvGen.kr(Env([125,52],[0.045],-6)));
        click = LPF.ar(WhiteNoise.ar,2800) * EnvGen.kr(Env.perc(0.001,0.012)) * 0.16;
        Out.ar(out,Pan2.ar((body*env+click)*amp));
    }),
    SynthDef(\D82HybridSnare, { |out=0, amp=0.18, seed=1|
        var tone, env;
        RandSeed.ir(1, seed);
        env = EnvGen.kr(Env.perc(0.002,0.18,1,-5), doneAction:2);
        tone = BPF.ar(WhiteNoise.ar,1900,0.85)*0.65
            + SinOsc.ar(185)*EnvGen.kr(Env.perc(0.001,0.085))*0.4;
        Out.ar(out,Pan2.ar(tone*env*amp,-0.06));
    }),
    SynthDef(\D82HybridBell, { |out=2, freq=440, amp=0.075, pan=0|
        var tone, env, stereo;
        env = EnvGen.kr(Env.perc(0.012,1.0,1,-5), doneAction:2);
        tone = SinOsc.ar(freq)*0.85 + LFTri.ar(freq*2)*0.15;
        stereo = Pan2.ar(LPF.ar(tone,1800)*env*amp,pan);
        Out.ar(out,FreeVerb2.ar(stereo[0],stereo[1],0.15,0.6,0.7));
    })
];
)
