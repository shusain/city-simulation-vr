"""Append AI facilities only; preserve current art, densities and foundation records."""
import unreal, json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
catalog=unreal.load_asset('/Game/CitySim/Data/DA_CityBuildingCatalog')
if not catalog: raise RuntimeError('Current catalog missing')
rows=[('AIInstitute','AI institute','College',4500,7,6,3,40,10,8,'ResearchCampus'),
      ('EnterpriseAICenter','Enterprise AI center','SecondarySchool',3000,4,6,3,20,6,5,'AIInstitute')]
original=list(catalog.get_editor_property('definitions'))
definitions=[d for d in original if str(d.definition_id) not in [v[0] for v in rows]]
report=[]
for name,label,source,cost,upkeep,jobs,education,power,water,updates,prerequisite in rows:
 template=next(d for d in original if str(d.definition_id)==source)
 d=unreal.CityBuildingDefinition()
 values=dict(definition_id=name,display_name=label,footprint=template.footprint,
             cost=cost,logical_height_cm=template.logical_height_cm,presentation_mesh=template.presentation_mesh,
             construction_mesh=template.construction_mesh,entrance_sides=template.entrance_sides,
             presentation_yaw_offset_quarter_turns=template.presentation_yaw_offset_quarter_turns,
             construction_steps=updates,jobs_provided=jobs,minimum_staff_education=education,
             required_operating_facilities=[prerequisite],requires_alternative_power=True,
             monthly_upkeep=upkeep,power_demand=power,water_demand=water)
 for k,v in values.items(): d.set_editor_property(k,v)
 if not unreal.load_asset(d.presentation_mesh.get_path_name()): raise RuntimeError('Missing established mesh '+name)
 definitions.append(d)
 report.append(dict(id=name,mesh=d.presentation_mesh.get_path_name(),source=source,cost=cost,upkeep=upkeep,
                    jobs=jobs,education=education,power=power,water=water,construction_updates=updates,
                    prerequisite=prerequisite,footprint=[d.footprint.width,d.footprint.height]))
catalog.set_editor_property('definitions',definitions)
if not unreal.EditorAssetLibrary.save_loaded_asset(catalog): raise RuntimeError('Catalog save failed')
output=root/'Artifacts/D90/AI/Catalog.json'; output.parent.mkdir(parents=True,exist_ok=True)
output.write_text(json.dumps(report,indent=2))
unreal.log('CITYSIM_AI_CATALOG_READY')
