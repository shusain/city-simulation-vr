"""D88 stage five native clips and paired field meshes without importing Unreal."""
from pathlib import Path
import bpy,sys,json,hashlib
from mathutils import Vector,Matrix
from mathutils.kdtree import KDTree
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityCitizenRigControls import bake,update,BASE_BONES
SOURCE=json.loads((ROOT/'ArtSource/Manifests/city_citizen_sports_refinement.json').read_text())
OUT=ROOT/'ArtSource/Exports/CitizenSportsRefinement';OUT.mkdir(parents=True,exist_ok=True)
REPORT=ROOT/'ArtSource/Manifests/city_citizen_sports_native.json'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(ROOT/SOURCE['source'])==SOURCE['source_sha256']
assert json.loads((ROOT/'Artifacts/D88/SourceValidation.json').read_text())['source_sha256']==SOURCE['source_sha256']
bpy.ops.wm.open_mainfile(filepath=str(ROOT/SOURCE['source']))
native=bpy.data.scenes.new('D88_NativeExports');native.unit_settings.system='METRIC';native.unit_settings.scale_length=.01;native.render.fps=24
def select(objects):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:o.hide_set(False);o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
def export(rig,mesh,path,animated):
    select([rig,mesh])
    assert rig.name=='Armature'
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH','ARMATURE'},
        apply_unit_scale=True,apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',
        mesh_smooth_type='FACE',add_leaf_bones=False,bake_anim=animated,
        bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0,bake_anim_step=1,path_mode='STRIP')
def points(mesh):
    bpy.context.view_layer.update();obj=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get());data=obj.to_mesh()
    values=[obj.matrix_world@v.co for v in data.vertices];obj.to_mesh_clear();return values
def pose_transform(obj):
    loc,rot,scale=obj.matrix_world.decompose()
    return dict(position=list(loc*100),quaternion=list(rot),scale=list(scale))
clips=[];rigs=[]
for record in SOURCE['scenes']:
    s=bpy.data.scenes[record['scene']];bpy.context.window.scene=s
    mesh=bpy.data.objects[record['citizen']];rig=mesh.parent;period=record['period']
    samples=[]
    for frame in range(1,period+2):
        s.frame_set(frame);update(rig)
        samples.append(dict(body=pose_transform(rig),props=[pose_transform(bpy.data.objects[n]) for n in record['props']]))
    clean,skin=bake(rig,mesh,'AS_Park_'+record['clip'],range(1,period+2),s)
    s.collection.objects.unlink(clean);s.collection.objects.unlink(skin)
    native.collection.objects.link(clean);native.collection.objects.link(skin);bpy.context.window.scene=native
    clean.name='Armature'
    select([clean,skin]);bpy.ops.object.mode_set(mode='EDIT')
    for b in clean.data.edit_bones:b.head*=100;b.tail*=100
    bpy.ops.object.mode_set(mode='OBJECT')
    for v in skin.data.vertices:v.co*=100
    action=clean.animation_data.action
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    if curve.data_path.endswith('.location'):
                        for key in curve.keyframe_points:key.co.y*=100;key.handle_left.y*=100;key.handle_right.y*=100
    native.frame_start=1;native.frame_end=period+1
    path=OUT/('AS_Park_'+record['clip']+'.fbx');export(clean,skin,path,True)
    keyframes=sorted(set((1,period//4+1,period//2+1,record.get('impact_frame',record.get('release_frame',record.get('catch_frame',period//2+1))),int(period*.8)+1,period+1)))
    expected={}
    for f in keyframes:native.frame_set(f);update(clean);expected[f]=points(skin)
    before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(path),anim_offset=0)
    check=next(o for o in set(bpy.data.objects)-before if o.type=='MESH')
    assert set(check.parent.data.bones.keys())==set(BASE_BONES)
    error=0
    for f,a in expected.items():
        native.frame_set(f);update(check.parent);b=points(check)
        for aa,bb in ((a,b),(b,a)):
            tree=KDTree(len(bb))
            for i,v in enumerate(bb):tree.insert(v,i)
            tree.balance();error=max(error,max(tree.find(v)[2] for v in aa))
    assert error<.025,(record['clip'],'native FBX deformation drift cm',error)
    for o in set(bpy.data.objects)-before:bpy.data.objects.remove(o,do_unlink=True)
    clips.append(dict(name=record['clip'],period=period,fps=24,playback='loop',export=path.relative_to(ROOT).as_posix(),
                      sha256=sha(path),frames=samples,roundtrip_pose_error_m=error*.01,roundtrip_pass=True,
                      source=SOURCE['source'],source_scene=record['scene'],refinement_task='D88'))
    if record['clip']=='BaseballPitch':
        for style in ('Casual','Office','Worker'):
            for lod in (0,1):
                name='SK_Citizen_Rig_'+style+('_LOD1' if lod else '')
                with bpy.data.libraries.load(str(ROOT/'ArtSource/Blender/CityCitizenControlRig.blend'),link=False) as (a,b):b.objects=[name]
                variant=b.objects[0];native.collection.objects.link(variant)
                variant.parent=clean;variant.matrix_parent_inverse=Matrix.Identity(4);variant.location=(0,0,0);variant.rotation_euler=(0,0,0);variant.scale=(1,1,1)
                variant.modifiers['D83HumanoidSkin'].object=clean
                for v in variant.data.vertices:v.co*=100
                variant.name='SK_Park_Field_'+style+('_LOD1' if lod else '')
                variant.data.calc_loop_triangles();triangles=len(variant.data.loop_triangles)
                assert triangles==(420 if lod else 754)
                clean.data.pose_position='REST';path=OUT/(variant.name+'.fbx');export(clean,variant,path,False)
                clean.data.pose_position='POSE';native.frame_set(1);update(clean)
                fallback=variant.copy();fallback.data=bpy.data.meshes.new_from_object(variant.evaluated_get(bpy.context.evaluated_depsgraph_get()))
                native.collection.objects.link(fallback);fallback.modifiers.clear();fallback.parent=None;fallback.name=variant.name.replace('SK_','SM_')
                static_path=OUT/(fallback.name+'.fbx');select([fallback])
                bpy.ops.export_scene.fbx(filepath=str(static_path),use_selection=True,object_types={'MESH'},apply_unit_scale=True,
                    apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',bake_anim=False,path_mode='STRIP')
                rigs.append(dict(name=variant.name,family='Field',style=style,lod=lod,triangles=triangles,bones=20,
                                 export=path.relative_to(ROOT).as_posix(),sha256=sha(path),
                                 static=dict(name=fallback.name,export=static_path.relative_to(ROOT).as_posix(),sha256=sha(static_path),triangles=triangles)))
                bpy.data.objects.remove(fallback,do_unlink=True);bpy.data.objects.remove(variant,do_unlink=True)
    bpy.data.objects.remove(skin,do_unlink=True);bpy.data.objects.remove(clean,do_unlink=True)
for p,d in SOURCE['protected_inputs'].items():assert sha(ROOT/p)==d,p
assert sha(ROOT/SOURCE['source'])==SOURCE['source_sha256']
doc=dict(task='D88',source=SOURCE['source'],source_sha256=SOURCE['source_sha256'],clips=clips,rigs=rigs,
         native_selected=False,baseball_period=72,original_inputs_unchanged=True)
REPORT.write_text(json.dumps(doc,indent=2)+'\n')
print('CITYSIM_D88_NATIVE_STAGE_PASS 5 clips;6 field rig/LOD exports;6 paired fallbacks;20-bone FBX readbacks')
