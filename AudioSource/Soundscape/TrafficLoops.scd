// Original steady dry engines. No pass envelope, stereo movement or baked Doppler.
(
~d58LibraryMake = { |kind, variant = 0|
    SynthDef(\D58Library, { |out = 0, duration = 20, seed = 581000, enveloped = 0|
        var truck, driving, frequency, engine, body, tires, signal;
        RandSeed.ir(1, seed);
        truck = variant >= 2;
        driving = (variant % 2) == 1;
        frequency = [42, 78, 29, 48][variant] * (1 + (LFNoise2.kr(2) * 0.006));
        engine = LPF.ar(Pulse.ar(frequency, 0.26), [780, 1100, 550, 850][variant]) * 0.12
            + (SinOsc.ar(frequency * 2) * 0.07);
        body = BPF.ar(PinkNoise.ar, if(truck, { 155 }, { 280 }), 0.8) * 0.20;
        tires = HPF.ar(LPF.ar(PinkNoise.ar, 4200), 500) * if(driving, { 0.32 }, { 0.0 });
        signal = (engine + body) * (0.95 + (LFNoise2.kr(0.7) * 0.025)) + tires;
        Out.ar(out, LeakDC.ar(signal));
    })
};
)
