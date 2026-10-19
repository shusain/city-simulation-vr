"""D121 saved-source checks and clean 20-bone authoring FBX readback."""
from pathlib import Path
import hashlib
import json
import math
import sys
import bpy
from mathutils import Vector
from mathutils.kdtree import KDTree

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'Tools'))
from CityCitizenRigControls import update, bake, BASE_BONES, HINGE_LIMITS
# The bake tool filename contains hyphens; load only its export function.
import runpy
export = runpy.run_path(str(ROOT/'Tools/Bake-CityCitizenControlRig.py'))['export']
DOC = json.loads((ROOT/'ArtSource/Manifests/city_citizen_walk_cycle_v2.json').read_text())
OUT = ROOT/'Artifacts/D121/V2'
OUT.mkdir(parents=True, exist_ok=True)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def points(mesh, world=False):
    evaluated = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
    data = evaluated.to_mesh()
    values = [(evaluated.matrix_world@v.co if world else v.co.copy()) for v in data.vertices]
    assert all(math.isfinite(c) for v in values for c in v)
    data.calc_loop_triangles()
    assert all((values[t.vertices[1]]-values[t.vertices[0]]).cross(
        values[t.vertices[2]]-values[t.vertices[0]]).length > 1e-9 for t in data.loop_triangles)
    evaluated.to_mesh_clear()
    return values


assert sha(ROOT/DOC['source']) == DOC['source_sha256']
for path, digest in DOC['protected_inputs'].items(): assert sha(ROOT/path) == digest, path
bpy.ops.wm.open_mainfile(filepath=str(ROOT/DOC['source']))
if bpy.context.object and bpy.context.object.mode != 'OBJECT': bpy.ops.object.mode_set(mode='OBJECT')
scene = bpy.context.scene
period = DOC['period_frames']
results = []
for record in DOC['records']:
    mesh = bpy.data.objects[record['mesh']]; rig = mesh.parent
    mesh.hide_set(False); rig.hide_set(False)
    error = 0; ground = 1; sole_error = 0; stretch = 0; seam = 0; first = None; slip = 0
    wrist_error = 0; wrist_direction = 0; torso_yaws = []; torso_leans = []
    prior = {}
    # Integer and half-frame poses catch both the keyed and interpolated motion.
    for sample in range(period*2+1):
        clock = 1+sample*.5
        scene.frame_set(int(clock), subframe=clock-int(clock)); update(rig)
        skin = points(mesh)
        ground = min(ground, min(v.z for v in skin))
        if first is None: first = skin
        if sample == period*2: seam = max((a-b).length for a,b in zip(first, skin))
        assert rig.pose.bones['root'].matrix_basis.translation.length < 1e-6
        chest = rig.pose.bones['chest']
        chest_forward = (chest.matrix @ chest.bone.matrix_local.inverted()).to_3x3() @ Vector((1,0,0))
        chest_up = (chest.tail-chest.head).normalized()
        torso_yaws.append(math.degrees(math.atan2(chest_forward.y, chest_forward.x)))
        torso_leans.append(math.degrees(math.atan2(chest_up.x, chest_up.z)))
        for side, offset in (('L', 0), ('R', .5)):
            phase = (sample*.5/period+offset) % 1
            for kind, lower, upper in (('foot','shin','thigh'), ('hand','forearm','upper_arm')):
                bone = rig.pose.bones[lower+'.'+side]
                error = max(error, (bone.tail-rig.pose.bones['CTRL_'+kind+'.'+side].head).length)
                angles = bone.bone.convert_local_to_pose(bone.matrix, bone.bone.matrix_local,
                    invert=True, parent_matrix=bone.parent.matrix,
                    parent_matrix_local=bone.parent.bone.matrix_local).to_euler('XYZ')
                lo, hi = HINGE_LIMITS[bone.name]
                assert lo-1 <= math.degrees(angles.z) <= hi+1, (record,clock,bone.name,angles)
                assert abs(angles.x)<math.radians(1) and abs(angles.y)<math.radians(1)
                for name in (lower, upper):
                    limb = rig.pose.bones[name+'.'+side]
                    stretch = max(stretch, abs((limb.tail-limb.head).length-limb.bone.length))
            fore = rig.pose.bones['forearm.'+side]; hand = rig.pose.bones['hand.'+side]
            wanted = fore.matrix @ fore.bone.matrix_local.inverted() @ hand.bone.matrix_local
            wanted_direction = wanted.to_3x3() @ Vector((0,1,0))
            correction = wanted_direction.rotation_difference((fore.tail-fore.head).normalized())
            wanted = correction.to_matrix().to_4x4() @ wanted
            wrist_error = max(wrist_error, math.degrees(wanted.to_quaternion().rotation_difference(
                hand.matrix.to_quaternion()).angle))
            wrist_direction = max(wrist_direction, math.degrees((fore.tail-fore.head).angle(hand.tail-hand.head)))
            # Inspect actual evaluated shoe vertices, rather than only control targets.
            group = mesh.vertex_groups['foot.'+side].index
            indices = [v.index for v in mesh.data.vertices if any(
                g.group == group and g.weight > .999 for g in v.groups)]
            sole = min(skin[i].z for i in indices)
            if phase < DOC['stance_fraction']:
                sole_error = max(sole_error, abs(sole))
                point = rig.pose.bones['foot.'+side].head.copy()
                point.x += DOC['stride_authoring_m'] * sample*.5/period
                if side in prior and phase > prior[side][0]:
                    slip = max(slip, (point-prior[side][1]).length)
                prior[side] = (phase, point)
            else: prior.pop(side, None)
    row = dict(style=record['style'], lod=record['lod'], samples=period*2+1,
               maximum_ik_error_m=error, minimum_sole_m=ground,
               maximum_stance_sole_error_m=sole_error, stance_world_slip_per_half_frame_m=slip,
               maximum_limb_stretch_m=stretch, loop_seam_m=seam,
               maximum_forearm_follow_error_deg=wrist_error, maximum_wrist_direction_angle_deg=wrist_direction,
               shoulder_twist_range_deg=[min(torso_yaws),max(torso_yaws)],
               forward_torso_lean_range_deg=[min(torso_leans),max(torso_leans)])
    print('SOURCE_METRICS', json.dumps(row))
    assert error < .018, ('unreachable target', row)
    assert ground > -.004 and sole_error < .004, ('sole contact', row)
    assert slip < .004 and stretch < .0001 and seam < .0001, ('motion continuity', row)
    assert wrist_error < 1, ('hand does not follow forearm',row)
    assert wrist_direction < 1, ('wrist bends away from forearm',row)
    assert min(torso_yaws) < -7 and max(torso_yaws) > 7, ('shoulder twist cancels',row)
    assert min(torso_leans) > 3 and max(torso_leans) < 9, ('forward lean',row)
    results.append(row)

