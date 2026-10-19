"""D91 Blender-first candidates; never write production manifests/assets.

After appearance approval, these exact exported entries can be promoted and
imported through the existing asset pipeline. --skip-renders refreshes source.
"""
from pathlib import Path
import bpy,bmesh,json,hashlib,math,sys
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityBuildingWindows import select_panes,add_frames,base_name
from CityBuildingStudyIO import checked_fbx,window_faces,snapshot,digest
OUT=ROOT/'Artifacts/D91/Buildings';PREVIEW=OUT/'Blender';EXPORT=ROOT/'ArtSource/Exports/BuildingWindowStudy'
for path in (OUT,PREVIEW,EXPORT):path.mkdir(parents=True,exist_ok=True)
SOURCE=ROOT/'ArtSource/Blender/CityBuildingWindows.blend'
starter=json.loads((ROOT/'ArtSource/Manifests/city_starter_kit.json').read_text())
seasonal=json.loads((ROOT/'ArtSource/Manifests/city_seasonal_kit.json').read_text())
entries=[dict(e.get('building_adoption_prior_entry',e),manifest='city_starter_kit.json') for e in starter['assets'] if e['asset_id'].startswith('building.') and '.damaged' not in e['asset_id']]
entries += [dict(e.get('building_adoption_prior_entry',e),asset_id=e['name'],manifest='city_seasonal_kit.json') for e in seasonal['assets'] if e['name']=='SM_Building_Residential_SeasonalSpace_A']
protected={ROOT/'ArtSource/Manifests/city_starter_kit.json',ROOT/'ArtSource/Manifests/city_seasonal_kit.json',ROOT/'ArtSource/Blender/CityResidentialScale.blend'}
for e in starter['assets']+seasonal['assets']:
    protected.update((ROOT/e['source'],ROOT/e['export']))
    asset=ROOT/'CitySimVR/Content'/e['mesh_destination'].removeprefix('/Game/');protected.add(asset.with_suffix('.uasset'))
protected={p:digest(p) for p in protected if p.is_file()}
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
material_cache={}
def load_material(path,name):
    key=(str(path),name)
    if key not in material_cache:
        with bpy.data.libraries.load(str(path),link=False) as (available,loaded):
            choices=[n for n in available.materials if n.split('.')[0]==name]
            assert choices,(path,name);loaded.materials=[name if name in choices else choices[0]]
        m=loaded.materials[0];m.use_nodes=True
        if len(m.node_tree.nodes)==2:
            p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=m.diffuse_color;p.inputs['Roughness'].default_value=.7
        material_cache[key]=m
    return material_cache[key]
def restore_materials(obj,entry):
    names=[base_name(m) for m in obj.data.materials]
    for i,name in enumerate(names):obj.data.materials[i]=load_material(ROOT/entry['source'],name)
    return names
window=load_material(ROOT/'ArtSource/Blender/CityResidentialScale.blend','HomeWindowOpaque').copy();window.name='BuildingWindowOpaque'
def load_home():
    with bpy.data.libraries.load(str(ROOT/'ArtSource/Blender/CityResidentialScale.blend'),link=False) as (available,loaded):loaded.objects=['SM_HomeScale_Refined']
    obj=loaded.objects[0];scene.collection.objects.link(obj);obj.hide_set(False);obj.hide_render=False;obj.location=(0,0,0)
    return obj
def export_checked(obj,name,slots,expected_windows):
    obj.location=(0,0,0);bpy.context.view_layer.update()
    path=EXPORT/(name+'.fbx')
    with checked_fbx(obj,path,slots) as check:
        windows=window_faces(check,('BuildingWindowOpaque','HomeWindowOpaque'))
        assert windows==expected_windows,(name,windows,expected_windows)
    return dict(export=path.relative_to(ROOT).as_posix(),sha256=digest(path),triangles=len(obj.data.loop_triangles),sections=len(slots),dimensions_m=list(obj.dimensions),bounds_min_m=[min(v.co[i] for v in obj.data.vertices) for i in range(3)],bounds_max_m=[max(v.co[i] for v in obj.data.vertices) for i in range(3)],roundtrip_pass=True)

