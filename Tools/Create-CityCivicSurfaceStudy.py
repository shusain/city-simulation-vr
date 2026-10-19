"""D91 civic/utility surface candidates layered onto approved window source."""
from pathlib import Path
import bpy,json,sys,math
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityCivicBuildingSurfaces import select_surfaces,make_surface,apply_surfaces
from CityBuildingStudyIO import Materials,load_object,export_checked,snapshot,digest
OUT=ROOT/'Artifacts/D91/CivicSurfaces';PREVIEW=OUT/'Blender';EXPORT=ROOT/'ArtSource/Exports/CivicSurfaceStudy';SOURCE=ROOT/'ArtSource/Blender/CityCivicSurfaces.blend'
for path in (OUT,PREVIEW,EXPORT):path.mkdir(parents=True,exist_ok=True)
window_doc=json.loads((ROOT/'ArtSource/Manifests/city_building_window_study.json').read_text());approved={e['asset_id']:e for e in window_doc['assets']}
assert digest(ROOT/window_doc['source'])==window_doc['source_sha256']
textures=json.loads((OUT/'TextureInputs.json').read_text())['textures']
starter=json.loads((ROOT/'ArtSource/Manifests/city_starter_kit.json').read_text())
entries=[e.get('building_adoption_prior_entry',e) for e in starter['assets'] if e['asset_id'].startswith('building.') and '.damaged' not in e['asset_id'] and not e['asset_id'].startswith(('building.residential.','building.shop.','building.factory.'))]
protected={ROOT/p:h for p,h in window_doc['protected'].items()}
for path in (ROOT/window_doc['source'],ROOT/'ArtSource/Manifests/city_building_window_study.json',OUT/'TextureInputs.json'):protected[path]=digest(path)
for e in approved.values():protected[ROOT/e['export']]=e['sha256']
for info in textures.values():
    for e in info['maps'].values():assert digest(ROOT/e['file'])==e['sha256'];protected[ROOT/e['file']]=e['sha256']
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
materials=Materials();brick_cache={}
info=textures['CivicAsphalt'];asphalt=make_surface(ROOT,'CivicAsphaltRoof',(.13,.15,.17),'CivicAsphalt',info['folder'],info['normalization'],info['strength'])
records=[];objects=[];skipped=[]
for entry in entries:
    prior=approved.get(entry['asset_id']);name=entry['mesh_destination'].rsplit('/',1)[1]
    if prior:
        before=load_object(ROOT/window_doc['source'],name);mapping=dict(prior['material_slots']);input_export=ROOT/prior['export'];input_source=ROOT/window_doc['source']
    else:
        bpy.ops.object.select_all(action='DESELECT');bpy.ops.import_scene.fbx(filepath=str(ROOT/entry['export']));before=next(o for o in bpy.context.selected_objects if o.type=='MESH')
        bpy.context.view_layer.objects.active=before;bpy.ops.object.transform_apply(location=False,rotation=True,scale=True);materials.restore(before,ROOT/entry['source']);mapping=dict(entry['material_slots']);input_export=ROOT/entry['export'];input_source=ROOT/entry['source']
    before.name=name+'_Before';walls,roofs,enclosures=select_surfaces(before,entry['asset_id'])
    if not walls and not roofs:
        skipped.append(dict(asset_id=entry['asset_id'],reason='No applicable enclosure/flat roof; tanks, wind/solar equipment and open park surfaces keep current finish'))
        bpy.data.objects.remove(before,do_unlink=True);continue
    assert roofs,entry['asset_id']
    after=before.copy();after.data=before.data.copy();scene.collection.objects.link(after);after.name=name
    original=snapshot(before);slot_names=[m.name.split('.')[0] for m in before.data.materials];wall_materials={};tints={}
    for old in sorted(set(walls.values())):
        material=before.data.materials[old];p=material.node_tree.nodes.get('Principled BSDF');assert p and not p.inputs['Base Color'].is_linked,(entry['asset_id'],material.name)
        tint=tuple(p.inputs['Base Color'].default_value[:3]);key=(slot_names[old],tuple(round(c,6) for c in tint))
        if key not in brick_cache:
            info=textures['CityBrick'];label='CivicBrick_'+slot_names[old]
            if label in bpy.data.materials:label+='_'+str(len(brick_cache)+1)
            brick_cache[key]=make_surface(ROOT,label,tint,'CityBrick',info['folder'],info['normalization'],info['strength'])
        wall_materials[old]=brick_cache[key];tints[str(old)]=dict(original_material=slot_names[old],color=list(tint),new_material=brick_cache[key].name)
    wall_slots,roof_slot=apply_surfaces(after,walls,roofs,wall_materials,asphalt)
    current=snapshot(after);assert original['vertices']==current['vertices'] and original['faces']==current['faces'] and original['rgba']==current['rgba']
    for p in before.data.polygons:
        if p.index in set(walls)|set(roofs):continue
        assert original['slots'][p.index]==current['slots'][p.index]
        for li in p.loop_indices:
            if original['uv']:assert original['uv'][li]==current['uv'][li]
    slots=slot_names+[m.name for m in wall_materials.values()]+['CivicAsphaltRoof']
    for m in wall_materials.values():mapping[m.name]='/Game/CitySim/Art/Materials/MI_'+m.name
    mapping['CivicAsphaltRoof']='/Game/CitySim/Art/Materials/MI_CivicAsphaltRoof'
    path=EXPORT/(name+'.fbx');record=dict(task='D91',asset_id=entry['asset_id'],source=SOURCE.relative_to(ROOT).as_posix(),source_entry=entry,approved_window_entry=prior,input_source=input_source.relative_to(ROOT).as_posix(),input_export=input_export.relative_to(ROOT).as_posix(),input_sha256=digest(input_export),mesh_destination=entry['mesh_destination'],material_slots=mapping,wall_faces=sorted(walls),roof_faces=roofs,enclosures=enclosures,tints=tints,wall_slots={str(k):v for k,v in wall_slots.items()},roof_slot=roof_slot,window_count=prior['window_count'] if prior else 0,geometry_rgba_preserved=True,unselected_uv_material_preserved=True,export=path.relative_to(ROOT).as_posix())
    record.update(export_checked(after,path,slots));records.append(record);objects.append((before,after,record))
