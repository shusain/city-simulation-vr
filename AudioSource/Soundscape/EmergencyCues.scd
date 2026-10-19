// Original D58 audition synthesis. Built-in SuperCollider UGens only.
// Dry mono: no distance, Doppler, reverb or board placement baked into the source.
// Parameters are supplied by Tools/Render-CityAudioAuditions.py from presets.json.
(
~d58Emergency = SynthDef(\D58Emergency, {
    |out = 0, duration = 10, centerHz = 800, depthHz = 260, sweepHz = 2.5,
    shape = 0, second = 0.22, third = 0.10, fifth = 0.03,
    detune = 0, rotorHz = 0, rotorDepth = 0, hornHz = 160, hornGain = 0,
    hornEvery = 4, hornDuty = 0.12, cutoffHz = 4200, attack = 0.12, release = 0.4|
    var triangle, alternating, rising, sweep, freq, tone, companion;
    var rotor, hornGate, horn, envelope, signal;
    triangle = LFTri.kr(sweepHz);
    alternating = Lag.kr(LFPulse.kr(sweepHz, 0, 0.5) * 2 - 1, 0.025);
    rising = Lag.kr(LFSaw.kr(sweepHz), 0.025);
    sweep = Select.kr(shape, [triangle, alternating, rising]);
    freq = centerHz + (depthHz * sweep);
    tone = SinOsc.ar(freq)
        + (SinOsc.ar(freq * 2) * second)
        + (SinOsc.ar(freq * 3) * third)
        + (SinOsc.ar(freq * 5) * fifth);
    companion = SinOsc.ar(freq * (1 + detune)) * detune.sign * 0.18;
    rotor = 1 - (rotorDepth * 0.5) + (SinOsc.kr(rotorHz) * rotorDepth * 0.5);
    hornGate = Lag.kr(LFPulse.kr(hornEvery.reciprocal, 0.12, hornDuty), 0.03);
    horn = (SinOsc.ar(hornHz) + (SinOsc.ar(hornHz * 2) * 0.4)
        + (SinOsc.ar(hornHz * 3) * 0.2)) * hornGate * hornGain;
    envelope = EnvGen.kr(Env.linen(attack, duration - attack - release, release));
    signal = LeakDC.ar(LPF.ar(((tone + companion) * rotor) + horn, cutoffHz));
    Out.ar(out, signal * envelope * 0.15);
});
)
