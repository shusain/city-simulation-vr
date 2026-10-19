// Original D58 ambience/effect/notification library. Built-in UGens only.
// Wildlife and crowd textures are synthetic designs, not species recordings
// or recorded voices. Outputs are dry mono; runtime owns spatial presentation.
(
~d58LibraryMake = { |kind, variant = 0|
    SynthDef(\D58Library, { |out = 0, duration = 20, seed = 580100, enveloped = 0|
        var signal, time, phrase;
        RandSeed.ir(1, seed);
        time = Line.kr(0, duration, duration);
        signal = switch(kind,
            0, { // Foliage: light irregular leaf hiss with gentle low breeze.
                BPF.ar(PinkNoise.ar, 1000 + (LFNoise2.kr(0.2) * 350), 0.7)
                    * (0.65 + (LFNoise2.kr(0.13) * 0.25))
                    + (LPF.ar(BrownNoise.ar, 260) * 0.12)
            },
            1, { // Fountain: broad water hiss and irregular small splash resonances.
                var drops;
                drops = Ringz.ar(Dust.ar(8) * 0.02, [850, 1350, 2100], [0.035, 0.02, 0.012]).sum;
                BPF.ar(WhiteNoise.ar, 2800, 0.8) * 0.35
                    + (LPF.ar(PinkNoise.ar, 1200) * 0.22) + drops
            },
            2, { // Shore: slow wash, kept separate from the fountain texture.
                var wash;
                wash = 0.35 + ((SinOsc.kr(0.1, -pi / 2) + 1) * 0.25);
                (LPF.ar(BrownNoise.ar, 900) * 0.5 + (BPF.ar(PinkNoise.ar, 1800, 1) * 0.35)) * wash
            },
            3, { // Industrial idle / working; no implied machinery locations.
                var motor, work;
                motor = SinOsc.ar(60) * 0.18 + (Saw.ar(120) * 0.05)
                    + (LPF.ar(BrownNoise.ar, 550) * 0.3);
                work = Ringz.ar(Dust.ar(5) * 0.018, [430, 970, 1670], [0.05, 0.025, 0.016]).sum
                    + (BPF.ar(PinkNoise.ar, 1300, 0.8) * 0.25)
                    + (SinOsc.ar(190) * LFPulse.kr(3.5, 0, 0.3) * 0.05);
                motor * (0.8 + (LFNoise2.kr(0.3) * 0.1)) + (work * variant)
            },
            4, { // Broad distant city activity bed, without identifiable horns.
                // Remove the wandering bass oscillator and deep noise swells.
                HPF.ar(LPF.ar(PinkNoise.ar, 1800), 180) * 0.34
                    + (BPF.ar(PinkNoise.ar, 1100, 0.8) * 0.15)
            },
            5, { // Optional stylized crowd/formant murmur; no intelligible words.
                Mix.fill(4, { |i|
                    var voice;
                    voice = Saw.ar([105, 137, 172, 213][i] + (LFNoise2.kr(0.3) * 8));
                    BPF.ar(voice, 650 + (i * 270) + (LFNoise2.kr(1.5 + (i * 0.2)) * 180), 0.18)
                        * (LFNoise2.kr(1.7 + (i * 0.1)).max(0) * 0.13)
                }) + (LPF.ar(PinkNoise.ar, 700) * 0.14)
            },
            6, { // Light wind / gustier wind. No whistles tied to a vehicle.
                var gust, body;
                gust = 0.55 + (SinOsc.kr(0.1, 0.4) * 0.25) + (LFNoise2.kr(0.14) * 0.1);
                body = LPF.ar(BrownNoise.ar, 350) * 0.25
                    + (BPF.ar(PinkNoise.ar, 800 + (variant * 650), 0.8) * 0.4);
                body * if(variant == 0, { 0.8 + (LFNoise2.kr(0.2) * 0.1) }, { gust })
            },
            7, { // Light / heavy / moderate / downpour: distinct surface density.
                var density, taps, splashes, wash;
                density = [9, 120, 40, 340][variant];
                // Longer noise envelopes make rounded drops rather than fire-like snaps.
                taps = Decay2.ar(Dust.ar(density), 0.010, 0.065)
                    * BPF.ar(PinkNoise.ar, [600, 1200, 2000], [0.8, 0.9, 1]).sum * 0.07;
                splashes = Decay2.ar(Dust.ar(density * 0.12), 0.015, 0.14)
                    * HPF.ar(LPF.ar(PinkNoise.ar, 1900), 240) * 0.08;
                wash = HPF.ar(LPF.ar(WhiteNoise.ar, [3000, 4200, 3600, 5000][variant]), 250)
                    * [0.045, 0.19, 0.10, 0.36][variant]
                    + (HPF.ar(LPF.ar(PinkNoise.ar, 1800), 150) * [0.04, 0.15, 0.08, 0.24][variant]);
                LPF.ar(taps + splashes + (wash * (0.94 + (LFNoise2.kr(0.2) * 0.06))), 4800)
            },
            8, { // Higher-pitched rough chirp-chirp pairs, spaced as before.
                var count;
                count = if(variant == 2, { 1 }, { if(variant == 0, { 2 }, { 4 }) });
                Mix.fill(count, { |i|
                    var trigger, gate, teeth, frequency, wing;
                    // Variant 2 isolates one pair for review/event scheduling.
                    // Light/dense call arrival rates and voice counts stay fixed.
                    trigger = if(variant == 2, { Impulse.kr(0) }, { Dust.kr(0.65 + (i * 0.17)) });
                    gate = EnvGen.kr(Env([0, 1, 0.8, 0, 0, 0.9, 0.7, 0],
                        [0.004, 0.060, 0.040, 0.100, 0.004, 0.055, 0.040],
                        [2, 0, -3, 0, 2, 0, -3]), trigger);
                    teeth = Decay2.ar(Impulse.ar(23 + (i * 3) + (LFNoise2.kr(2.7) * 2.5)), 0.0018, 0.013);
                    frequency = (3950 + (i * 290) + (LFNoise2.kr(6.3) * 85)) * 1.22;
                    // Modulation, saturation and band-limited rasp break up pure tones.
                    wing = (SinOsc.ar(frequency + (SinOsc.ar(137 + (i * 19)) * 170))
                        + (SinOsc.ar(frequency * 1.013) * 0.27)) * 1.7;
                    wing = BPF.ar(wing.tanh, frequency, 0.35)
                        + (BPF.ar(WhiteNoise.ar, frequency, 0.22) * 0.8);
                    wing * teeth * gate * 0.20
                })
            },
            9, { // Small fire texture: flame body and irregular crackle.
                var crackle;
                crackle = Decay2.ar(Dust.ar(14), 0.0008, 0.018) * HPF.ar(WhiteNoise.ar, 1700) * 0.23;
                LPF.ar(BrownNoise.ar, 480) * 0.25
                    + (BPF.ar(PinkNoise.ar, 1100, 1) * (0.12 + (LFNoise2.kr(0.4) * 0.04))) + crackle
            },
            10, { // Broad storm/tornado roar, independent of accepted sirens.
                (LPF.ar(BrownNoise.ar, 220) * 0.6 + (BPF.ar(PinkNoise.ar, 650, 1) * 0.3))
                    * (0.7 + (SinOsc.kr(0.05, 0.3) * 0.16) + (LFNoise2.kr(0.3) * 0.08))
            },
            11, { // Three stylized bird phrases, no species identity claimed.
                var trigger, chirp, frequency, base;
                base = [2900, 3700, 2400][variant];
                trigger = Impulse.kr([3.2, 4.4, 2.8][variant]);
                chirp = EnvGen.kr(Env.perc(0.008, [0.14, 0.10, 0.20][variant]), trigger);
                frequency = base + EnvGen.kr(Env([0, 1200, -250], [0.045, 0.13], [-2, 2]), trigger)
                    + (SinOsc.kr(27 + (variant * 9)) * 55);
                (SinOsc.ar(frequency) + (SinOsc.ar(frequency * 2) * 0.055)
                    + (BPF.ar(PinkNoise.ar, base, 0.25) * 0.035)) * chirp * 0.3
            },
            12, { // Seven short notifications. Patterns are feedback, not music.
                var notes, trigger, index, frequency, noteEnv, decay;
                notes = [[72], [60, 57], [67, 74], [60, 67, 72], [62, 62], [65, 72, 65], [69, 76]][variant];
                decay = if(variant == 3, { 0.55 }, { 0.32 });
                trigger = Impulse.kr(5.5);
                index = PulseCount.kr(trigger) - 1;
                frequency = Select.kr(index.clip(0, notes.size - 1), notes.midicps);
                noteEnv = EnvGen.kr(Env.perc(0.012, decay), trigger * (index < notes.size));
                (SinOsc.ar(frequency) + (SinOsc.ar(frequency * 2) * 0.13)
                    + (SinOsc.ar(frequency * 3) * 0.025)) * noteEnv * 0.32
            },
            13, { // Near / distant thunder, with a rolling tail.
                var crack, roll, shape;
                crack = HPF.ar(WhiteNoise.ar, 850) * EnvGen.kr(Env.perc(0.003, 0.085)) * 0.5;
                shape = EnvGen.kr(Env([0, 1, 0.7, 0], [0.22, 0.7, duration - 0.92], [-1, 0, -3]));
                roll = LPF.ar(BrownNoise.ar, if(variant == 0, { 1700 }, { 650 }))
                    * shape * (0.65 + (LFNoise2.kr(3) * 0.17));
                roll + (crack * if(variant == 0, { 1 }, { 0.07 }))
            },
            14, { // Passing combustion/tire texture; runtime still owns position.
                var frequency, engine, tires, pass, grit;
                frequency = if(variant == 0, { 91 }, { 47 }) * (1 + (LFNoise2.kr(5) * 0.016));
                engine = LPF.ar(HPF.ar(Pulse.ar(frequency, 0.23 + (LFNoise2.kr(3) * 0.025)),
                    if(variant == 0, { 85 }, { 55 })), if(variant == 0, { 1250 }, { 950 })) * 0.10;
                engine = engine + (BPF.ar(PinkNoise.ar, if(variant == 0, { 260 }, { 150 }), 0.8) * 0.22);
                tires = HPF.ar(LPF.ar(PinkNoise.ar, 6500), 420) * if(variant == 0, { 0.42 }, { 0.35 });
                grit = BPF.ar(WhiteNoise.ar, 1800, 0.9)
                    * Decay2.ar(Dust.ar(if(variant == 0, { 36 }, { 52 })), 0.001, 0.012) * 0.08;
                pass = EnvGen.kr(Env([0, 0.10, 0.52, 1, 0.48, 0.08, 0],
                    [0.19, 0.19, 0.13, 0.13, 0.19, 0.17] * duration, [2, 1, 0, -1, -2, -2]));
                (engine + tires + grit) * pass
            },
            15, { // Earthquake / construction taps / demolition impact.
                if(variant == 0, {
                    LPF.ar(BrownNoise.ar, 400) * EnvGen.kr(Env.perc(0.12, duration - 0.12)) * 0.7
                        + (HPF.ar(PinkNoise.ar, 1400) * Decay2.kr(Dust.kr(8), 0.003, 0.07) * 0.13)
                }, {
                    if(variant == 1, {
                        var trigger, index;
                        trigger = Impulse.ar(6);
                        index = PulseCount.ar(trigger);
                        Ringz.ar(trigger * (index <= 3) * 0.008, [240, 720, 1530], [0.07, 0.035, 0.02]).sum
                            + (BPF.ar(WhiteNoise.ar, 1700, 1) * Decay2.ar(trigger * (index <= 3), 0.002, 0.025) * 0.08)
                    }, {
                        LPF.ar(BrownNoise.ar, 550) * EnvGen.kr(Env.perc(0.005, 0.65)) * 0.65
                            + (BPF.ar(PinkNoise.ar, 2200, 1) * EnvGen.kr(Env.perc(0.04, 1.1)) * 0.22)
                    })
                })
            },
            16, { // Active factory bed: steady drive with intermittent tools/rattle.
                var motor, work, clatter;
                motor = SinOsc.ar(60) * 0.18 + (Saw.ar(120) * 0.05)
                    + (LPF.ar(BrownNoise.ar, 550) * 0.3);
                work = Decay2.ar(Dust.ar(0.6), 0.004, 0.33)
                    * HPF.ar(LPF.ar(PinkNoise.ar, 4200), 600) * 1.0;
                clatter = Ringz.ar(Dust.ar(1.1) * 0.035,
                    [320, 715, 1430, 2470], [0.13, 0.08, 0.047, 0.024]).sum;
                motor * 0.55 + work + clatter
            },
            17, { // Separate metal strike/rattle accent for varied work scheduling.
                var trigger, ring, impact;
                trigger = Impulse.ar(0) + (Impulse.ar(9) * (time > 0.15) * (time < 0.44) * 0.28);
                ring = Ringz.ar(trigger * 0.06, [280, 613, 1180, 2130, 3510],
                    [0.24, 0.18, 0.10, 0.065, 0.025]).sum;
                impact = HPF.ar(LPF.ar(WhiteNoise.ar, 6500), 750)
                    * Decay2.ar(trigger, 0.0008, 0.024) * 0.25;
                ring + impact
            },
            { Error("Unknown D58 library synth kind").throw }
        );
        phrase = Select.kr(enveloped, [1, EnvGen.kr(Env.linen(0.012, duration - 0.132, 0.12))]);
        Out.ar(out, LeakDC.ar(HPF.ar(signal, 25)) * phrase * 0.08);
    });
};
)