assert len(records)==26

citizen=load_object(ROOT/'ArtSource/Blender/CityResidentialScale.blend','ScaleReference_Street');citizen.scale=(1.15,)*3;citizen.rotation_euler.z=math.pi/2
ground_material=bpy.data.materials.new('CivicReviewGround');ground_material.use_nodes=True;p=ground_material.node_tree.nodes['Principled BSDF'];p.inputs['Base Color'].default_value=(.16,.18,.22,1);p.inputs['Roughness'].default_value=.85
bpy.ops.mesh.primitive_plane_add(size=400,location=(0,0,-.22));ground=bpy.context.object;ground.name='CivicReviewGround';ground.data.materials.append(ground_material)
camera_data=bpy.data.cameras.new('CivicSurfaceReviewCamera');camera_data.type='ORTHO';camera=bpy.data.objects.new('CivicSurfaceReviewCamera',camera_data);scene.collection.objects.link(camera);scene.camera=camera
sun_data=bpy.data.lights.new('CivicReviewSun','SUN');sun_data.energy=3;sun_data.angle=.15;sun=bpy.data.objects.new('CivicReviewSun',sun_data);scene.collection.objects.link(sun);sun.rotation_euler=(.5,-.45,-.6)
scene.world.use_nodes=True;background=scene.world.node_tree.nodes['Background'];background.inputs['Color'].default_value=(.28,.34,.44,1);background.inputs['Strength'].default_value=.65
scene.render.engine='CYCLES';scene.cycles.samples=16;scene.cycles.use_denoising=True;scene.render.resolution_x=1200;scene.render.resolution_y=900;scene.render.resolution_percentage=100;scene.view_settings.view_transform='AgX'
compare={'building.police.high.a','building.fire.high.a','building.school.a','building.hospital.medium.a','building.recycling.medium.a','building.power.high.a','building.service.high.a','building.coalpower.a'}
night_ids={'building.police.high.a','building.hospital.medium.a','building.school.a'}
detail_ids={'building.police.high.a','building.power.high.a'}
views=[]
if '--skip-renders' not in sys.argv:
    for before,after,record in objects:
        low=Vector(tuple(min(v.co[i] for v in before.data.vertices) for i in range(3)));high=Vector(tuple(max(v.co[i] for v in before.data.vertices) for i in range(3)));size=high-low
        modes=[('After','Day','Whole')]+([('Before','Day','Whole')] if record['asset_id'] in compare else [])+([('After','Night','Whole')] if record['asset_id'] in night_ids else [])+([('After','Day','Detail')] if record['asset_id'] in detail_ids else [])
        for state,light,framing in modes:
            obj=after if state=='After' else before;night=light=='Night'
            for o in scene.objects:
                if o.type=='MESH':o.hide_render=o not in (obj,ground,citizen)
            for m in bpy.data.materials:
                if m.use_nodes:
                    for n in m.node_tree.nodes:
                        if n.name=='NightGlow':n.outputs[0].default_value=1 if night else 0
            sun_data.energy=.08 if night else 3;background.inputs['Strength'].default_value=.12 if night else .65
            aim=Vector(((low.x+high.x)/2,(low.y+high.y)/2,size.z*.45));offset=Vector((30,-44,23));width=max(size.x*1.65,size.y*1.65,size.z*1.55,19)
            if framing=='Detail':
                face=max((obj.data.polygons[i] for i in record['roof_faces']),key=lambda p:p.area);points=[obj.data.vertices[i].co for i in face.vertices]
                aim=Vector((max(c.x for c in points)-.6,min(c.y for c in points)+.6,points[0].z-.7));offset=Vector((8,-10,8));width=12
            camera.location=aim+offset;camera.rotation_euler=(aim-camera.location).to_track_quat('-Z','Y').to_euler();camera_data.ortho_scale=width
            citizen.location=(low.x+size.x*.32,low.y-.65,-.21-.125*1.15)
            path=PREVIEW/(record['mesh_destination'].rsplit('/',1)[1]+'-'+state+'-'+light+'-'+framing+'.png');scene.render.filepath=str(path)
            reuse=('--reuse-renders' in sys.argv or ('--reuse-before' in sys.argv and state=='Before')) and path.is_file()
            if not reuse:bpy.ops.render.render(write_still=True)
            views.append(dict(asset_id=record['asset_id'],state=state,lighting=light,framing=framing,file=path.relative_to(ROOT).as_posix(),camera=list(camera.location),ortho_width_m=width,citizen_height_m=1.43175,citizen_feet_z_m=-.21))
