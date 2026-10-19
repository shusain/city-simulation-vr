"""Append the stable School definition without moving the existing catalog entries."""
import unreal
from pathlib import Path
catalog = unreal.EditorAssetLibrary.load_asset('/Game/CitySim/Data/DA_CityBuildingCatalog')
if not catalog:
    raise RuntimeError('Existing catalog missing')
definitions = list(catalog.get_editor_property('definitions'))
definition = next((d for d in definitions if str(d.get_editor_property('definition_id')) == 'School'), None)
if not definition:
    definition = unreal.CityBuildingDefinition()
definition.set_editor_property('definition_id', 'School')
definition.set_editor_property('display_name', 'Public School')
footprint = unreal.CityCellFootprint()
footprint.set_editor_property('width', 3)
footprint.set_editor_property('height', 2)
definition.set_editor_property('footprint', footprint)
for name, value in {'cost':1200, 'logical_height_cm':635.0, 'power_demand':10, 'water_demand':8,
                    'monthly_upkeep':35, 'construction_steps':25, 'presentation_yaw_offset_quarter_turns':2}.items():
    definition.set_editor_property(name, value)
definition.set_editor_property('presentation_mesh', unreal.load_asset('/Game/CitySim/Art/Meshes/SM_Building_School_A'))
definition.set_editor_property('construction_mesh', unreal.load_asset('/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A'))
index = next((i for i, d in enumerate(definitions) if str(d.get_editor_property('definition_id')) == 'School'), None)
if index is None: definitions.append(definition)
else: definitions[index] = definition
catalog.set_editor_property('definitions', definitions)
if not unreal.EditorAssetLibrary.save_loaded_asset(catalog): raise RuntimeError('School catalog save failed')
root = Path(__file__).resolve().parents[1]
(root / 'Artifacts/Diagnostics/D46-school-catalog-ready.txt').write_text('ready', encoding='utf-8')
unreal.log('CITYSIM_FIRE_CATALOG_SAVED: 3x2; $1200; upkeep35; power10; water8')
