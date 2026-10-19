"""Compare the four revised rain textures and verify46 untouched staged waveforms."""
from pathlib import Path
import json,hashlib,wave
import numpy as np
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D58/RainSirenRefinement'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def metrics(path):
    with wave.open(str(path),'rb') as w:
        assert (w.getnchannels(),w.getframerate(),w.getsampwidth(),w.getnframes())==(1,48000,2,960000)
        samples=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(float)/32768
    windows=samples.reshape(-1,240)
    rms=np.sqrt(np.mean(windows**2,axis=1))
    spectrum=np.abs(np.fft.rfft(samples))**2;freq=np.fft.rfftfreq(len(samples),1/48000)
    return dict(short_rms_p99_to_median_db=float(20*np.log10(np.percentile(rms,99)/np.median(rms))),
        crest_db=float(20*np.log10(np.max(np.abs(samples))/np.sqrt(np.mean(samples**2)))),
        energy_above_4khz_percent=float(100*spectrum[freq>=4000].sum()/spectrum.sum()))
manifest=json.loads((ROOT/'AudioSource/Soundscape/Unreal/Manifest.json').read_text())
changes=json.loads((OUT/'RainStage.json').read_text())['changes'];selected={e['id'] for e in changes};records=[]
for e in manifest['assets']:
    old=OUT/'Before/AudioSource/Soundscape/Unreal'/e['file'];new=ROOT/'AudioSource/Soundscape/Unreal'/e['file']
    if e['id'] not in selected:assert sha(old)==sha(new),e['id'];continue
    before=metrics(old);after=metrics(new)
    assert after['short_rms_p99_to_median_db']<before['short_rms_p99_to_median_db'],e['id']
    assert after['energy_above_4khz_percent']<before['energy_above_4khz_percent'],e['id']
    records.append(dict(id=e['id'],before=before,after=after,lufs=e['integratedLUFS']))
(OUT/'RainComparison.json').write_text(json.dumps(dict(status='Pass',rain=records,other46_waves_byte_identical=True,
    scope='Reduced short envelope spikes/high-frequency energy; naturalness remains a listening judgement'),indent=2),encoding='utf-8')
print('D58_RAIN_TEXTURE_COMPARISON_PASS')
