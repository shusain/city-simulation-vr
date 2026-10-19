"""Read-only inventory of actual exported building panes for D91."""
from pathlib import Path
import bpy,json,sys
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D91/Buildings';OUT.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(ROOT/'Tools'))
from CityBuildingWindows import select_panes
entries=[dict(e,manifest='city_starter_kit.json') for e in json.loads((ROOT/'ArtSource/Manifests/city_starter_kit.json').read_text())['assets'] if e['asset_id'].startswith('building.') and '.damaged' not in e['asset_id']]
entries += [dict(e,asset_id=e['name'],manifest='city_seasonal_kit.json') for e in json.loads((ROOT/'ArtSource/Manifests/city_seasonal_kit.json').read_text())['assets'] if e['name']=='SM_Building_Residential_SeasonalSpace_A']
result=[]
for e in entries:
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=str(ROOT/e['export']))
    obj=next(o for o in bpy.context.selected_objects if o.type=='MESH');mesh=obj.data
    attr=mesh.color_attributes.get('WindowMask')
    selected,window_faces=select_panes(obj,e['asset_id']);panes=[mesh.polygons[i] for i in selected]
    examples=[]
    for p in panes:
        coords=[mesh.vertices[i].co for i in p.vertices]
        examples.append(dict(index=p.index,vertices=len(p.vertices),material=mesh.materials[p.material_index].name.split('.')[0],
            coords=[list(c) for c in coords],normal=list(p.normal)))
    result.append(dict(asset_id=e['asset_id'],export=e['export'],manifest=e['manifest'],mesh_destination=e['mesh_destination'],dimensions_m=list(obj.dimensions),window_count=len(panes),windows=examples))
(OUT/'WindowInventory.json').write_text(json.dumps(dict(task='D91',assets=result),indent=2)+'\n')
print('CITYSIM_D91_WINDOW_INVENTORY',len(result),'assets;',sum(e['window_count'] for e in result),'panes')