# One representative full-detail bake checks the common skeleton/export path.
mesh = bpy.data.objects['SK_Citizen_Rig_Casual']; rig = mesh.parent
rig.location = (0,0,0); rig.scale = (1,1,1)
frames = range(1, period+2)
expected = {}
for frame in frames:
    scene.frame_set(frame); update(rig); expected[frame] = points(mesh, True)
clean, skin = bake(rig, mesh, 'D121_Walk_Casual_Baked', frames, scene)
assert set(clean.data.bones.keys()) == set(BASE_BONES)
assert all(not bone.constraints for bone in clean.pose.bones)
bake_error = 0
for frame in frames:
    scene.frame_set(frame); update(clean)
    bake_error = max(bake_error, max((a-b).length for a,b in zip(expected[frame], points(skin, True))))
assert bake_error < .001, bake_error
path = OUT/'Walk-Casual-Authoring.fbx'
# Validation may refresh its own generated bridge, never production exports.
scene.frame_start = 1; scene.frame_end = period+1
export(clean, skin, path)
before = set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=str(path), anim_offset=0)
imported = next(o for o in set(bpy.data.objects)-before if o.type=='MESH')
import_rig = imported.parent
assert set(import_rig.data.bones.keys()) == set(BASE_BONES)
assert all(not p.constraints for p in import_rig.pose.bones)
fbx_error = 0
for frame in (1,5,9,15,19,23,29):
    scene.frame_set(frame); update(import_rig)
    actual = points(imported, True)
    for a,b in ((expected[frame],actual),(actual,expected[frame])):
        tree=KDTree(len(b))
        for i,v in enumerate(b): tree.insert(v,i)
        tree.balance(); fbx_error=max(fbx_error,max(tree.find(v)[2] for v in a))
assert fbx_error < .001, ('FBX deformation', fbx_error)
for path_, digest in DOC['protected_inputs'].items(): assert sha(ROOT/path_) == digest, path_
assert sha(ROOT/DOC['source']) == DOC['source_sha256']
report = dict(task='D121', version=2, source_sha256=DOC['source_sha256'], pass_checks=True,
              rigs=results, total_pose_samples=sum(r['samples'] for r in results),
              baked_frames=period+1, bake_error_m=bake_error, fbx_sample_frames=7,
              fbx_error_m=fbx_error, export_bones=20, controls_exported=0, constraints_exported=0,
              bridge=str(path.relative_to(ROOT)), bridge_sha256=sha(path),
              protected_inputs_unchanged=True, human_review='pending', native_status='not imported')
(OUT/'SourceValidation.json').write_text(json.dumps(report,indent=2)+'\n')
print('CITYSIM_D121_VALIDATION_PASS', report['total_pose_samples'], 'poses; 29 baked frames; 7 FBX readbacks')