records=[];objects=[];skipped=[]
for entry in entries:
    bpy.ops.object.select_all(action='DESELECT');bpy.ops.import_scene.fbx(filepath=str(ROOT/entry['export']))
    before=next(o for o in bpy.context.selected_objects if o.type=='MESH');before.name=entry['mesh_destination'].rsplit('/',1)[1]+'_Before'
    bpy.context.view_layer.objects.active=before;bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    panes,box_faces=select_panes(before,entry['asset_id'])
    if not panes:
        skipped.append(dict(asset_id=entry['asset_id'],reason='No authored architectural window panes',export=entry['export']))
        bpy.data.objects.remove(before,do_unlink=True);continue
    names=restore_materials(before,entry);original=snapshot(before)
    home=entry['asset_id'] in ('building.residential.a','SM_Building_Residential_SeasonalSpace_A')
    if home:
        after=load_home();slots=['ResidentialBrick','HomeSolidDetails','RoofShingles','HomeWindowOpaque'];specs=[]
        mapping={name:('/Game/CitySim/Art/Materials/M_'+name if name in ('HomeSolidDetails','HomeWindowOpaque') else '/Game/CitySim/Art/Materials/MI_'+name) for name in slots}
    else:
        after=before.copy();after.data=before.data.copy();scene.collection.objects.link(after)
        specs=add_frames(after,panes,box_faces,window);slots=names+['BuildingWindowOpaque'];mapping=dict(entry['material_slots'],BuildingWindowOpaque='/Game/CitySim/Art/Materials/M_BuildingWindowOpaque')
        current=snapshot(after);assert current['vertices'][:len(original['vertices'])]==original['vertices']
        assert current['faces'][:len(original['faces'])]==original['faces']
        for p in before.data.polygons:
            if p.index in set(panes)|box_faces:continue
            assert current['slots'][p.index]==original['slots'][p.index]
            for li in p.loop_indices:
                if original['uv']:assert current['uv'][li]==original['uv'][li]
                if original['rgba']:assert current['rgba'][li]==original['rgba'][li]
        assert len(after.data.polygons)==len(before.data.polygons)+16*len(panes)
    name=entry['mesh_destination'].rsplit('/',1)[1];after.name=name
    record=dict(task='D91',asset_id=entry['asset_id'],manifest=entry['manifest'],source=SOURCE.relative_to(ROOT).as_posix(),source_entry=entry,source_export_sha256=digest(ROOT/entry['export']),mesh_destination=entry['mesh_destination'],material_slots=mapping,window_count=len(panes),pane_faces=panes,window_box_faces=sorted(box_faces),frame_specs=specs,home_scale_adopted=home,nonwindow_source_preserved=not home)
    record.update(export_checked(after,name,slots,len(panes)));records.append(record);objects.append((before,after,record))
assert len(records)==50 and sum(e['window_count'] for e in records)==1641
# Only the low home needs damage scale alignment. Regenerate its closed,
# non-emissive charred derivative from the now accepted home geometry.
after=load_home();after.name='SM_Building_Residential_A_Damaged'
normal=after.data;old_slots=[base_name(m) for m in normal.materials]
char_names=['CharredBrick','CharredTrim','CharredRoofShingles']
indices=[0 if old_slots[p.material_index]=='ResidentialBrick' else 2 if old_slots[p.material_index]=='RoofShingles' else 1 for p in normal.polygons]
normal.materials.clear()
for name in char_names:normal.materials.append(load_material(ROOT/'ArtSource/Blender/CityBuildingSurfacePrototype.blend',name))
for p,i in zip(normal.polygons,indices):p.material_index=i
for value in normal.color_attributes['WindowMask'].data:value.color=(1,1,1,1)
height=max(v.co.z for v in normal.vertices)
for v in normal.vertices:
    if v.co.z>height*.45:v.co.z*=.72+.14*math.sin(v.co.x*.55+v.co.y*.43)
bm=bmesh.new();bm.from_mesh(normal);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));assert all(f.calc_area()>1e-8 for f in bm.faces);bm.to_mesh(normal);bm.free();normal.update()
entry=next(e for e in starter['assets'] if e['asset_id']=='building.residential.a.damaged')
bpy.ops.import_scene.fbx(filepath=str(ROOT/entry['export']));before=next(o for o in bpy.context.selected_objects if o.type=='MESH');before.name=after.name+'_Before';restore_materials(before,entry)
record=dict(task='D91',asset_id=entry['asset_id'],manifest='city_starter_kit.json',source=SOURCE.relative_to(ROOT).as_posix(),source_entry=entry,source_export_sha256=digest(ROOT/entry['export']),mesh_destination=entry['mesh_destination'],material_slots={n:'/Game/CitySim/Art/Materials/MI_'+n for n in char_names},window_count=0,home_scale_adopted=True,damaged_scale_only=True,nonwindow_source_preserved=False)
record.update(export_checked(after,after.name,char_names,0));records.append(record);objects.append((before,after,record))