for m in bpy.data.materials:
    if m.use_nodes:
        for n in m.node_tree.nodes:
            if n.name=='NightGlow':n.outputs[0].default_value=0
for i,(before,after,record) in enumerate(objects):before.hide_set(True);before.hide_render=True;after.hide_set(False);after.hide_render=False;after.location=(i%6*42,i//6*48,0)
citizen.hide_set(True);citizen.hide_render=True;sun_data.energy=3;background.inputs['Strength'].default_value=.65
aim=Vector((105,100,5));camera.location=aim+Vector((200,-280,200));camera.rotation_euler=(aim-camera.location).to_track_quat('-Z','Y').to_euler();camera_data.ortho_scale=350
bpy.data.orphans_purge(do_recursive=True);bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE))
assert all(digest(p)==h for p,h in protected.items()),'Accepted or production input changed'
doc=dict(task='D91',slice='CivicAsphaltAndBrick',blender_review_only=True,production_imported=False,source=SOURCE.relative_to(ROOT).as_posix(),source_sha256=digest(SOURCE),approved_window_manifest='ArtSource/Manifests/city_building_window_study.json',assets=records,skipped=skipped,views=views,textures=textures,protected={p.relative_to(ROOT).as_posix():h for p,h in protected.items()},brick_repeat_m=2.4,asphalt_repeat_m=2,notes='User accepts all windows/day-night/damaged scaled home; add aggregate asphalt flat roofs and brick siding while retaining wall colors.26 civic/utility/pavilion candidates; accepted windows and all geometry remain unchanged. Review in Blender before combined Unreal adoption.')
(ROOT/'ArtSource/Manifests/city_civic_surface_study.json').write_text(json.dumps(doc,indent=2)+'\n')
print('CITYSIM_D91_CIVIC_SURFACES_BLENDER_PASS',len(records),'exports/FBX roundtrips;',len(views),'renders;approved windows and geometry preserved')
