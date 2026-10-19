"""D83 guarded source handoff: native units, paired LODs and scene-baked clips.

Reads accepted blends; writes only a separate integration kit/export manifest.
"""
from pathlib import Path
import bpy,bmesh,json,math,hashlib
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0, str(ROOT/'Tools'))
from CityParkGeometry import (
    append, bounds, triangles as tri, refine, REVIEW_DETAIL, INTEGRATION_LOD_DETAIL,
)
OUT=ROOT/'ArtSource/Exports/ParkIntegration';OUT.mkdir(parents=True,exist_ok=True)
REPORT=ROOT/'ArtSource/Manifests/city_park_integration.json'
SOURCE=ROOT/'ArtSource/Blender/CityParkIntegration.blend'
starter=json.loads((ROOT/'ArtSource/Manifests/city_starter_kit.json').read_text(encoding='utf-8'))
seasonal=json.loads((ROOT/'ArtSource/Manifests/city_seasonal_kit.json').read_text(encoding='utf-8'))
accepted=json.loads((ROOT/'ArtSource/Manifests/city_park_refinement.json').read_text())
sports=json.loads((ROOT/'Artifacts/D83/sports-preview.json').read_text())
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
inputs={p.relative_to(ROOT).as_posix():digest(p) for p in (ROOT/'ArtSource/Blender').glob('*.blend') if p!=SOURCE}
inputs.update({p.relative_to(ROOT).as_posix():digest(p) for p in (ROOT/'ArtSource/Exports/ParkRefinement').glob('*.fbx')})
ids=[e['asset_id'] for e in accepted['assets']]
originals=[e for e in starter['assets'] if e['asset_id'].startswith('building.') and not e.get('damaged_variant')]
def signature(o):
    color=o.data.color_attributes.active_color;uv=o.data.uv_layers.active
    return sorted((tuple(round(c,4) for c in o.data.vertices[o.data.loops[i].vertex_index].co),
                   tuple(round(c,3) for c in color.data[i].color) if color else (),
                   tuple(round(c,4) for c in uv.data[i].uv) if uv else ()) for p in o.data.polygons for i in p.loop_indices)
def select(objects):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:o.hide_set(False);o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
def export(o,name):
    expected=signature(o);p=OUT/(name+'.fbx');select([o])
    old_names={m:m.name for m in bpy.data.materials}
    for i,m in enumerate(old_names):m.name='_D83ExportMaterial'+str(i)
    for m in o.data.materials:m.name=old_names[m].split('.')[0]
    bpy.ops.export_scene.fbx(filepath=str(p),use_selection=True,object_types={'MESH'},
        apply_unit_scale=True,global_scale=1,axis_forward='-Y',axis_up='Z',bake_anim=False,
        mesh_smooth_type='FACE',add_leaf_bones=False,path_mode='STRIP')
    before=set(bpy.data.objects);before_materials=set(bpy.data.materials);bpy.ops.import_scene.fbx(filepath=str(p))
    check=next(v for v in set(bpy.data.objects)-before if v.type=='MESH');select([check])
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    assert signature(check)==expected,('native roundtrip',name)
    for v in set(bpy.data.objects)-before:bpy.data.objects.remove(v,do_unlink=True)
    for m in set(bpy.data.materials)-before_materials:bpy.data.materials.remove(m)
    for i,m in enumerate(old_names):m.name='_D83RestoreMaterial'+str(i)
    for m,name_before in old_names.items():m.name=name_before
    scale=bpy.context.scene.unit_settings.scale_length
    return dict(export=p.relative_to(ROOT).as_posix(),sha256=digest(p),triangles=tri(o),bounds_m=[[c*scale for c in row] for row in bounds(o)],roundtrip_pass=True)

bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
statics=[];objects=[]
for entry in accepted['assets']:
    aid=entry['asset_id'];live=next(e for e in starter['assets'] if e['asset_id']==aid)
    live=live.get('park_refinement_source_entry',live)
    normal=append(ROOT/'ArtSource/Blender/CityParkRefinement.blend',entry['name']);normal.name=Path(live['mesh_destination']).name
    canonical=live.get('native_export_source',live['export'])
    # Canonical source object names may differ from normalized native filenames.
    old_name=Path(canonical).stem
    low=append(ROOT/live['source'],old_name);low.name=normal.name+'_LOD1'
    refine(low,aid,detail=INTEGRATION_LOD_DETAIL)
    for damage in (False,True):
        target=next(e for e in starter['assets'] if e['asset_id']==aid+('.damaged' if damage else ''))
        copies=[]
        for original in (normal,low):
            obj=original.copy();obj.data=original.data.copy();bpy.context.scene.collection.objects.link(obj)
            suffix='_Damaged' if damage else ''
            name=normal.name+suffix+('_LOD1' if original==low else '')
            obj.name=name
            if damage:
                height=max(v.co.z for v in obj.data.vertices)
                phase=next(i for i,e in enumerate(originals) if e['asset_id']==aid)
                for v in obj.data.vertices:
                    if v.co.z>height*.45:
                        v.co.z*=.78+.08*math.sin(v.co.x*.55+v.co.y*.43) if target.get('utility_upgrade') else .72+.14*math.sin(v.co.x*.55+v.co.y*.43+phase)
                old_indices=[p.material_index for p in obj.data.polygons]
                names=list(target['material_slots'])
                obj.data.materials.clear()
                for m in names:
                    material=bpy.data.materials.get(m) or bpy.data.materials.new(m);obj.data.materials.append(material)
                for p,i in zip(obj.data.polygons,old_indices):p.material_index=min(i,len(names)-1)
                for attr in obj.data.color_attributes:
                    for c in attr.data:c.color=(1,1,1,1)
                bm=bmesh.new();bm.from_mesh(obj.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()
            copies.append(obj)
        materials=dict(target['material_slots'])
        if not damage:
            materials={m.name.split('.')[0]:'/Game/CitySim/Art/Materials/MI_City'+m.name.split('.')[0] for m in normal.data.materials}
        record=dict(asset_id=target['asset_id'],mesh_destination=target['mesh_destination'],materials=materials,
                    source=SOURCE.relative_to(ROOT).as_posix(),lods=[export(o,o.name) for o in copies])
        statics.append(record);objects+=copies
    bpy.data.objects.remove(normal,do_unlink=True);bpy.data.objects.remove(low,do_unlink=True)
for entry in seasonal['assets']:
    entry=entry.get('park_refinement_source_entry',entry)
    if entry['name'] not in ('SM_Building_Park_December_A','SM_Building_NeighborhoodPark_December_A'):continue
    aid='building.park.a' if entry['name']=='SM_Building_Park_December_A' else 'building.park.neighborhood'
    copies=[]
    for i,detail in enumerate((REVIEW_DETAIL,INTEGRATION_LOD_DETAIL)):
        obj=append(ROOT/entry['source'],entry['name']);obj.name=entry['name']+('_LOD1' if i else '')
        refine(obj,aid,detail=detail);copies.append(obj)
    materials={m.name.split('.')[0]:'/Game/CitySim/Art/Materials/MI_City'+m.name.split('.')[0] for m in copies[0].data.materials}
    statics.append(dict(asset_id=entry['asset_id'],mesh_destination=entry['mesh_destination'],materials=materials,
                        source=SOURCE.relative_to(ROOT).as_posix(),lods=[export(o,o.name) for o in copies]))
    objects+=copies
bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE))

# Bake accepted actions on a private skeleton. Each FBX has one action, avoiding
# importer's take naming/selection ambiguity. Pose matrices include bat-grip IK.
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'ArtSource/Blender/CityParkAnimationPreview.blend'))
native_scene=bpy.data.scenes.new('D83_NativeExports');bpy.context.window.scene=native_scene
native_scene.unit_settings.system='METRIC';native_scene.unit_settings.scale_length=.01
native_scene.render.fps=24
clips=[];rigs=[];props=[]
records=list(sports['scenes'])
with bpy.data.libraries.load(str(ROOT/'ArtSource/Blender/CityCitizenRigPrototype.blend'),link=False) as (a,b):
    b.objects=['SK_Citizen_Rig_Casual']
template=b.objects[0]
for clip,period in (('Idle',48),('Walk',24)):
    scene=bpy.data.scenes.new('D83_'+clip);bpy.context.window.scene=scene;scene.render.fps=24
    hero=template.copy();hero.data=template.data.copy();scene.collection.objects.link(hero)
    rig=template.parent.copy();rig.data=template.parent.data.copy();scene.collection.objects.link(rig)
    hero.parent=rig;hero.matrix_parent_inverse.identity();hero.location=(0,0,0)
    hero.modifiers['D83HumanoidSkin'].object=rig;hero.name='D83_'+clip+'_Citizen'
    rig.location=(0,0,0);rig.rotation_euler=(0,0,0);rig.scale=(.811,)*3
    rig.animation_data_clear();rig.animation_data_create()
    rig.animation_data.action=bpy.data.actions[template.parent.name+'_'+clip]
    records.append(dict(scene=scene.name,citizen=hero.name,period=period,clip=clip,props=[],sport='General'))
