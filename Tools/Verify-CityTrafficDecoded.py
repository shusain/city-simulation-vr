"""Check four steady traffic SoundCues against staged PCM and continuous repetition."""
from pathlib import Path
import json,wave,hashlib
import numpy as np
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D58/TrafficAlarmRefinement'
manifest=json.loads((ROOT/'AudioSource/Soundscape/Unreal/Manifest.json').read_text())
playback=json.loads((OUT/'BankPlayback/NativePlayback.json').read_text());assert playback['status']=='Pass' and len(playback['jobs'])==8
def pcm(path,channels):
    with wave.open(str(path),'rb') as w:
        assert (w.getnchannels(),w.getsampwidth(),w.getframerate())==(channels,2,48000),path
        signal=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(float).reshape(-1,channels).mean(axis=1)/32768
    return signal
records=[]
for job in playback['jobs']:
    path=OUT/'BankPlayback'/job['file'];decoded=pcm(path,2)
    assert len(decoded)/48000>=job['seconds']-.2 and np.max(np.abs(decoded))<.99
    assert np.sqrt(np.mean(decoded**2))>1e-5
    entry=next(e for e in manifest['assets'] if e['id']==job['id'])
    source=pcm(ROOT/'AudioSource/Soundscape/Unreal'/entry['file'],1)
    reference=source[:48000];search=decoded[:144000]
    n=1<<(len(search)+len(reference)-1).bit_length()
    values=np.fft.irfft(np.fft.rfft(search,n)*np.fft.rfft(reference[::-1],n),n)
    valid=values[len(reference)-1:len(search)]
    offset=int(np.argmax(valid));section=search[offset:offset+len(reference)]
    correlation=float(np.corrcoef(reference,section)[0,1]);assert correlation>.75,(job['id'],correlation)
    repeat_correlation=None
    if job['repeated']:
        assert job['playingAtEnd']
        a=decoded[offset+24000:offset+240000]
        b=decoded[offset+984000:offset+1200000]
        repeat_correlation=float(np.corrcoef(a,b)[0,1]);assert repeat_correlation>.98,(job['id'],repeat_correlation)
    records.append(dict(id=job['id'],file=job['file'],duration=len(decoded)/48000,source_correlation=correlation,
        repeated=job['repeated'],repeat_correlation=repeat_correlation,source_offset_seconds=offset/48000,
        sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
(OUT/'DecodedVerification.json').write_text(json.dumps(dict(status='Pass',native_recordings=8,repeat_loops=4,records=records,
    scope='Actual decoded 48kHz stereo mixer output matches revised PCM and repeats continuously; subjective/device listening remains pending'),indent=2),encoding='utf-8')
print('D58_TRAFFIC_NATIVE_DECODED_PASS 8 recordings,4 repeats')
