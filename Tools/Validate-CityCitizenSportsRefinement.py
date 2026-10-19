"""D88 saved-source skin/contact/limits/loop checks, independent of pose authoring."""
from pathlib import Path
import bpy,sys,json,hashlib,math
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityCitizenRigControls import update,HINGE_LIMITS
from CityCitizenSportsMotion import palm
DOC=json.loads((ROOT/'ArtSource/Manifests/city_citizen_sports_refinement.json').read_text())
OUT=ROOT/'Artifacts/D88';OUT.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(ROOT/DOC['source'])==DOC['source_sha256']
for p,d in DOC['protected_inputs'].items():assert sha(ROOT/p)==d,p
bpy.ops.wm.open_mainfile(filepath=str(ROOT/DOC['source']))
results=[]
for record in DOC['scenes']:
    s=bpy.data.scenes[record['scene']];bpy.context.window.scene=s
    mesh=bpy.data.objects[record['citizen']];rig=mesh.parent;ball=bpy.data.objects[record['ball']]
    mesh.hide_set(False);rig.hide_set(False)
    errors={};skin_first=None;prop_first=None;max_target=0;min_ground=0;max_grip=0;last_ball=None;worst_target=None;worst_grip=None
    for frame in range(1,record['period']+2):
        s.frame_set(frame);update(rig)
        evaluated=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get());data=evaluated.to_mesh()
        points=[evaluated.matrix_world@v.co for v in data.vertices]
        assert all(math.isfinite(c) for v in points for c in v)
        data.calc_loop_triangles()
        assert all((points[t.vertices[1]]-points[t.vertices[0]]).cross(points[t.vertices[2]]-points[t.vertices[0]]).length>1e-9 for t in data.loop_triangles)
        min_ground=min(min_ground,min(v.z-rig.location.z for v in points))
        evaluated.to_mesh_clear()
        for side in ('L','R'):
            for kind,lower in (('hand','forearm'),('foot','shin')):
                p=rig.pose.bones[lower+'.'+side]
                error=(p.tail-rig.pose.bones['CTRL_'+kind+'.'+side].head).length
                if error>max_target:max_target=error;worst_target=(frame,kind+'.'+side,list(p.tail),list(rig.pose.bones['CTRL_'+kind+'.'+side].head))
                args=dict(parent_matrix=p.parent.matrix,parent_matrix_local=p.parent.bone.matrix_local)
                angles=p.bone.convert_local_to_pose(p.matrix,p.bone.matrix_local,invert=True,**args).to_euler('XYZ')
                lo,hi=HINGE_LIMITS[p.name]
                assert lo-1<=math.degrees(angles.z)<=hi+1,(record['clip'],frame,p.name,'hinge')
                assert abs(math.degrees(angles.x))<1 and abs(math.degrees(angles.y))<1
        if record['clip']=='BaseballPitch' and frame<=record['release_frame']:
            assert (ball.location-(rig.matrix_world@palm(rig,'R'))).length<.001
        elif record['clip']=='BaseballSwing':
            bat=bpy.data.objects[record['bat']]
            right=rig.matrix_world@palm(rig,'R');left=rig.matrix_world@palm(rig,'L')
            assert (right-bat.matrix_world@Vector((0,0,-.425))).length<.001
            error=(left-bat.matrix_world@Vector((0,0,-.360))).length
            if error>max_grip:max_grip=error;worst_grip=(frame,list(left),list(bat.matrix_world@Vector((0,0,-.360))))
            if frame==record['impact_frame']:assert (ball.location-bat.matrix_world@Vector((0,0,.275))).length<.001
        elif record['clip']=='SoccerKick':
            if frame<record['impact_frame'] and last_ball is not None:assert (ball.location-last_ball).length<1e-6,'Ball moves before kick'
            if frame==record['impact_frame']:
                toe=rig.matrix_world@rig.pose.bones['foot.R'].tail
                assert abs((ball.location-toe).length-.13)<.001,'Toe does not meet the ball surface'
        elif record['clip']=='GoalkeeperSave' and record['catch_frame']<=frame<=record['hold_end_frame']:
            palms=[rig.matrix_world@palm(rig,side) for side in ('L','R')]
            assert all((p-ball.location).length<.18 for p in palms),('Hands miss keeper ball',frame)
        if frame==1:
            skin_first=points;prop_first={n:bpy.data.objects[n].matrix_world.copy() for n in record['props']}
        if frame==record['period']+1:
            assert max((a-b).length for a,b in zip(skin_first,points))<.002,(record['clip'],'skin loop seam')
            assert all(max(abs(a-b) for rowa,rowb in zip(mat,bpy.data.objects[n].matrix_world) for a,b in zip(rowa,rowb))<.002 for n,mat in prop_first.items()),(record['clip'],'prop loop seam')
        last_ball=ball.location.copy()
    results.append(dict(clip=record['clip'],frames=record['period']+1,maximum_ik_target_error_m=max_target,
                        lowest_sole_relative_ground_m=min_ground,maximum_left_bat_grip_error_m=max_grip,
                        finite_nondegenerate_skin=True,hinge_limits=True,loop_seam_pass=True))
    assert max_target<.018,(record['clip'],'unreachable authored controls',max_target,worst_target)
    assert min_ground>-.012,(record['clip'],'ground penetration',min_ground)
    if record['clip']=='BaseballSwing':assert max_grip<.008,('Two-handed bat grip',max_grip,worst_grip)

# Pitch and batter own the same play clock and transfer one visible ball at contact.
pair=bpy.data.scenes[DOC['baseball_pair_scene']];bpy.context.window.scene=pair
pitch=next(r for r in DOC['scenes'] if r['clip']=='BaseballPitch');swing=next(r for r in DOC['scenes'] if r['clip']=='BaseballSwing')
pair.frame_set(DOC['baseball_impact_frame']);a=bpy.data.objects[pitch['ball']];b=bpy.data.objects[swing['ball']]
assert a.scale.length<1e-6 and b.scale.x>.99
for p,d in DOC['protected_inputs'].items():assert sha(ROOT/p)==d,p
assert sha(ROOT/DOC['source'])==DOC['source_sha256']
(OUT/'SourceValidation.json').write_text(json.dumps(dict(task='D88',source_sha256=DOC['source_sha256'],clips=results,
        total_frames=sum(r['frames'] for r in results),baseball_synchronized=True,original_inputs_unchanged=True,pass_checks=True),indent=2)+'\n')
print('CITYSIM_D88_SOURCE_VALIDATION_PASS',sum(r['frames'] for r in results),'frames;5 clips;contacts/limits/loops')
