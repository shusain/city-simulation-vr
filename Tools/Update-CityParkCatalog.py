"""Append larger parks without replacing the existing city catalog or map."""
from pathlib import Path
import json,unreal
root=Path(__file__).resolve().parents[1]
catalog=unreal.load_asset('/Game/CitySim/Data/DA_CityBuildingCatalog')
if not catalog:raise RuntimeError('City catalog missing')
definitions=list(catalog.get_editor_property('definitions'))
for id,label,cells,cost,upkeep,radius,bonus,demand,steps in [('NeighborhoodPark','Neighborhood Park',5,800,30,12,25,4,20),('CityPark','City Park',7,1600,50,18,30,8,30),('BasketballPark','Basketball Park',5,800,30,12,25,4,20),('SkatePark','Skate Park',5,800,30,12,25,4,20),('BaseballPark','Baseball Park',7,1600,50,18,30,8,30),('SoccerPark','Soccer Park',7,1600,50,18,30,8,30)]:
    index=next((i for i,d in enumerate(definitions) if str(d.get_editor_property('definition_id'))==id),None)
    d=definitions[index] if index is not None else unreal.CityBuildingDefinition()
    d.set_editor_property('definition_id',id);d.set_editor_property('display_name',label)
    footprint=unreal.CityCellFootprint();footprint.set_editor_property('width',cells);footprint.set_editor_property('height',cells);d.set_editor_property('footprint',footprint)
    for key,value in {'cost':cost,'monthly_upkeep':upkeep,'logical_height_cm':822 if id in ('NeighborhoodPark','CityPark') else 600,'park_radius_cells':radius,'park_desirability_bonus':bonus,'power_demand':demand,'water_demand':demand,'construction_steps':steps,'entrance_sides':15,'presentation_yaw_offset_quarter_turns':2}.items():d.set_editor_property(key,value)
    mesh=unreal.load_asset('/Game/CitySim/Art/Meshes/SM_Building_'+id+'_A')
    if not mesh:raise RuntimeError('Park mesh missing: '+id)
    d.set_editor_property('presentation_mesh',mesh)
    d.set_editor_property('construction_mesh',unreal.load_asset('/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A'))
    if index is None:definitions.append(d)
    else:definitions[index]=d
catalog.set_editor_property('definitions',definitions)
if not unreal.EditorAssetLibrary.save_loaded_asset(catalog):raise RuntimeError('Park catalog save failed')
(root/'Artifacts/Diagnostics/D47-park-catalog-ready.txt').write_text('ready',encoding='utf-8')
unreal.log('CITYSIM_PARK_CATALOG_SAVED: existing IDs preserved; neighborhood5x5/city7x7')
