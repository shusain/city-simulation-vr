"""D87 focused saved-rig posing, limits, bake and FBX deformation checks."""
from pathlib import Path
import bpy, sys, json, math, hashlib, runpy
from mathutils import Vector
from mathutils.kdtree import KDTree
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityCitizenRigControls import BASE_BONES, BODY_LIMITS, HINGE_LIMITS, reset, preset, move_control, update, bake
export=runpy.run_path(str(ROOT/'Tools/Bake-CityCitizenControlRig.py'))['export']
OUT=ROOT/'Artifacts/D87';OUT.mkdir(parents=True,exist_ok=True)
MANIFEST=ROOT/'ArtSource/Manifests/city_citizen_control_rig.json'
manifest=json.loads(MANIFEST.read_text())
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
for name,digest in manifest['protected_inputs'].items():assert sha(ROOT/name)==digest,name
assert sha(ROOT/manifest['source'])==manifest['source_sha256']
bpy.ops.wm.open_mainfile(filepath=str(ROOT/manifest['source']))
if bpy.context.object and bpy.context.object.mode!='OBJECT':bpy.ops.object.mode_set(mode='OBJECT')
scene=bpy.context.scene
def points(mesh):
    evaluated=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get());data=evaluated.to_mesh()
    result=[evaluated.matrix_world@v.co for v in data.vertices]
    assert all(math.isfinite(c) for v in result for c in v)
    data.calc_loop_triangles()
    assert all((result[t.vertices[1]]-result[t.vertices[0]]).cross(result[t.vertices[2]]-result[t.vertices[0]]).length>1e-9 for t in data.loop_triangles)
    evaluated.to_mesh_clear();return result
def local_rotation(p):
    args=dict(parent_matrix=p.parent.matrix,parent_matrix_local=p.parent.bone.matrix_local) if p.parent else {}
    basis=p.bone.convert_local_to_pose(p.matrix,p.bone.matrix_local,invert=True,**args)
    return basis.to_euler('XYZ')
results=[];pose_dump={}
for entry in manifest['records']:
    mesh=bpy.data.objects[entry['mesh']];rig=mesh.parent
    rig.hide_set(False);mesh.hide_set(False);rig.location=(0,0,0)
    assert len(rig.data.bones)==28
    assert all(not rig.data.bones[n].use_deform for n in entry['controls'])
    assert all(abs(sum(g.weight for g in v.groups)-1)<1e-6 and len(v.groups)<=2 for v in mesh.data.vertices)
    cases=[]
    for case in ('Neutral','Crouch','Reach','Step'):
        preset(rig,case);skin=points(mesh)
        errors={};hinges={}
        for side in ('L','R'):
            for kind,lower,upper in (('hand','forearm','upper_arm'),('foot','shin','thigh')):
                p=rig.pose.bones[lower+'.'+side];control=rig.pose.bones['CTRL_'+kind+'.'+side]
                error=(p.tail-control.head).length
                errors[kind+'.'+side]=error
                angles=local_rotation(p);degrees=[math.degrees(v) for v in angles]
                hinges[lower+'.'+side]=degrees
                lo,hi=HINGE_LIMITS[p.name]
                assert lo-1<=degrees[2]<=hi+1,(entry['rig'],case,lower+'.'+side,'hinge',degrees)
                assert abs(degrees[0])<1 and abs(degrees[1])<1,(entry['rig'],case,lower,'off-axis',degrees)
                for n in (lower,upper):
                    b=rig.pose.bones[n+'.'+side]
                    assert abs((b.tail-b.head).length-b.bone.length)<1e-4,(case,n,'stretch over0.1mm',abs((b.tail-b.head).length-b.bone.length))
        assert max(errors.values())<.015,(entry['rig'],case,'target reach',errors,hinges)
        if case=='Crouch':
            assert all((rig.pose.bones['foot.'+side].head-rig.data.bones['foot.'+side].head_local).length<.015 for side in ('L','R'))
            assert abs(min(v.z for v in skin))<.001,('planted soles',entry['mesh'],min(v.z for v in skin))
            assert all(rig.pose.bones['shin.'+side].head.x>0 for side in ('L','R')),('backward knee',entry['mesh'])
        cases.append(dict(pose=case,target_errors_m=errors,hinge_angles_local_xyz_degrees=hinges,finite_skin=True))
        if entry['mesh']=='SK_Citizen_Rig_Casual':
            pose_dump[case]=dict(bones=[dict(name=p.name,head=list(p.head),tail=list(p.tail)) for p in rig.pose.bones],
                                 skin_bounds=[[min(v[i] for v in skin),max(v[i] for v in skin)] for i in range(3)])
    # Out-of-reach targets must not stretch a limb to satisfy the handle.
    preset(rig,'Neutral');move_control(rig,'CTRL_hand.L',(2,0,0));move_control(rig,'CTRL_foot.L',(2,0,0))
    for name in ('upper_arm.L','forearm.L','thigh.L','shin.L'):
        p=rig.pose.bones[name];assert abs((p.tail-p.head).length-p.bone.length)<1e-4
    points(mesh)
    # Pole handles must steer joints while the terminal target stays reachable.
    pole_checks=[]
    for kind,lower,pole in (('hand','forearm','elbow'),('foot','shin','knee')):
        preset(rig,'Crouch');p=rig.pose.bones[lower+'.L'];before_joint=p.head.copy()
        move_control(rig,'CTRL_'+pole+'_pole.L',(0,.20,0))
        assert (p.head-before_joint).length>.015,('pole did not steer',entry['mesh'],pole)
        assert (p.tail-rig.pose.bones['CTRL_'+kind+'.L'].head).length<.015
        pole_checks.append(pole)
    # Near-root targets demand an excessive fold; the solver must obey its cap.
    fold_checks=[]
    for side in ('L','R'):
        for kind,lower,upper in (('hand','forearm','upper_arm'),('foot','shin','thigh')):
            reset(rig)
            control=rig.pose.bones['CTRL_'+kind+'.'+side]
            goal=rig.pose.bones[upper+'.'+side].head+Vector((.025,0,-.025))
            move_control(rig,control.name,goal-control.bone.head_local)
            angles=[math.degrees(v) for v in local_rotation(rig.pose.bones[lower+'.'+side])]
            lo,hi=HINGE_LIMITS[lower+'.'+side]
            assert lo-1<=angles[2]<=hi+1 and abs(angles[0])<1 and abs(angles[1])<1
            fold_checks.append(dict(bone=lower+'.'+side,angle_degrees=angles[2]))
            points(mesh)
    # Body, wrist and ankle limits must clamp evaluated rotations, not just be present.
    for name,ranges in BODY_LIMITS.items():
        names=[name] if name in ('pelvis','spine','chest','neck','head') else [name+'.L',name+'.R']
        for bone_name in names:
            for sign in (-1,1):
                reset(rig);p=rig.pose.bones[bone_name]
                if name in ('hand','foot'):
                    # Terminal orientation is driven by its corresponding handle.
                    rig.pose.bones['CTRL_'+bone_name].rotation_euler=(sign*2.2,sign*1.1,sign*2.2)
                else:p.rotation_euler=(sign*2.2,sign*1.1,sign*2.2)
                update(rig);degrees=[math.degrees(v) for v in local_rotation(p)]
                assert all(lo-1<=angle<=hi+1 for angle,(lo,hi) in zip(degrees,ranges)),(bone_name,'clamp',degrees,ranges)
    reset(rig)
    results.append(dict(mesh=entry['mesh'],rig=rig.name,poses=cases,unreachable_no_stretch=True,
                        pole_steering=pole_checks,excessive_fold_limits=fold_checks,evaluated_rotation_clamps=True))

