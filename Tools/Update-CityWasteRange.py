"""D127: change only both waste facility levels' road coverage, retaining all other assets/policies."""
from pathlib import Path
import json,unreal
root=Path(__file__).resolve().parents[1]
assert '**D127 waste range / B008 hospital service investigation — native/source/build window CLAIMED' in (root/'docs/DEVELOPMENT_TASKS.md').read_text(encoding='utf-8-sig')
catalog=unreal.load_asset('/Game/CitySim/Data/DA_CityBuildingCatalog');assert catalog
definitions=list(catalog.get_editor_property('definitions'));changes=[]
for index,definition in enumerate(definitions):
 identity=str(definition.get_editor_property('definition_id'))
 if identity not in ['WasteDepot','Recycling']:continue
 levels=list(definition.get_editor_property('density_levels'))
 for i,level in enumerate(levels):
  old=level.get_editor_property('civic_road_range');assert old==32,(identity,old)
  level.set_editor_property('civic_road_range',64);levels[i]=level
  changes.append(dict(definition=identity,level=str(level.get_editor_property('level')),before=old,after=64))
 definition.set_editor_property('density_levels',levels);definitions[index]=definition
assert len(changes)==4
catalog.set_editor_property('definitions',definitions)
assert unreal.EditorAssetLibrary.save_loaded_asset(catalog)
(root/'Artifacts/D127/RangeImport.json').write_text(json.dumps(dict(task='D127',changes=changes,otherAssetsSaved=False),indent=2)+'\n')
unreal.log('CITYSIM_D127_WASTE_RANGE_PASS four levels32-to64; catalog only')
