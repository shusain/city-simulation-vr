// Original D82 instruments: oscillators and seeded noise only; no recorded samples.
(
~d82Defs = [
    SynthDef(\D82Pad, { |out=0, freq=220, hold=4, amp=0.08, pan=0, seed=1|
        var tone, env, stereo;
        RandSeed.ir(1, seed);
        env = EnvGen.kr(Env.linen(1.3, hold, 2.4, 1, -4), doneAction:2);
        tone = Mix(SinOsc.ar(freq * [0.997, 1, 1.003, 2.001], [0, 0.7, 1.8, 2.1], [0.36, 0.3, 0.24, 0.1]));
        tone = tone + LPF.ar(LFTri.ar(freq * 0.5), 750) * 0.08;
        stereo = Pan2.ar(tone * env * amp, pan + SinOsc.kr(0.07, seed % 6, 0.08));
        Out.ar(out, FreeVerb2.ar(stereo[0], stereo[1], 0.34, 0.82, 0.64));
    }),
    SynthDef(\D82Key, { |out=0, freq=440, amp=0.13, pan=0, seed=1|
        var tone, env, mod, stereo;
        RandSeed.ir(1, seed);
        env = EnvGen.kr(Env.perc(0.009, 2.8, 1, -5), doneAction:2);
        mod = SinOsc.ar(freq * 2, 0, freq * 0.65 * EnvGen.kr(Env.perc(0.001, 0.5)));
        tone = SinOsc.ar(freq + mod) * 0.78 + SinOsc.ar(freq * 1.002, 0, 0.22);
        stereo = Pan2.ar(LPF.ar(tone, 4200) * env * amp, pan);
        stereo = stereo + DelayC.ar(stereo.reverse, 0.5, [0.31, 0.43]) * 0.22;
        Out.ar(out, FreeVerb2.ar(stereo[0], stereo[1], 0.25, 0.72, 0.6));
    }),
    SynthDef(\D82Bass, { |out=0, freq=55, hold=0.6, amp=0.14|
        var env, tone;
        env = EnvGen.kr(Env.linen(0.025, hold, 0.22, 1, -4), doneAction:2);
        tone = SinOsc.ar(freq) + SinOsc.ar(freq*2, 0, 0.18);
        Out.ar(out, Pan2.ar(tone * env * amp));
    }),
    SynthDef(\D82Kick, { |out=0, amp=0.2|
        var env, tone;
        env = EnvGen.kr(Env.perc(0.002, 0.32, 1, -6), doneAction:2);
        tone = SinOsc.ar(EnvGen.kr(Env([130, 48], [0.065], -5))) + LPF.ar(WhiteNoise.ar, 700) * 0.06;
        Out.ar(out, Pan2.ar(tone * env * amp));
    }),
    SynthDef(\D82Snare, { |out=0, amp=0.11, seed=1|
        var env, tone;
        RandSeed.ir(1, seed);
        env = EnvGen.kr(Env.perc(0.004, 0.2, 1, -5), doneAction:2);
        tone = BPF.ar(WhiteNoise.ar, 1900, 0.8) * 0.7 + SinOsc.ar(175) * 0.3;
        Out.ar(out, Pan2.ar(tone * env * amp, -0.08));
    }),
    SynthDef(\D82Hat, { |out=0, amp=0.028, pan=0.2, seed=1|
        var env, tone;
        RandSeed.ir(1, seed);
        env = EnvGen.kr(Env.perc(0.003, 0.075, 1, -5), doneAction:2);
        tone = HPF.ar(LPF.ar(WhiteNoise.ar, 8200), 5100);
        Out.ar(out, Pan2.ar(tone * env * amp, pan));
    })
];
)
