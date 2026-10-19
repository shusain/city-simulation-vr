"""Update only private R/C/I construction durations in the existing catalog."""
import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[1]
catalog = unreal.load_asset('/Game/CitySim/Data/DA_CityBuildingCatalog')
if not catalog:
    raise RuntimeError('Missing existing building catalog')
definitions = list(catalog.get_editor_property('definitions'))
private_ids = {'Home', 'Shop', 'Factory'}
before = {str(d.get_editor_property('definition_id')): d.export_text() for d in definitions}
report = []
for definition in definitions:
    identity = str(definition.get_editor_property('definition_id'))
    if identity not in private_ids:
        continue
    if definition.get_editor_property('player_purchased_levels'):
        raise RuntimeError(f'{identity} unexpectedly uses public purchased levels')
    old_days = definition.get_editor_property('construction_steps')
    definition.set_editor_property('construction_steps', 3)
    report.append(dict(definition=identity, previous_days=old_days, construction_days=3))
if {item['definition'] for item in report} != private_ids:
    raise RuntimeError('Expected all three private building families')
for definition in definitions:
    identity = str(definition.get_editor_property('definition_id'))
    if identity not in private_ids and definition.export_text() != before[identity]:
        raise RuntimeError(f'Unrelated definition changed: {identity}')
catalog.set_editor_property('definitions', definitions)
if not unreal.EditorAssetLibrary.save_loaded_asset(catalog):
    raise RuntimeError('Growth cadence catalog save failed')
evidence = root / 'Artifacts/AssetImport/D75-growth-cadence.json'
evidence.parent.mkdir(parents=True, exist_ok=True)
evidence.write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('CITYSIM_GROWTH_CADENCE_CATALOG_READY')
