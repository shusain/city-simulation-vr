"""Validate actual native mixer PCM against each D82 original start and seek."""
from pathlib import Path
import hashlib
import json
import wave
import argparse
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--approved",action="store_true")
args=parser.parse_args()
STAGE=ROOT/("AudioSource/Music/CitySoundtrack/UnrealApproved" if args.approved else "AudioSource/Music/CitySoundtrack/Unreal")
OUT=ROOT/("Artifacts/D82/ApprovedIntegration/NativePlayback" if args.approved else "Artifacts/D82/NativePlayback")
manifest = json.loads((STAGE/"Manifest.json").read_text())
report = json.loads((OUT/"Report.json").read_text())
if report["status"] != "Pass" or len(report["jobs"]) != len(manifest["assets"])*2:
    raise RuntimeError("Every bank source requires a native start and seek recording")

def pcm(path):
    with wave.open(str(path)) as wav:
        if (wav.getnchannels(),wav.getsampwidth(),wav.getframerate()) != (2,2,48000):
            raise RuntimeError(f"Unexpected recorded format: {path}")
        return np.frombuffer(wav.readframes(wav.getnframes()),dtype="<i2").astype(float).reshape(-1,2)/32768

records = []
for job in report["jobs"]:
    path = OUT/job["file"]
    decoded = pcm(path)
    if len(decoded)/48000 < 3.5 or np.abs(decoded).max() >= .99 or np.sqrt(np.mean(decoded**2)) < 1e-5:
        raise RuntimeError(f"Silent, clipped or truncated recording: {path}")
    entry = next(e for e in manifest["assets"] if e["id"] == job["id"])
    source = pcm(STAGE/entry["file"])
    start = round((job["seekSeconds"]+1)*48000)
    reference = source[start:start+24000].mean(axis=1)
    search = decoded[:144000].mean(axis=1)
    size = 1 << (len(search)+len(reference)-1).bit_length()
    convolution = np.fft.irfft(np.fft.rfft(search,size)*np.fft.rfft(reference[::-1],size),size)
    valid = convolution[len(reference)-1:len(search)]
    # Normalize each candidate window: late-song fades have unequal amplitudes and raw
    # convolution can select a louder, unrelated window instead of the correct seek.
    energy = np.r_[0,np.cumsum(search*search)]
    denominator = np.sqrt((energy[len(reference):]-energy[:-len(reference)])*np.dot(reference,reference))
    normalized = valid/np.maximum(denominator,1e-15)
    offset = int(np.argmax(normalized))
    actual = search[offset:offset+len(reference)]
    correlation = float(np.corrcoef(reference,actual)[0,1])
    if correlation < .95:
        raise RuntimeError(f"Native playback/seek mismatched source: {job['id']} {job['seekSeconds']} correlation={correlation}")
    records.append({**job,"correlation":correlation,"decodedSeconds":len(decoded)/48000,
                    "rms":float(np.sqrt(np.mean(decoded**2))),"sha256":hashlib.sha256(path.read_bytes()).hexdigest()})
(OUT/"DecodedVerification.json").write_text(json.dumps({"status":"Pass","records":records,
    "scope":"All actual native stereo mixer recordings match selected source starts/late seeks; human listening and device balance pending"},indent=2))
print(f"D82_NATIVE_DECODE_PASS starts={len(manifest['assets'])} lateSeeks={len(manifest['assets'])}")