# Independent evaluated skin comparison: control demo -> clean bake -> imported FBX.
mesh=bpy.data.objects['SK_Citizen_Rig_Casual'];rig=mesh.parent;rig.location=(0,0,0)
rig.animation_data_create();rig.animation_data.action=bpy.data.actions['D87_Demo_PlantedCrouch']
expected={}
demo_ground_error=0
for frame in range(1,50):
    scene.frame_set(frame);update(rig);expected[frame]=points(mesh)
    demo_ground_error=max(demo_ground_error,abs(min(v.z for v in expected[frame])))
assert demo_ground_error<.001,('demo soles',demo_ground_error)
target,skin=bake(rig,mesh,'D87_Validation_Crouch',range(1,50),scene)
assert set(target.data.bones.keys())==set(BASE_BONES) and not any(p.constraints for p in target.pose.bones)
bake_error=0
for frame in range(1,50):
    scene.frame_set(frame);update(target)
    bake_error=max(bake_error,max((a-b).length for a,b in zip(expected[frame],points(skin))))
assert bake_error<1e-4,('bake deformation exceeds0.1mm',bake_error)
scene.frame_start=1;scene.frame_end=49
path=OUT/'D87-Validation-Crouch.fbx';export(target,skin,path)
before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(path),anim_offset=0)
imported=[o for o in set(bpy.data.objects)-before if o.type=='MESH'];assert len(imported)==1
check=imported[0];import_rig=check.parent
assert set(import_rig.data.bones.keys())==set(BASE_BONES)
assert not any(p.constraints for p in import_rig.pose.bones)
fbx_error=0
for frame in (1,7,13,19,25,37,49):
    scene.frame_set(frame);update(import_rig)
    # FBX may split vertices at color/normal seams; compare bidirectional clouds.
    actual=points(check)
    for a,b in ((expected[frame],actual),(actual,expected[frame])):
        tree=KDTree(len(b))
        for i,v in enumerate(b):tree.insert(v,i)
        tree.balance();fbx_error=max(fbx_error,max(tree.find(v)[2] for v in a))
assert fbx_error<.001,('FBX deformation',fbx_error)
for name,digest in manifest['protected_inputs'].items():assert sha(ROOT/name)==digest,name
assert sha(ROOT/manifest['source'])==manifest['source_sha256']
report=dict(task='D87',rigs=results,pose_samples=pose_dump,bake_frames=49,bake_error_m=bake_error,
            demo_sole_ground_error_m=demo_ground_error,
            fbx_frames=7,fbx_error_m=fbx_error,export_bones=20,export_controls=0,export_constraints=0,
            source_sha256=manifest['source_sha256'],protected_inputs_unchanged=True,pass_checks=True)
(OUT/'Validation.json').write_text(json.dumps(report,indent=2)+'\n')
print('CITYSIM_D87_VALIDATION_PASS 6 rigs;24 poses;hinges/clamps/no-stretch;49 baked frames;7 FBX samples',bake_error,fbx_error)
