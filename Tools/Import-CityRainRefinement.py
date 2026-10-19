"""Reimport only the four explicitly revised D58 rain waves, preserving ownership/settings."""
from pathlib import Path
import json,hashlib,re
import unreal
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D58/RainSirenRefinement'
assert re.findall(r'\*\*Native window OWNED BY ([A-Z]+\d+)\b',(ROOT/'docs/DEVELOPMENT_TASKS.md').read_text(encoding='utf-8'))==['D58']
manifest=json.loads((ROOT/'AudioSource/Soundscape/Unreal/Manifest.json').read_text(encoding='utf-8'))
changes=json.loads((OUT/'RainStage.json').read_text())['changes'];assert len(changes)==4
tools=unreal.AssetToolsHelpers.get_asset_tools();library=unreal.EditorAssetLibrary;records=[]
for change in changes:
    entry=next(e for e in manifest['assets'] if e['id']==change['id'])
    sound=unreal.load_asset(entry['package']);assert isinstance(sound,unreal.SoundWave)
    assert library.get_metadata_tag(sound,'CitySimD58SourceSHA256') in (change['old_sha256'],change['new_sha256'])
    assert sound.get_editor_property('volume')==1 and sound.get_editor_property('pitch')==1 and sound.get_editor_property('looping')
    native=ROOT/('CitySimVR/Content/'+entry['package'].removeprefix('/Game/')+'.uasset')
    backup=OUT/'BeforeNative'/native.relative_to(ROOT);backup.parent.mkdir(parents=True,exist_ok=True)
    if not backup.exists():backup.write_bytes(native.read_bytes())
    source=ROOT/'AudioSource/Soundscape/Unreal'/entry['file']
    assert hashlib.sha256(source.read_bytes()).hexdigest()==entry['sha256']
    task=unreal.AssetImportTask();factory=unreal.SoundFactory();factory.set_editor_property('auto_create_cue',False)
    for k,v in dict(filename=str(source),destination_path=entry['package'].rsplit('/',1)[0],destination_name=entry['assetName'],
        automated=True,replace_existing=True,replace_existing_settings=False,save=False).items():task.set_editor_property(k,v)
    task.set_editor_property('factory',factory);tools.import_asset_tasks([task]);sound=unreal.load_asset(entry['package'])
    for k,v in dict(looping=True,volume=1.0,pitch=1.0).items():sound.set_editor_property(k,v)
    assert abs(sound.get_editor_property('duration')-20)<.001
    assert sound.get_editor_property('num_channels')==1 and sound.get_editor_property('imported_sample_rate')==48000
    library.set_metadata_tag(sound,'CitySimD58SourceSHA256',entry['sha256']);assert library.save_loaded_asset(sound)
    records.append(dict(id=entry['id'],asset=entry['package'],sha256=entry['sha256']))
(OUT/'RainImport.json').write_text(json.dumps(dict(status='Pass',assets=records,otherNativeAudioPreserved=True),indent=2),encoding='utf-8')
unreal.log('D58_RAIN_NATIVE_IMPORT_PASS 4')
