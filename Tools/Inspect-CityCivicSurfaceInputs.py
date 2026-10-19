from pathlib import Path
import bpy,json,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityCivicBuildingSurfaces import select_surfaces
entries=[e for e in json.loads((ROOT/'ArtSource/Manifests/city_starter_kit.json').read_text())['assets'] if e['asset_id'].startswith('building.') and '.damaged' not in e['asset_id'] and not e['asset_id'].startswith(('building.residential.','building.shop.','building.factory.'))]
entries += [dict(e,asset_id=e['name']) for e in json.loads((ROOT/'ArtSource/Manifests/city_seasonal_kit.json').read_text())['assets'] if e['name']=='SM_Building_CityPark_December_A']
result=[]
for e in entries:
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False);bpy.ops.import_scene.fbx(filepath=str(ROOT/e['export']))
    obj=next(o for o in bpy.context.selected_objects if o.type=='MESH')
    walls,roofs,parts=select_surfaces(obj,e['asset_id'])
    result.append(dict(asset_id=e['asset_id'],wall_faces=len(walls),roof_faces=len(roofs),enclosures=parts,export=e['export']))
out=ROOT/'Artifacts/D91/CivicSurfaces/SurfaceInventory.json';out.write_text(json.dumps(dict(task='D91',assets=result),indent=2)+'\n')
print('CITYSIM_D91_CIVIC_INVENTORY_PASS',len(result),'examined;',sum(bool(e['wall_faces'] or e['roof_faces']) for e in result),'selected')
