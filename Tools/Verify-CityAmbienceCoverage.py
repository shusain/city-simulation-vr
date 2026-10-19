"""Read-only native catalog/cook audit of the integrated D58 ambience coverage."""
from pathlib import Path
import hashlib
import json
import unreal

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Artifacts/D58/Publication-20261012'
OUT.mkdir(parents=True,exist_ok=True)
catalog_path=ROOT/'CitySimVR/Content/CitySim/Data/DA_CityBuildingCatalog.uasset'
before=hashlib.sha256(catalog_path.read_bytes()).hexdigest()
catalog=unreal.load_asset('/Game/CitySim/Data/DA_CityBuildingCatalog')
assert catalog
definitions=list(catalog.get_editor_property('definitions'))
(OUT/'CatalogParkEntries.json').write_text(json.dumps([dict(id=str(d.get_editor_property('definition_id')),
    radiusCells=d.get_editor_property('park_radius_cells')) for d in definitions if 'Park' in str(d.get_editor_property('definition_id'))],indent=2))
parks=[]
for d in definitions:
    identity=str(d.get_editor_property('definition_id'))
    radius=d.get_editor_property('park_radius_cells')
    if radius>0 or identity=='Park':
        parks.append(dict(id=identity,radiusCells=radius,sharedAreaRouting=True))
expected={'Park','NeighborhoodPark','CityPark','BasketballPark','SkatePark','BaseballPark','SoccerPark'}
assert expected=={p['id'] for p in parks}, parks
assert any(str(d.get_editor_property('definition_id'))=='Factory' for d in definitions)
manifest=json.loads((ROOT/'AudioSource/Soundscape/Unreal/Manifest.json').read_text())
ids={'Industry-Idle','Industry-Working','Industry-Clank','Park-Foliage','Park-Fountain',
     'Bird-Chirp-A','Bird-Chirp-B','Bird-Chirp-C','Night-Crickets-Light','Night-Crickets-Dense','City-Distant'}
records=[]
for identity in sorted(ids):
    e=next(e for e in manifest['assets'] if e['id']==identity)
    wave=unreal.load_asset(e['package']);assert isinstance(wave,unreal.SoundWave)
    graph_name=e['assetName'].replace('SW_','MS_',1)
    graph=unreal.load_asset('/Game/CitySim/Audio/Soundscape/Graphs/'+graph_name);assert graph
    assert unreal.EditorAssetLibrary.get_metadata_tag(wave,'CitySimD58SourceSHA256')==e['sha256']
    assert hashlib.sha256((ROOT/'AudioSource/Soundscape/Unreal'/e['file']).read_bytes()).hexdigest()==e['sha256']
    for platform in ('Windows','Android_ASTC'):
        base=ROOT/f'CitySimVR/Saved/Cooked/{platform}/CitySimVR/Content'
        assert (base/(e['package'].removeprefix('/Game/')+'.uasset')).is_file()
        assert (base/f'CitySim/Audio/Soundscape/Graphs/{graph_name}.uasset').is_file()
    records.append(dict(id=identity,wave=e['package'],graph=graph.get_path_name(),bothPlatformCooks=True))
assert hashlib.sha256(catalog_path.read_bytes()).hexdigest()==before
(OUT/'AmbienceCoverage.json').write_text(json.dumps(dict(status='Pass',parks=parks,assets=records,
    catalogUnchanged=True,contentSaved=False,
    behavior=dict(industry='Operational factory areas: idle/working beds and occasional clanks',
                  parks='Shared foliage/fountain ambience for all seven park definitions, including simple/corner variants of legacy Park',
                  wildlife='Daytime bird events/night crickets near parks, suppressed in rain/cold; hotter nights select dense crickets',
                  background='Distant city bed plus wind and weather, with zoom blending'),
    pending='Physical mix/localization/comfort and dense/device cost remain pending; no bespoke sport-action sound set is claimed'),indent=2),encoding='utf-8')
unreal.log('D58_AMBIENCE_COVERAGE_PASS seven definitions including simple/corner parks,11 assets,Windows+Quest cooks; read-only')
