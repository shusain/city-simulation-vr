"""CI-owned five-source import. Preserve all unrelated native/audio assets."""
from pathlib import Path
import hashlib
import json
import unreal

ROOT = Path(__file__).resolve().parents[1]
EXPORT = ROOT / 'AudioSource/Generated/recordings/issue-54/v1'
import sys
sys.path.insert(0, str(ROOT / 'Tools'))
from Assets.Manifests.CityApprovedSources import verified_export

PINNED_APPROVAL = 'fd6977b44e1b325a81d055147a7e1c9d203a9799609b739ad5e32742bdc86f24'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
handoff = verified_export(EXPORT, 54, 1, PINNED_APPROVAL, kind='audio')

library = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
products = []
records = []
for entry in handoff['assets']:
    source = ROOT / entry['runtimeFile']
    assert sha(source) == entry['sha256']
    sound = unreal.load_asset(entry['package'])
    if sound:
        prior = library.get_metadata_tag(sound, 'CitySimD58SourceSHA256')
        assert prior in (entry['oldSha256'], entry['sha256']), (entry['id'], prior)
    task = unreal.AssetImportTask()
    factory = unreal.SoundFactory()
    factory.set_editor_property('auto_create_cue', False)
    for key, value in dict(filename=str(source), destination_path=entry['package'].rsplit('/', 1)[0],
                           destination_name=entry['assetName'], automated=True, replace_existing=True,
                           replace_existing_settings=False, save=False).items():
        task.set_editor_property(key, value)
    task.set_editor_property('factory', factory)
    tools.import_asset_tasks([task])
    sound = unreal.load_asset(entry['package'])
    assert isinstance(sound, unreal.SoundWave)
    for key, value in dict(looping=True, volume=1., pitch=1.).items():
        sound.set_editor_property(key, value)
    assert sound.get_editor_property('num_channels') == 1
    assert sound.get_editor_property('imported_sample_rate') == 48000
    assert abs(sound.get_editor_property('duration') - entry['seconds']) < .001
    library.set_metadata_tag(sound, 'CitySimD58SourceSHA256', entry['sha256'])
    library.set_metadata_tag(sound, 'CitySimD58SourceID', entry['id'])
    assert library.save_loaded_asset(sound)
    products.append('CitySimVR/Content/' + entry['package'].removeprefix('/Game/') + '.uasset')
    records.append(dict(id=entry['id'], sha256=entry['sha256'], seconds=entry['seconds']))

# Duplicate the accepted Doppler/looping-player structure for B. Change its
# reflected soft and loaded wave references together, then verify serialized
# routing in the separate native bank test process.
alternate = '/Game/CitySim/Audio/Soundscape/Graphs/SC_Car_Driving_Alternate'
cue = unreal.load_asset(alternate)
if not cue:
    cue = library.duplicate_asset('/Game/CitySim/Audio/Soundscape/Graphs/SC_Car_Driving', alternate)
assert isinstance(cue, unreal.SoundCue)
player = cue.get_editor_property('first_node').get_editor_property('child_nodes')[0]
assert isinstance(player, unreal.SoundNodeWavePlayer)
wave = unreal.load_asset('/Game/CitySim/Audio/Soundscape/Ambience/City/SW_Car_Driving_Alternate')
player.set_editor_property('sound_wave_asset_ptr', wave)
player.set_editor_property('sound_wave', wave)
assert library.save_loaded_asset(cue)
products.append('CitySimVR/Content/CitySim/Audio/Soundscape/Graphs/SC_Car_Driving_Alternate.uasset')

out = ROOT / 'Artifacts/CIAssetPreparation'
out.mkdir(parents=True, exist_ok=True)
(out / 'Files.txt').write_text('\n'.join(products) + '\n')
(out / 'RecordedEngines55.json').write_text(json.dumps(dict(status='Pass', assets=records, products=products), indent=2))
unreal.log('RECORDED_ENGINES55_IMPORTED five waves and alternate native cue')
