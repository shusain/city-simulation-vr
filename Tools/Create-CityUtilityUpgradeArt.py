"""Original utility extensions with closed damaged shells; preserve starter exports."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from Assets.Manifests.CityAssetCapabilities import has_capability
import bpy, bmesh, json, math
root=Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from CityArtKit import (
    MATERIALS, box, cylinder, export_one, initialize_materials,
)
initialize_materials()
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC'; bpy.context.scene.unit_settings.scale_length=1
manifest_path=root/'ArtSource/Manifests/city_starter_kit.json'
manifest=json.loads(manifest_path.read_text(encoding='utf-8'))
entries=[]; report=[]
for service in ('Power','Service'):
    base=next(e for e in manifest['assets'] if e['asset_id']==f'building.{service.lower()}.a')
    for stage,suffix in ((1,'Medium'),(2,'High')):
        adopted=[e for e in manifest['assets'] if e['asset_id'].removesuffix('.damaged')==f'building.{service.lower()}.{suffix.lower()}.a' and has_capability(e, 'building_refinement')]
        if adopted:
            assert len(adopted)==2
            entries+=adopted
            report.append(dict(name=Path(adopted[0]['export']).stem,triangles=adopted[0]['triangles'],slots=2,accepted_source_preserved=True))
            continue
        before=set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(root/base['export']))
        obj=next(o for o in set(bpy.data.objects)-before if o.type=='MESH')
        bpy.context.view_layer.objects.active=obj
        bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
        name=f'SM_Building_{service}_{suffix}_A'
        parts=[obj]
        # Equipment occupies existing roofs, leaving the entrance and footprint intact.
        if service=='Power':
            for x in (-4,4):
                parts += [box(name+'_Generator',(3,4,2.4),(x,0,10.8),'Power'),box(name+'_Cap',(3.3,4.3,.25),(x,0,12.13),'Trim')]
                if stage==2:
                    parts += [cylinder(name+'_Vent',.7,3,(x,0,13.7),'Trim'),box(name+'_Conduit',(.3,6,.3),(x,0,8.1),'Trim')]
        else:
            for x in (-4,4):
                parts += [box(name+'_FilterSupport',(.6,.6,5),(x,0,6.5),'Trim'),cylinder(name+'_Filter',1.5,3,(x,0,10.5),'Service'),cylinder(name+'_Lid',1.65,.25,(x,0,12.13),'Trim')]
                if stage==2:
                    parts += [cylinder(name+'_UpperFilter',1.2,3,(x,0,13.75),'Service'),box(name+'_Pipe',(.4,5,.4),(x,0,12.4),'Trim')]
        bpy.ops.object.select_all(action='DESELECT')
        for o in parts:o.select_set(True)
        bpy.context.view_layer.objects.active=obj; bpy.ops.object.join()
        obj.name=name;obj.data.name=name
        # Collapse same semantic imported/new slots; retain vertex attributes.
        old=list(obj.data.materials)
        indices=[0 if old[f.material_index].name.split('.')[0]==service else 1 for f in obj.data.polygons]
        obj.data.materials.clear();obj.data.materials.append(MATERIALS[service]);obj.data.materials.append(MATERIALS['Trim'])
        for f,i in zip(obj.data.polygons,indices):f.material_index=i
        export_one(obj);obj.data.calc_loop_triangles()
        count=len(obj.data.loop_triangles)
        if count>1500:raise RuntimeError('Utility mesh exceeds budget')
        e=dict(base);e.update(asset_id=f'building.{service.lower()}.{suffix.lower()}.a',source='ArtSource/Blender/CityUtilityUpgradeKit.blend',export=f'ArtSource/Exports/{name}.fbx',mesh_destination=f'/Game/CitySim/Art/Meshes/{name}',dimensions_m=[round(v,4) for v in obj.dimensions],utility_upgrade=True,provenance='Original starter plus Blender extension primitives; Create-CityUtilityUpgradeArt.py')
        entries.append(e);report.append(dict(name=name,triangles=count,slots=2))
        damaged=obj.copy();damaged.data=obj.data.copy();bpy.context.collection.objects.link(damaged)
        damaged.name=name+'_Damaged'
        height=max(v.co.z for v in damaged.data.vertices)
        bm=bmesh.new();bm.from_mesh(damaged.data);before_boundary=sum(e.is_boundary for e in bm.edges);bm.free()
        for v in damaged.data.vertices:
            if v.co.z>height*.45:v.co.z*=.78+.08*math.sin(v.co.x*.55+v.co.y*.43)
        bm=bmesh.new();bm.from_mesh(damaged.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
        if sum(e.is_boundary for e in bm.edges)!=before_boundary or any(f.calc_area()<1e-8 for f in bm.faces):raise RuntimeError('Invalid damaged utility shell')
        bm.to_mesh(damaged.data);bm.free()
        export_one(damaged)
        de=dict(e);de.update(asset_id=e['asset_id']+'.damaged',export=f'ArtSource/Exports/{damaged.name}.fbx',mesh_destination=f'/Game/CitySim/Art/Meshes/{damaged.name}',damaged_variant=True,material_slots={service:'/Game/CitySim/Art/Materials/MI_CityCharred','Trim':'/Game/CitySim/Art/Materials/MI_CityCharredTrim'},dimensions_m=[round(v,4) for v in damaged.dimensions])
        entries.append(de)
        obj.location.x=len(entries)*25;damaged.location.x=obj.location.x+20
bpy.ops.wm.save_as_mainfile(filepath=str(root/'ArtSource/Blender/CityUtilityUpgradeKit.blend'))
manifest['assets']=[e for e in manifest['assets'] if not e.get('utility_upgrade')]+entries
manifest_path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
(root/'Artifacts/AssetImport/D56-utility-geometry.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('CITYSIM_UTILITY_ART_READY',len(entries))
