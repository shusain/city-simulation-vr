"""Append the D60 hospital without changing existing definition order/policies."""
from pathlib import Path
import json
import unreal

root = Path(__file__).resolve().parents[1]
catalog = unreal.load_asset('/Game/CitySim/Data/DA_CityBuildingCatalog')
if not catalog:
    raise RuntimeError('Existing catalog missing')
definitions = list(catalog.get_editor_property('definitions'))
hospital = unreal.CityBuildingDefinition()
footprint = unreal.CityCellFootprint()
footprint.set_editor_property('width',4)
footprint.set_editor_property('height',3)
starter = unreal.load_asset('/Game/CitySim/Art/Meshes/SM_Building_Hospital_A')
expanded = unreal.load_asset('/Game/CitySim/Art/Meshes/SM_Building_Hospital_Medium_A')
if not starter or not expanded:
    raise RuntimeError('Import hospital meshes first')
values = dict(definition_id='Hospital',display_name='Hospital',footprint=footprint,cost=2400,
              presentation_mesh=starter,logical_height_cm=1100,entrance_sides=1,
              presentation_yaw_offset_quarter_turns=2,construction_steps=5,
              player_purchased_levels=True,jobs_provided=6,minimum_staff_education=2,
              monthly_upkeep=70,power_demand=20,water_demand=20)
for key,value in values.items():
    hospital.set_editor_property(key,value)
fire = next(d for d in definitions if str(d.get_editor_property('definition_id'))=='Fire')
hospital.set_editor_property('construction_mesh',fire.get_editor_property('construction_mesh'))
levels = []
report = []
for index,kind in enumerate((unreal.CityDensityLevel.LOW,unreal.CityDensityLevel.MEDIUM)):
    level = unreal.CityBuildingLevelDefinition()
    level.set_editor_property('level',kind)
    level.set_editor_property('presentation_mesh',starter if index==0 else expanded)
    policy = dict(jobs_provided=(6,10)[index],monthly_upkeep=(70,110)[index],
                  power_demand=(20,30)[index],water_demand=(20,30)[index],
                  upgrade_cost=(0,1800)[index],civic_construction_steps=(0,5)[index],
                  civic_road_range=48,civic_protection_capacity=(24,48)[index],civic_response_vehicles=index+1)
    for key,value in policy.items():
        level.set_editor_property(key,value)
    levels.append(level)
    report.append(dict(stage=index,**policy))
hospital.set_editor_property('density_levels',levels)
existing = next((i for i,d in enumerate(definitions) if str(d.get_editor_property('definition_id'))=='Hospital'),None)
if existing is None:
    definitions.append(hospital)
else:
    definitions[existing] = hospital
catalog.set_editor_property('definitions',definitions)
if not unreal.EditorAssetLibrary.save_loaded_asset(catalog):
    raise RuntimeError('Hospital catalog save failed')
(root/'Artifacts/AssetImport/D60-hospital-catalog.json').write_text(json.dumps({'cost':2400,'education':2,'build_days':5,'levels':report},indent=2),encoding='utf-8')
unreal.log('CITYSIM_HOSPITAL_CATALOG_READY')
