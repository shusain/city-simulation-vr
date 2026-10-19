"""Check a native PIE recording using the workspace Python runtime and NumPy."""
from pathlib import Path
import json
import wave
import numpy as np

output = Path(__file__).resolve().parents[1] / 'Artifacts/Preview'
with wave.open(str(output / 'D57-CityAlarm.wav'), 'rb') as recording:
    channels = recording.getnchannels()
    rate = recording.getframerate()
    frames = recording.getnframes()
    assert recording.getsampwidth() == 2, 'Expected Unreal PCM16 export'
    samples = np.frombuffer(recording.readframes(frames), dtype='<i2').astype(float) / 32768
mono = samples.reshape(-1, channels).mean(axis=1)
active = np.where(np.abs(mono) > .001)[0]
assert len(active) > rate * 3, 'Recorded output is silent or truncated'
span = (active[-1] - active[0]) / rate
assert 4.5 < span < 5.3, f'Unexpected audible duration: {span}'
peak = float(np.abs(samples).max())
assert .03 < peak < .95, f'Unexpected peak level: {peak}'
frequencies = []
for second in (.7, 1.2, 2, 2.7, 3.2, 4):
    offset = active[0] / rate + second
    chunk = mono[int(offset * rate):int((offset + .08) * rate)]
    spectrum = np.abs(np.fft.rfft(chunk * np.hanning(len(chunk))))
    frequencies.append(float(np.fft.rfftfreq(len(chunk), 1 / rate)[np.argmax(spectrum)]))
assert max(frequencies) - min(frequencies) > 200, 'Siren frequency sweep is missing'
tail_rms = float(np.sqrt(np.mean(mono[-int(.3 * rate):] ** 2)))
assert tail_rms < .001, 'Cue did not end quietly'
result = {
    'sampleRate': rate, 'channels': channels, 'capturedSeconds': frames / rate,
    'audibleSeconds': span, 'peak': peak,
    'clippedSamples': int(np.count_nonzero(np.abs(samples) >= .999)),
    'sampledPeakFrequenciesHz': frequencies, 'tailRMS': tail_rms,
}
(output / 'D57-AlarmWaveform.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))
