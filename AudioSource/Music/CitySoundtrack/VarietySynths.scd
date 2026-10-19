// Original additional D82 instruments; oscillator/noise/physical-model synthesis only.
// Keep the first audition's instrument definitions intact for source provenance.
(
(PathName(thisProcess.nowExecutingPath).pathOnly +/+ "DreamSynths.scd").load;
~d82Defs = ~d82Defs ++ [
    SynthDef(\D82Pluck, { |out=0, freq=330, amp=0.2, pan=0, seed=1|
        var env, tone, stereo;
        RandSeed.ir(1, seed);
        env = EnvGen.kr(Env.linen(0.003, 1.8, 0.8), doneAction:2);
        tone = Pluck.ar(PinkNoise.ar(0.5), Impulse.kr(0), 0.05, freq.reciprocal, 2.0, 0.5);
        tone = LPF.ar(LeakDC.ar(tone), 3800);
        stereo = Pan2.ar(tone * env * amp, pan);
        Out.ar(out, FreeVerb2.ar(stereo[0], stereo[1], 0.16, 0.56, 0.58));
    }),
    SynthDef(\D82Marimba, { |out=0, freq=440, amp=0.13, pan=0|
        var env, tone, stereo;
        env = EnvGen.kr(Env.perc(0.002, 1.4, 1, -5), doneAction:2);
        tone = SinOsc.ar(freq) * env + SinOsc.ar(freq*4.03) * EnvGen.kr(Env.perc(0.001,0.14)) * 0.22
            + SinOsc.ar(freq*7.01) * EnvGen.kr(Env.perc(0.001,0.055)) * 0.055;
        stereo = Pan2.ar(tone * amp, pan);
        Out.ar(out, FreeVerb2.ar(stereo[0], stereo[1], 0.2, 0.64, 0.7));
    }),
    SynthDef(\D82WarmChord, { |out=0, freq=220, hold=3, amp=0.04, pan=0|
        var env, tone, stereo;
        env = EnvGen.kr(Env.linen(0.4,hold,1.0,1,-4), doneAction:2);
        tone = LPF.ar(Mix(LFTri.ar(freq*[0.997,1.003],0,0.5)),1000);
        stereo = Pan2.ar(tone * env * amp, pan + SinOsc.kr(0.09,0,0.08));
        Out.ar(out, FreeVerb2.ar(stereo[0],stereo[1],0.3,0.7,0.62));
    }),
    SynthDef(\D82Ribbon, { |out=0, freq=440, hold=0.2, amp=0.09, pan=0|
        var env, tone, stereo;
        env = EnvGen.kr(Env.linen(0.025,hold,0.32,1,-4), doneAction:2);
        tone = Mix(Pulse.ar(freq*[0.998,1.002]*(1+SinOsc.kr(4.5,0,0.0015)),[0.38,0.52],0.5));
        tone = RLPF.ar(tone,1400 + env*900,0.65);
        stereo = Pan2.ar(tone * env * amp,pan);
        Out.ar(out, FreeVerb2.ar(stereo[0],stereo[1],0.18,0.66,0.6));
    }),
    SynthDef(\D82AnalogBass, { |out=0, freq=55, hold=0.3, amp=0.12|
        var env, tone;
        env = EnvGen.kr(Env.linen(0.012,hold,0.16,1,-4), doneAction:2);
        tone = SinOsc.ar(freq)*0.75 + LPF.ar(Pulse.ar(freq,0.35),280)*0.25;
        Out.ar(out,Pan2.ar(tone*env*amp));
    }),
    SynthDef(\D82Stick, { |out=0, amp=0.1, seed=1|
        var env, tone;
        RandSeed.ir(1,seed);
        env = EnvGen.kr(Env.perc(0.001,0.075,1,-6), doneAction:2);
        tone = SinOsc.ar(1150)*0.45 + BPF.ar(WhiteNoise.ar,2100,0.7)*0.55;
        Out.ar(out,Pan2.ar(tone*env*amp,-0.14));
    }),
    SynthDef(\D82Shaker, { |out=0, amp=0.04, pan=0.2, seed=1|
        var env, tone;
        RandSeed.ir(1,seed);
        env = EnvGen.kr(Env.perc(0.009,0.09,1,-5), doneAction:2);
        tone = HPF.ar(LPF.ar(PinkNoise.ar,7500),3200);
        Out.ar(out,Pan2.ar(tone*env*amp,pan));
    }),
    SynthDef(\D82Clap, { |out=0, amp=0.1, seed=1|
        var env, tone;
        RandSeed.ir(1,seed);
        env = EnvGen.kr(Env.perc(0.002,0.18,1,-5), doneAction:2);
        tone = BPF.ar(WhiteNoise.ar,1800,0.8);
        tone = tone*(1+SinOsc.kr(85,0,0.25)*EnvGen.kr(Env.perc(0.001,0.035)));
        Out.ar(out,Pan2.ar(tone*env*amp,0.12));
    })
];
)
