"""Render only four new steady D58 traffic stems; retain every accepted source."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import numpy as np

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('library', ROOT / 'Tools/Render-CityAudioLibrary.py')
library = importlib.util.module_from_spec(spec)
spec.loader.exec_module(library)
production, auditions = library.production, library.auditions
parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
output = args.output.resolve()
assert not output.exists() or not any(output.iterdir()), 'Preserve previous packs'
runtime = ROOT / 'Artifacts/Toolchain/SuperCollider/SuperCollider'
config = json.loads((ROOT / 'AudioSource/Soundscape/traffic-presets.json').read_text())
assert {s['id'] for s in config['samples']} == {'Car-Idle','Car-Driving','Truck-Idle','Truck-Driving'}
for folder in ('Masters','Runtime','Preview','Logs','Sources','Work/Profile/Roaming','Work/Profile/Local','Work/Profile/Temp'):
    (output / folder).mkdir(parents=True, exist_ok=True)
sources = [ROOT/'AudioSource/Soundscape/TrafficLoops.scd', ROOT/'AudioSource/Soundscape/traffic-presets.json', Path(__file__),
           ROOT/'Tools/Render-CityAudioLibrary.py', ROOT/'Tools/Render-CityAudioProduction.py', ROOT/'Tools/Render-CityAudioAuditions.py']
for p in sources: shutil.copy2(p, output/'Sources'/p.name)
# Existing renderer safely compiles this dedicated source as SampleLibrary.scd.
shutil.copy2(sources[0], output/'Sources/SampleLibrary.scd')
library.SOURCE = output/'Sources'
env = os.environ.copy()
env.pop('QT_QPA_PLATFORM', None)
env.update(APPDATA=str(output/'Work/Profile/Roaming'), LOCALAPPDATA=str(output/'Work/Profile/Local'),
           TEMP=str(output/'Work/Profile/Temp'), TMP=str(output/'Work/Profile/Temp'))
(output/'Work/sclang.yaml').write_text('includePaths:\n  - '+json.dumps((runtime/'SCClassLibrary').as_posix())+'\nexcludePaths: []\npostInlineWarnings: false\n')
ffmpeg, ffprobe = shutil.which('ffmpeg'), shutil.which('ffprobe')
assert ffmpeg and ffprobe
for binary in ('sclang.exe','scsynth.exe'):
    assert '3.14.1' in auditions.run([runtime/binary,'-v']).stdout.decode()
assets = {}
for i, sample in enumerate(config['samples']):
    signal = library.render_sample(sample, 900+i, output, runtime, env, ffmpeg, ffprobe, production.frames(.04))
    gain, gain_db = library.calibrated_gain(signal, sample['id'], sample['targetLUFS'], -9, output, ffmpeg)
    signal *= gain
    record = production.write_asset(output, sample['id'], signal, ffmpeg, -9, loop=True)
    record.update(group=sample['group'],intent=sample['intent'],loop=True,targetLUFS=sample['targetLUFS'],
                  staticGainDB=gain_db,seed=581000+i,normalization='Static gain')
    assets[sample['id']] = record
    library.review_copy(output, sample['id'], production.quiet_edges(signal,start=True,end=True), ffmpeg)
    library.review_copy(output, sample['id']+'-Repeat', production.quiet_edges(np.tile(signal,2),start=True,end=True), ffmpeg)
    print('D58_TRAFFIC_STEM_PASS', sample['id'], flush=True)
manifest = dict(task='D58',config=config,assets=assets,categoryReels={},scenes={},
                sourceSha256={p.name:auditions.sha(output/'Sources'/p.name) for p in sources},
                runtimePCMBytes=sum((output/r['runtime']).stat().st_size for r in assets.values()),
                provenance='Original seeded SuperCollider synthesis; no third-party audio',
                scope='Dry steady engines; position/velocity/spatialization and Doppler are runtime-owned')
(output/'Manifest.json').write_text(json.dumps(manifest,indent=2))
(output/'Validation.json').write_text(json.dumps(dict(status='Pass',sha256={p.relative_to(output).as_posix():auditions.sha(p)
    for folder in ('Masters','Runtime','Preview') for p in (output/folder).iterdir()}),indent=2))
print('D58_TRAFFIC_SOURCE_PASS', output)
