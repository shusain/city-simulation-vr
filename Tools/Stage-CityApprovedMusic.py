"""Stage the user-approved D82 set for Unreal without touching previous sources."""
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import wave
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
ALBUM=ROOT/"Artifacts/D82/Soundtrack3"
STAGE=ROOT/"AudioSource/Music/CitySoundtrack/UnrealApproved"
OUT=ROOT/"Artifacts/D82/ApprovedIntegration"

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    album=json.loads((ALBUM/"Manifest.json").read_text())
    assert len(album["tracks"])==9
    STAGE.mkdir(parents=True,exist_ok=True);OUT.mkdir(parents=True,exist_ok=True)
    module=importlib.util.spec_from_file_location("hybrid",ROOT/"Tools/Render-CityHybridMusic.py")
    hybrid=importlib.util.module_from_spec(module);module.loader.exec_module(hybrid)
    records=[]
    for track in album["tracks"]:
        master=ALBUM/track["master"]
        assert sha(master)==track["masterSha256"]
        with wave.open(str(master)) as wav:
            assert (wav.getnchannels(),wav.getframerate(),wav.getsampwidth())==(2,48000,3)
            frames=wav.getnframes()
        samples=hybrid.decode(shutil.which("ffmpeg"),master)
        path=STAGE/(track["id"]+".wav")
        if path.exists():
            # A previous interrupted staging may not have written its manifest.
            # Reuse only an exact quantization of this approved master; never overwrite.
            with wave.open(str(path)) as wav:
                assert (wav.getnchannels(),wav.getframerate(),wav.getsampwidth(),wav.getnframes())==(2,48000,2,frames)
                assert wav.readframes(frames)==np.rint(samples*32767).astype("<i2").tobytes()
        else:
            hybrid.music.write_wave(path,samples,16)
        decoded=hybrid.decode(shutil.which("ffmpeg"),path)
        error=float(np.max(np.abs(samples-decoded)))
        assert error<=1/32768 and len(decoded)==frames
        records.append({"id":track["id"],"title":track["title"],"file":path.name,"sha256":sha(path),
                        "masterSha256":track["masterSha256"],"approvedPreviewSha256":track["mp3Sha256"],
                        "frames":frames,"seconds":frames/48000,"channels":2,"sampleRate":48000,"bits":16,
                        "quantizationMaxError":error,"package":f'/Game/CitySim/Audio/Music/Approved/SW_Music_{track["id"]}',
                        "renderManifest":track["renderManifest"],"method":track["method"]})
        print(f'D82_APPROVED_STAGE {track["id"]}',flush=True)
    manifest={"schema":1,"task":"D82","createdUTC":datetime.now(timezone.utc).isoformat(),
              "status":"All nine selected recordings user-approved2026-10-13; exact import source bank staged",
              "integrationEvidence":"Artifacts/D82/ApprovedIntegration/Implementation.json",
              "approval":{"source":"Direct human request in Review pending validation and tasks", "date":"2026-10-13",
                          "text":"Approved I think these are good tracks to go with integrating just coordinate with the other agent sessions and get the changes integrated into the unreal project.",
                          "previousReceipts":[album["feedbackSource"],album["earlierApprovalsSource"]]},
              "albumManifest":"Artifacts/D82/Soundtrack3/Manifest.json","albumManifestSha256":sha(ALBUM/"Manifest.json"),
              "method":"Eight original synthesis recordings; Neon retains documented ACE-Step texture provenance. Approved24-bit masters converted to16-bit PCM, without resampling/gain/effects.",
              "assets":records}
    (STAGE/"Manifest.json").write_text(json.dumps(manifest,indent=2))
    (OUT/"StagingVerification.json").write_text(json.dumps({"task":"D82","status":"Pass","assets":records,
        "checks":"Nine approved source/master hashes, stereo48kHz16-bit/frame counts, <=one16-bit-LSB error; original draft bank and all audition files preserved"},indent=2))
    print('D82_APPROVED_STAGE_PASS nine waves')

if __name__=="__main__":main()
