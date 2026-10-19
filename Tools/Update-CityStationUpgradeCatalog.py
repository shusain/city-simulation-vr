"""Authored paid station stages; retain definition IDs, footprint and entrances."""
from pathlib import Path
import json
import unreal

catalog=unreal.load_asset('/Game/CitySim/Data/DA_CityBuildingCatalog')
if not catalog:raise RuntimeError('Existing catalog missing')
definitions=list(catalog.get_editor_property('definitions'))
report=[]
for definition in definitions:
    service=str(definition.get_editor_property('definition_id'))
    if service not in ('Police','Fire'):continue
    fire=service=='Fire'
    definition.set_editor_property('player_purchased_levels',True)
    definition.set_editor_property('construction_steps',1)
    levels=[]
    for index,kind in enumerate((unreal.CityDensityLevel.LOW,unreal.CityDensityLevel.MEDIUM,unreal.CityDensityLevel.HIGH)):
        level=unreal.CityBuildingLevelDefinition()
        level.set_editor_property('level',kind)
        suffix='' if index==0 else '_Medium' if index==1 else '_High'
        mesh=unreal.load_asset(f'/Game/CitySim/Art/Meshes/SM_Building_{service}{suffix}_A')
        if not mesh:raise RuntimeError('Missing station stage '+service+suffix)
        level.set_editor_property('presentation_mesh',mesh)
        policy=dict(jobs_provided=(4,8,12)[index],power_demand=(10,14,18)[index],
                    water_demand=((15,20,25) if fire else (5,8,12))[index],
                    monthly_upkeep=((30,45,65) if fire else (25,40,60))[index],
                    upgrade_cost=((0,900,1800) if fire else (0,800,1600))[index],
                    civic_construction_steps=(0,1,1)[index],civic_road_range=(24,32,40)[index],
                    civic_protection_capacity=(160,320,480)[index],civic_response_vehicles=index+1)
        for key,value in policy.items():level.set_editor_property(key,value)
        levels.append(level)
        report.append(dict(service=service,stage=index,**policy))
    definition.set_editor_property('density_levels',levels)
    definition.set_editor_property('logical_height_cm',1000.0)
catalog.set_editor_property('definitions',definitions)
if not unreal.EditorAssetLibrary.save_loaded_asset(catalog):raise RuntimeError('Station catalog save failed')
out=Path(__file__).resolve().parents[1]/'Artifacts/AssetImport/D48-station-catalog.json'
out.write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('CITYSIM_STATION_CATALOG_READY stages='+str(len(report)))
