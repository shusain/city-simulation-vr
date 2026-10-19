"""Refresh the eight sign offsets in Unreal's top-left UV convention."""
from pathlib import Path
import json,unreal
ROOT=Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.project_dir()).resolve()==(ROOT/'CitySimVR').resolve()
rows=json.loads((ROOT/'ArtSource/Textures/RoofsSigns/roofs_signs.json').read_text())['signs'];report=[]
lib=unreal.MaterialEditingLibrary
for row in rows:
    path='/Game/CitySim/Art/LowDensity/Materials/MI_Sign_'+row['id'];mi=unreal.load_asset(path);assert mi
    index=row['atlas_index'];value=unreal.LinearColor((index%4)*.25,(index//4)*.5,0,0)
    lib.set_material_instance_vector_parameter_value(mi,'AtlasOffset',value);assert unreal.EditorAssetLibrary.save_loaded_asset(mi)
    actual=lib.get_material_instance_vector_parameter_value(mi,'AtlasOffset');assert actual.r==value.r and actual.g==value.g
    report.append(dict(business=row['id'],material=path,offset=[actual.r,actual.g]))
(ROOT/'Artifacts/LowDensity/Integration/Signs.json').write_text(json.dumps(report,indent=2)+'\n')
unreal.log('CITYSIM_D104_SIGN_OFFSETS_PASS eight business signs')
