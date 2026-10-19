"""Create only the four new steady D58 traffic waves under the exclusive native window."""
from pathlib import Path
import hashlib
import json
import re
import unreal

ROOT = Path(__file__).resolve().parents[1]
assert re.findall(r'\*\*Native window OWNED BY ([A-Z]+\d+)\b',(ROOT/'docs/DEVELOPMENT_TASKS.md').read_text(encoding='utf-8')) == ['D58']
manifest = json.loads((ROOT/'AudioSource/Soundscape/Unreal/Manifest.json').read_text())
selected = [e for e in manifest['assets'] if e['bank']=='Traffic']; assert len(selected)==4
library = unreal.EditorAssetLibrary
assert not any(library.does_asset_exist(e['package']) for e in selected), 'New packages only; preserve existing native sources'
records=[]
for e in selected:
    path=ROOT/'AudioSource/Soundscape/Unreal'/e['file'];assert hashlib.sha256(path.read_bytes()).hexdigest()==e['sha256']
    task=unreal.AssetImportTask();factory=unreal.SoundFactory();factory.set_editor_property('auto_create_cue',False)
    for k,v in dict(filename=str(path),destination_path=e['package'].rsplit('/',1)[0],destination_name=e['assetName'],
        automated=True,replace_existing=False,replace_existing_settings=False,save=False).items():task.set_editor_property(k,v)
    task.set_editor_property('factory',factory);unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sound=library.load_asset(e['package']);assert isinstance(sound,unreal.SoundWave)
    for k,v in dict(looping=True,volume=1.0,pitch=1.0).items():sound.set_editor_property(k,v)
    assert sound.get_editor_property('num_channels')==1 and sound.get_editor_property('imported_sample_rate')==48000
    assert abs(sound.get_editor_property('duration')-20)<.001
    library.set_metadata_tag(sound,'CitySimD58SourceSHA256',e['sha256']);library.set_metadata_tag(sound,'CitySimD58SourceID',e['id'])
    assert library.save_loaded_asset(sound)
    records.append(dict(id=e['id'],package=e['package'],sourceSha256=e['sha256']))
(ROOT/'Artifacts/D58/TrafficAlarmRefinement/Import.json').write_text(json.dumps(dict(status='Pass',assets=records),indent=2))
unreal.log('D58_TRAFFIC_IMPORT_PASS 4')
