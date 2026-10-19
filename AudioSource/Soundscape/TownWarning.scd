// Original reference-informed mechanical town-warning synthesis.
// No recorded sample playback: chopper harmonics, air/motor texture and inertia.
// Fixed noise seed makes the offline candidates reproducible.
(
~d58Emergency = SynthDef(\D58Emergency, {
    |out = 0, duration = 24, nominalHz = 533, lowerRatio = 0.35,
    spinUp = 3.5, spinDown = 5, sweepHz = 0.08, wailDepth = 0,
    secondaryRatio = 1.2, secondaryGain = 0, brightness = 0.8,
    rotationHz = 0.05, rotationDepth = 0.3, airGain = 0.015,
    motorGain = 0.035, cutoffHz = 3200, seed = 5802, lowerOctaveGain = 0|
    var speed, drift, freq, pulse, companion, weights, rotation;
    var motor, air, envelope, signal, lowerTone;
    RandSeed.ir(1, seed);
    // Motor startup and coast change pitch as well as level; no abrupt saw reset.
    speed = EnvGen.kr(Env(
        [lowerRatio, 1, 1, lowerRatio * 0.4],
        [spinUp, duration - spinUp - spinDown, spinDown],
        [-2, 0, -1]
    ));
    drift = 1 - (wailDepth * 0.5) + (SinOsc.kr(sweepHz, -pi / 2) * wailDepth * 0.5);
    freq = nominalHz * speed * drift;
    // Additive port/chopper approximation retains even and odd harmonics,
    // unlike the earlier largely sinusoidal wail. Bandwidth remains bounded.
    weights = [1, 0.48, 0.34, 0.22, 0.15, 0.10, 0.065, 0.04];
    pulse = Mix.fill(8, { |i|
        SinOsc.ar(freq * (i + 1)) * weights[i] * brightness.pow(i * 0.4)
    });
    companion = Mix.fill(8, { |i|
        SinOsc.ar(freq * secondaryRatio * (i + 1)) * weights[i]
            * brightness.pow(i * 0.4)
    }) * secondaryGain;
    // Optional quiet octave under both held tones. Keep this independent of
    // nominal pitch so the review can distinguish pitch reduction from body.
    lowerTone = (SinOsc.ar(freq * 0.5)
        + (SinOsc.ar(freq * secondaryRatio * 0.5) * secondaryGain)) * lowerOctaveGain;
    // A slow projected-horn swell is separate from motor pitch and texture.
    rotation = 1 - (rotationDepth * 0.5)
        + (SinOsc.kr(rotationHz, 0.3) * rotationDepth * 0.5);
    motor = SinOsc.ar(freq / 5) * motorGain;
    air = BPF.ar(PinkNoise.ar, 1300 * speed.sqrt + 300, 0.8) * airGain;
    envelope = EnvGen.kr(Env.linen(0.2, duration - 1.4, 1.2)) * speed.pow(0.7);
    signal = LeakDC.ar(LPF.ar(pulse + companion + motor + air + lowerTone, cutoffHz));
    Out.ar(out, signal * rotation * envelope * 0.12);
});
)