# Real street mesh at its accepted runtime scale provides a consistent ruler.
with bpy.data.libraries.load(str(ROOT/'ArtSource/Blender/CityResidentialScale.blend'),link=False) as (available,loaded):loaded.objects=['ScaleReference_Street']
citizen=loaded.objects[0];scene.collection.objects.link(citizen);citizen.hide_set(False);citizen.hide_render=False;citizen.scale=(1.15,)*3;citizen.rotation_euler.z=math.pi/2
ground_material=bpy.data.materials.new('WindowReviewGround');ground_material.use_nodes=True;p=ground_material.node_tree.nodes['Principled BSDF'];p.inputs['Base Color'].default_value=(.16,.18,.22,1);p.inputs['Roughness'].default_value=.85
bpy.ops.mesh.primitive_plane_add(size=500,location=(0,0,-.22));ground=bpy.context.object;ground.name='WindowReviewGround';ground.data.materials.append(ground_material)
camera_data=bpy.data.cameras.new('BuildingWindowReviewCamera');camera_data.type='ORTHO';camera=bpy.data.objects.new('BuildingWindowReviewCamera',camera_data);scene.collection.objects.link(camera);scene.camera=camera
sun_data=bpy.data.lights.new('WindowReviewSun','SUN');sun_data.energy=3;sun_data.angle=.15;sun=bpy.data.objects.new('WindowReviewSun',sun_data);scene.collection.objects.link(sun);sun.rotation_euler=(.5,-.45,-.6)
scene.world.use_nodes=True;background=scene.world.node_tree.nodes['Background'];background.inputs['Color'].default_value=(.28,.34,.44,1);background.inputs['Strength'].default_value=.65
scene.render.engine='CYCLES';scene.cycles.samples=16;scene.cycles.use_denoising=True;scene.render.resolution_x=1200;scene.render.resolution_y=900;scene.render.resolution_percentage=100;scene.view_settings.view_transform='AgX'
comparison_ids={'building.residential.a','building.residential.high.a','building.shop.medium.a','building.factory.high.a','building.police.high.a','building.school.a','building.hospital.medium.a','building.recycling.medium.a','building.residential.a.damaged'}
views=[]
if '--skip-renders' not in sys.argv:
    for before,after,record in objects:
        if '.store.' in record['asset_id'] or record['manifest']=='city_seasonal_kit.json':continue
        for state,light in [('After','Day')]+([('Before','Day'),('After','Night')] if record['asset_id'] in comparison_ids else []):
            obj=after if state=='After' else before;night=light=='Night'
            for o in scene.objects:
                if o.type=='MESH':o.hide_render=o not in (obj,ground,citizen)
            for m in bpy.data.materials:
                if m.use_nodes:
                    for n in m.node_tree.nodes:
                        if n.name=='NightGlow':n.outputs[0].default_value=1 if night else 0
            sun_data.energy=.08 if night else 3;background.inputs['Strength'].default_value=.12 if night else .65
            low=Vector(tuple(min(v.co[i] for v in obj.data.vertices) for i in range(3)));high=Vector(tuple(max(v.co[i] for v in obj.data.vertices) for i in range(3)));size=high-low
            aim=Vector(((low.x+high.x)/2,(low.y+high.y)/2,size.z*.45));camera.location=aim+Vector((30,-44,23));camera.rotation_euler=(aim-camera.location).to_track_quat('-Z','Y').to_euler();camera_data.ortho_scale=max(size.x*1.65,size.y*1.65,size.z*1.55,19)
            citizen.location=(low.x+size.x*.32,low.y-.65,low.z-.125*1.15)
            path=PREVIEW/(record['mesh_destination'].rsplit('/',1)[1]+'-'+state+'-'+light+'.png');scene.render.filepath=str(path)
            if not ('--reuse-renders' in sys.argv and path.is_file()):bpy.ops.render.render(write_still=True)
            views.append(dict(asset_id=record['asset_id'],state=state,lighting=light,file=path.relative_to(ROOT).as_posix(),camera=list(camera.location),ortho_width_m=camera_data.ortho_scale,citizen_height_m=1.43175))
for m in bpy.data.materials:
    if m.use_nodes:
        for n in m.node_tree.nodes:
            if n.name=='NightGlow':n.outputs[0].default_value=0
for i,(before,after,record) in enumerate(objects):
    before.hide_render=True;before.hide_set(True);after.hide_set(False);after.hide_render=False;after.location=(i%8*42,i//8*48,0)
citizen.hide_set(True);citizen.hide_render=True;sun_data.energy=3;background.inputs['Strength'].default_value=.65
aim=Vector((140,140,5));camera.location=aim+Vector((200,-280,200));camera.rotation_euler=(aim-camera.location).to_track_quat('-Z','Y').to_euler();camera_data.ortho_scale=440
# FBX round-trip imports leave unused material/image dependencies whose relative
# texture paths refer to the export folder. Remove only unreferenced Blender
# datablocks before packing the actual source materials and their textures.
bpy.data.orphans_purge(do_recursive=True)
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE))
assert all(digest(path)==expected for path,expected in protected.items()),'Production source/assets changed'
manifest=dict(task='D91',slice='AllBuildingWindows',blender_review_only=True,production_imported=False,source=SOURCE.relative_to(ROOT).as_posix(),source_sha256=digest(SOURCE),assets=records,skipped=skipped,views=views,protected={p.relative_to(ROOT).as_posix():h for p,h in protected.items()},window_count=1641,normal_candidates=50,damaged_candidates=1,notes='User accepts relative home scale; all intact authored architectural windows use opaque illustrated glass and pale framing. Beacons/cabs/garage doors remain unchanged. Only low-home damage regenerated for accepted scale; other damage assets preserved. Unreal/adoption awaits Blender appearance approval.')
(ROOT/'ArtSource/Manifests/city_building_window_study.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('CITYSIM_D91_BUILDING_WINDOWS_BLENDER_PASS',len(records),'exports/FBX roundtrips;',len(views),'renders;1641 panes')