for record in records:
    scene=bpy.data.scenes[record['scene']];bpy.context.window.scene=scene
    hero=bpy.data.objects[record['citizen']];source_rig=hero.parent;period=record['period']
    samples=[];bone_matrices=[]
    for frame in range(1,period+2):
        scene.frame_set(frame)
        bone_matrices.append({p.name:p.matrix.copy() for p in source_rig.pose.bones})
        def transform(obj):
            loc,rot,scale=obj.matrix_world.decompose()
            return dict(position=list(loc*100),quaternion=list(rot),scale=list(scale))
        samples.append(dict(body=transform(source_rig),props=[transform(bpy.data.objects[p]) for p in record['props']]))
    bpy.context.window.scene=native_scene
    rig=source_rig.copy();rig.data=source_rig.data.copy();native_scene.collection.objects.link(rig)
    rig.parent=None;rig.location=(0,0,0);rig.rotation_euler=(0,0,0);rig.scale=(1,1,1);rig.animation_data_clear()
    for p in rig.pose.bones:
        for c in list(p.constraints):p.constraints.remove(c)
    rig.name='Armature';rig.animation_data_create()
    select([rig]);bpy.ops.object.mode_set(mode='EDIT')
    for bone in rig.data.edit_bones:bone.head*=100;bone.tail*=100
    bpy.ops.object.mode_set(mode='OBJECT')
    action=bpy.data.actions.new('AS_Park_'+record['clip']);rig.animation_data.action=action
    for frame,matrices in enumerate(bone_matrices,1):
        matrices={name:matrix.copy() for name,matrix in matrices.items()}
        for matrix in matrices.values():matrix.translation*=100
        native_scene.frame_set(frame)
        for p in rig.pose.bones:
            p.rotation_mode='QUATERNION'
            parent_args=dict(parent_matrix=matrices[p.parent.name],parent_matrix_local=p.parent.bone.matrix_local) if p.parent else {}
            p.matrix_basis=p.bone.convert_local_to_pose(matrices[p.name],p.bone.matrix_local,invert=True,**parent_args)
            for key in ('location','rotation_quaternion','scale'):p.keyframe_insert(key,frame=frame,group=p.name)
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for p in curve.keyframe_points:p.interpolation='LINEAR'
    mesh=hero.copy();mesh.data=hero.data.copy();native_scene.collection.objects.link(mesh)
    for vertex in mesh.data.vertices:vertex.co*=100
    mesh.parent=rig;mesh.matrix_parent_inverse.identity();mesh.location=(0,0,0);mesh.rotation_euler=(0,0,0);mesh.scale=(1,1,1)
    mesh.modifiers['D83HumanoidSkin'].object=rig;mesh.name='SK_Park_Casual'
    native_scene.frame_start=1;native_scene.frame_end=period+1
    select([rig,mesh]);path=OUT/('AS_Park_'+record['clip']+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH','ARMATURE'},
        apply_unit_scale=True,apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',add_leaf_bones=False,
        bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,
        bake_anim_simplify_factor=0,bake_anim_step=1,path_mode='STRIP')
    # Independent readback compares the actual skinned point clouds, allowing
    # harmless FBX normal/color seam splitting without allowing pose drift.
    from mathutils.kdtree import KDTree
    def points(obj):
        bpy.context.view_layer.update();evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
        data=evaluated.to_mesh();values=[evaluated.matrix_world@v.co for v in data.vertices]
        evaluated.to_mesh_clear();return values
    expected={}
    for f in (1,1+int(period*.43),period+1):native_scene.frame_set(f);expected[f]=points(mesh)
    before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(path),anim_offset=0)
    check=next(o for o in set(bpy.data.objects)-before if o.type=='MESH')
    assert len(check.parent.data.bones)==20
    error=0
    for f,values in expected.items():
        native_scene.frame_set(f);actual=points(check)
        frame_error=0
        for a,b in ((values,actual),(actual,values)):
            tree=KDTree(len(b))
            for i,v in enumerate(b):tree.insert(v,i)
            tree.balance();frame_error=max(frame_error,max(tree.find(v)[2] for v in a))
        print('ROUNDTRIP_FRAME',record['clip'],f,frame_error,'fps',native_scene.render.fps)
        error=max(error,frame_error)
    assert error<.001,(record['clip'],'FBX deformed roundtrip',error)
    for o in set(bpy.data.objects)-before:bpy.data.objects.remove(o,do_unlink=True)
    clips.append(dict(name=record['clip'],period=period,fps=24,playback=record.get('playback','loop'),
                      export=path.relative_to(ROOT).as_posix(),sha256=digest(path),frames=samples,
                      roundtrip_pose_error_m=error*native_scene.unit_settings.scale_length,roundtrip_pass=True))
    if record['clip'] in ('SkateKickflip','BaseballPitch'):
        family='Skate' if record['sport']=='Skate' else 'Field'
        # Retain mesh family skinning and pair the original authored420-triangle LOD.
        for style in ('Casual','Office','Worker'):
            for lod in (0,1):
                with bpy.data.libraries.load(str(ROOT/'ArtSource/Blender/CityCitizenRigPrototype.blend'),link=False) as (a,b):
                    name='SK_Citizen_Rig_'+style+('_LOD1' if lod else '')
                    b.objects=[name]
                variant=b.objects[0];native_scene.collection.objects.link(variant)
                variant.parent=rig;variant.matrix_parent_inverse.identity();variant.location=(0,0,0);variant.rotation_euler=(0,0,0);variant.scale=(1,1,1)
                variant.modifiers['D83HumanoidSkin'].object=rig
                if family=='Skate':
                    import sys;sys.path.insert(0,str(ROOT/'Tools'));import CitySkateKeyframes
                    # Connected shoes use identical rigid-foot grouping as accepted scenes.
                    helper=rig.copy();helper.data=rig.data.copy();native_scene.collection.objects.link(helper)
                    state=CitySkateKeyframes.prepare(native_scene,variant,helper);CitySkateKeyframes.finish(helper,state)
                    bpy.data.objects.remove(helper,do_unlink=True)
                for vertex in variant.data.vertices:vertex.co*=100
                variant.name='SK_Park_'+family+'_'+style+('_LOD1' if lod else '')
                path=OUT/(variant.name+'.fbx');rig.animation_data_clear();rig.data.pose_position='REST'
                select([rig,variant]);bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH','ARMATURE'},
                    apply_unit_scale=True,apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',add_leaf_bones=False,bake_anim=False,path_mode='STRIP')
                rigs.append(dict(name=variant.name,family=family,style=style,lod=lod,triangles=tri(variant),
                                 export=path.relative_to(ROOT).as_posix(),sha256=digest(path),bones=len(rig.data.bones)))
                rig.data.pose_position='POSE'
                first_matrices={name:matrix.copy() for name,matrix in bone_matrices[0].items()}
                for matrix in first_matrices.values():matrix.translation*=100
                for p in rig.pose.bones:
                    parent_args=dict(parent_matrix=first_matrices[p.parent.name],parent_matrix_local=p.parent.bone.matrix_local) if p.parent else {}
                    p.matrix_basis=p.bone.convert_local_to_pose(first_matrices[p.name],p.bone.matrix_local,invert=True,**parent_args)
                bpy.context.view_layer.update()
                static=variant.copy();static.data=bpy.data.meshes.new_from_object(variant.evaluated_get(bpy.context.evaluated_depsgraph_get()))
                native_scene.collection.objects.link(static);static.modifiers.clear();static.parent=None
                static.name='SM_Park_'+family+'_'+style+('_LOD1' if lod else '')
                rigs[-1]['static']=export(static,static.name)
                bpy.data.objects.remove(static,do_unlink=True)
                bpy.data.objects.remove(variant,do_unlink=True)
        rig.data.pose_position='POSE'
    # Native prop geometry with frame transforms kept separately in choreography.
    for name in record['props']:
        original=bpy.data.objects[name]
        kind='Skateboard' if record['sport']=='Skate' else 'Bat' if name==record.get('bat') else 'Basketball' if record['sport']=='Basketball' else 'Baseball' if record['sport']=='Baseball' else 'Soccer'
        if any(p['kind']==kind for p in props):continue
        obj=original.copy();obj.data=original.data.copy();native_scene.collection.objects.link(obj)
        obj.parent=None;obj.animation_data_clear();obj.location=(0,0,0);obj.rotation_euler=(0,0,0);obj.scale=(1,1,1)
        obj.name='SM_ParkNative_'+kind
        native_scene.unit_settings.scale_length=1
        props.append(dict(kind=kind,name=obj.name,**export(obj,obj.name)));bpy.data.objects.remove(obj,do_unlink=True)
        native_scene.unit_settings.scale_length=.01
    bpy.data.objects.remove(mesh,do_unlink=True);bpy.data.objects.remove(rig,do_unlink=True)
assert len(statics)==18 and len(clips)==14 and len(rigs)==12
for path,h in inputs.items():assert digest(ROOT/path)==h,path
for e in statics:
    for m in ('Bark','Canopy'):
        if m in e['materials']:e['materials'][m]='/Game/CitySim/Art/Materials/MI_Environment'+m
REPORT.write_text(json.dumps(dict(task='D83',statics=statics,rigs=rigs,clips=clips,props=props,
    source_inputs=inputs,accepted_sources_unchanged=True,native_selected=False),indent=2)+'\n')
print('CITYSIM_D83_NATIVE_EXPORT_PASS 18 statics; 12 skeletal meshes; 14 clips')
