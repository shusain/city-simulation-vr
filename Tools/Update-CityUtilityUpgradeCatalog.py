"""Utility expansions retain stable tuning and reserve their largest reviewed site."""
from pathlib import Path
import unreal,json
root=Path(__file__).resolve().parents[1]
catalog=unreal.load_asset('/Game/CitySim/Data/DA_CityBuildingCatalog')
definitions=list(catalog.get_editor_property('definitions')); report=[]
for d in definitions:
    service=str(d.get_editor_property('definition_id'))
    if service not in ('Power','Service'):continue
    footprint=d.get_editor_property('footprint')
    footprint.set_editor_property('width',4 if service=='Power' else 6)
    footprint.set_editor_property('height',4);d.set_editor_property('footprint',footprint)
    d.set_editor_property('presentation_yaw_offset_quarter_turns',2)
    construction=unreal.load_asset('/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A');assert construction
    d.set_editor_property('construction_mesh',construction)
    d.set_editor_property('player_purchased_levels',True)
    levels=[]
    for i,kind in enumerate((unreal.CityDensityLevel.LOW,unreal.CityDensityLevel.MEDIUM,unreal.CityDensityLevel.HIGH)):
        l=unreal.CityBuildingLevelDefinition();l.set_editor_property('level',kind)
        suffix='' if i==0 else '_Medium' if i==1 else '_High'
        mesh=unreal.load_asset(f'/Game/CitySim/Art/Meshes/SM_Building_{service}{suffix}_A')
        if not mesh:raise RuntimeError('Missing utility mesh')
        l.set_editor_property('presentation_mesh',mesh)
        values=dict(jobs_provided=(2,4,6)[i],utility_supply_percent=(100,200,300)[i],monthly_upkeep=((50,75,100) if service=='Power' else (30,45,60))[i],upgrade_cost=((0,600,1000) if service=='Power' else (0,450,800))[i],civic_construction_steps=(0,5,10)[i],pollution_emission=0)
        for k,v in values.items():l.set_editor_property(k,v)
        levels.append(l);report.append(dict(service=service,stage=i,**values))
    d.set_editor_property('density_levels',levels)
    d.set_editor_property('logical_height_cm',2000.0)
catalog.set_editor_property('definitions',definitions)
if not unreal.EditorAssetLibrary.save_loaded_asset(catalog):raise RuntimeError('Utility catalog save failed')
(root/'Artifacts/AssetImport/D56-utility-catalog.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('CITYSIM_UTILITY_CATALOG_READY')
