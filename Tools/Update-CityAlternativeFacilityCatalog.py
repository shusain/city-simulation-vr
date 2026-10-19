"""Append stable alternative IDs without changing legacy facilities or catalog order."""
import unreal,json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
catalog=unreal.load_asset('/Game/CitySim/Data/DA_CityBuildingCatalog')
# ID, label, size, cost, supply, upkeep, jobs, education, minimum output, pollution, build days.
rows=[('WaterTower','Water tower',2,150,100,10,1,0,100,0,1),
 ('CoalPower','Coal power plant',4,700,400,60,4,0,50,35,3),
 ('OilPower','Oil power plant',4,1000,600,90,4,1,0,20,4),
 ('WindPower','Wind turbine',3,600,150,15,2,1,0,0,2),
 ('SolarPower','Solar farm',4,1200,300,25,3,2,0,0,3),
 ('NuclearPower','Nuclear power plant',4,6000,2000,220,8,3,0,0,10),
 ('College','Higher education',3,3000,0,90,6,1,0,0,5),
 ('PrimarySchool','Primary education',2,600,0,20,4,1,0,0,3),
 ('SecondarySchool','Secondary education',3,1200,0,35,4,1,0,0,4)]
definitions=[d for d in catalog.get_editor_property('definitions') if str(d.get_editor_property('definition_id')) not in [r[0] for r in rows]]
report=[]
for name,label,size,cost,supply,upkeep,jobs,education,floor,pollution,days in rows:
 d=unreal.CityBuildingDefinition();mesh=unreal.load_asset(f'/Game/CitySim/Art/Meshes/SM_Building_{name}_A')
 if not mesh:raise RuntimeError(f'Missing mesh {name}')
 footprint=unreal.CityCellFootprint();footprint.set_editor_property('width',size);footprint.set_editor_property('height',size)
 values=dict(definition_id=name,display_name=label,footprint=footprint,cost=cost,presentation_mesh=mesh,logical_height_cm=max(300.0,float(mesh.get_bounding_box().max.z)+25.0),construction_steps=days,jobs_provided=jobs,monthly_upkeep=upkeep,utility_capacity=supply,utility_minimum_output_percent=floor,minimum_staff_education=education,pollution_emission=pollution,entrance_sides=1,presentation_yaw_offset_quarter_turns=2,power_demand=15 if name in ('College','PrimarySchool','SecondarySchool') else 0,water_demand=10 if name in ('College','PrimarySchool','SecondarySchool') else 0)
 for k,v in values.items():d.set_editor_property(k,v)
 if name in ('WaterTower','CoalPower','OilPower','WindPower','SolarPower','NuclearPower'):
  construction=unreal.load_asset('/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A');assert construction
  d.set_editor_property('construction_mesh',construction)
 definitions.append(d);report.append(dict(id=name,cost=cost,supply=supply,upkeep=upkeep,jobs=jobs,education=education,floor=floor,pollution=pollution,days=days))
catalog.set_editor_property('definitions',definitions)
if not unreal.EditorAssetLibrary.save_loaded_asset(catalog):raise RuntimeError('Catalog save failed')
(root/'Artifacts/AssetImport/D56-alternative-catalog.json').write_text(json.dumps(report,indent=2))
unreal.log('CITYSIM_ALTERNATIVE_CATALOG_READY')
