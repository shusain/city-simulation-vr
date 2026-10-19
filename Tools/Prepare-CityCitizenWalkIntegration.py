"""Stage the approved D121 v2 walk in native cm, retaining source/native meshes."""
from pathlib import Path
import bpy,sys,json,hashlib
from mathutils.kdtree import KDTree
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityCitizenRigControls import bake,update,BASE_BONES
OUT=ROOT/'Artifacts/D121/Integration';OUT.mkdir(parents=True,exist_ok=True)
DOC=json.loads((ROOT/'ArtSource/Manifests/city_citizen_walk_cycle_v2.json').read_text(encoding='utf-8-sig'))
assert DOC['status']=='approved'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(ROOT/DOC['source'])=='1cf0a3d40f92d23c80b2a84fa4e5cf2026617748a5940ebb318107e1a82940bb'
assert json.loads((ROOT/'docs/evidence/D121-v2-approval.json').read_text(encoding='utf-8-sig'))['source_sha256']==DOC['source_sha256']
path=ROOT/'ArtSource/Exports/CitizenWalk/AS_Citizen_Walk.fbx'
assert not path.exists(),'Preserve staged approved bridge'
path.parent.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(ROOT/DOC['source']))
if bpy.context.object and bpy.context.object.mode!='OBJECT':bpy.ops.object.mode_set(mode='OBJECT')
scene=bpy.context.scene;mesh=bpy.data.objects['SK_Citizen_Rig_Casual'];rig=mesh.parent
rig.location=(0,0,0);rig.scale=(1,1,1);rig.hide_set(False);mesh.hide_set(False)
clean,skin=bake(rig,mesh,'AS_Citizen_Walk',range(1,30),scene)
native=bpy.data.scenes.new('D121_NativeWalk');native.unit_settings.system='METRIC';native.unit_settings.scale_length=.01;native.render.fps=24
scene.collection.objects.unlink(clean);scene.collection.objects.unlink(skin)
native.collection.objects.link(clean);native.collection.objects.link(skin);bpy.context.window.scene=native
clean.name='Armature'
bpy.ops.object.select_all(action='DESELECT');clean.hide_set(False);clean.select_set(True);bpy.context.view_layer.objects.active=clean
bpy.ops.object.mode_set(mode='EDIT')
for b in clean.data.edit_bones:b.head*=100;b.tail*=100
bpy.ops.object.mode_set(mode='OBJECT')
for v in skin.data.vertices:v.co*=100
for layer in clean.animation_data.action.layers:
 for strip in layer.strips:
  for bag in strip.channelbags:
   for curve in bag.fcurves:
    if curve.data_path.endswith('.location'):
     for key in curve.keyframe_points:key.co.y*=100;key.handle_left.y*=100;key.handle_right.y*=100
native.frame_start=1;native.frame_end=29
skin.select_set(True)
bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH','ARMATURE'},
 apply_unit_scale=True,apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',
 add_leaf_bones=False,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0,bake_anim_step=1,path_mode='STRIP')
def points(obj):
 update(obj.parent);e=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());m=e.to_mesh();p=[e.matrix_world@v.co for v in m.vertices];e.to_mesh_clear();return p
expected={}
for frame in [1,5,9,15,19,23,29]:native.frame_set(frame);expected[frame]=points(skin)
before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(path),anim_offset=0)
check=next(o for o in set(bpy.data.objects)-before if o.type=='MESH')
assert set(check.parent.data.bones.keys())==set(BASE_BONES)
error=0
for frame,a in expected.items():
 native.frame_set(frame);b=points(check)
 for aa,bb in [(a,b),(b,a)]:
  tree=KDTree(len(bb))
  for i,v in enumerate(bb):tree.insert(v,i)
  tree.balance();error=max(error,max(tree.find(v)[2] for v in aa))
assert error<.025,error
doc=dict(task='D121.2',version=2,source=DOC['source'],source_sha256=DOC['source_sha256'],export=path.relative_to(ROOT).as_posix(),sha256=sha(path),
 seconds=28/24,frames=29,bones=20,controls=0,roundtrip_error_cm=error,
 native_expected_skin={str(f):[[v.x,-v.y,v.z] for v in a] for f,a in expected.items()},
 reused_mesh_contract='D88 field rigs were authored from identical protected D87 source and restored original20-bone bind. Native tests compare skin samples, actual soles and all outfit/LOD reference poses.')
(ROOT/'ArtSource/Manifests/city_citizen_walk_native.json').write_text(json.dumps(doc,indent=2)+'\n')
assert sha(ROOT/DOC['source'])==DOC['source_sha256']
print('CITYSIM_D121_STAGE_PASS cm;20 bones;7 deformed FBX readbacks',error)
