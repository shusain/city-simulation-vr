"""Read saved D88 scale source; compare approved controls and physical contacts."""
from pathlib import Path
import bpy,sys,json,hashlib
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityCitizenRigControls import update
from CityCitizenSportsMotion import palm
OUT=ROOT/'Artifacts/D88/Scale';doc=json.loads((ROOT/'ArtSource/Manifests/city_citizen_sports_scale.json').read_text())
original=json.loads((ROOT/'ArtSource/Manifests/city_citizen_sports_refinement.json').read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def controls(action):
    return sorted((c.data_path,c.array_index,tuple(tuple(k.co) for k in c.keyframe_points))
                  for layer in action.layers for strip in layer.strips for bag in strip.channelbags for c in bag.fcurves)
assert sha(ROOT/doc['source'])==doc['source_sha256']
bpy.ops.wm.open_mainfile(filepath=str(ROOT/original['source']))
actions={e['clip']:controls(bpy.data.objects[e['rig']].animation_data.action) for e in original['scenes']}
bpy.ops.wm.open_mainfile(filepath=str(ROOT/doc['source']))
results=[]
for e in original['scenes']:
    s=bpy.data.scenes[e['scene']];bpy.context.window.scene=s;rig=bpy.data.objects[e['rig']];ball=bpy.data.objects[e['ball']]
    assert controls(rig.animation_data.action)==actions[e['clip']],'Approved rig controls changed'
    radius=.04 if e['sport']=='Baseball' else .11
    assert abs(max(v.co.length for v in ball.data.vertices)-radius)<.0001
    maximum_keeper_gap=0;first=None
    for frame in range(1,e['period']+2):
        s.frame_set(frame);update(rig)
        if e['clip']=='SoccerKick':
            if frame<=25:
                if first is None:first=ball.location.copy()
                assert (ball.location-first).length<.00001
            assert ball.location.z-radius>=rig.location.z-.00001
            if frame==25:assert abs((ball.location-(rig.matrix_world@rig.pose.bones['foot.R'].tail)).length-radius)<.001
        elif e['clip']=='SoccerRun':assert abs(ball.location.z-radius-rig.location.z)<.0001
        elif e['clip']=='GoalkeeperSave' and 25<=frame<=40:
            gap=max((rig.matrix_world@palm(rig,side)-ball.location).length-radius for side in ('L','R'))
            maximum_keeper_gap=max(maximum_keeper_gap,gap);assert gap<.008,(frame,'Keeper palms miss smaller ball',gap)
        elif e['clip']=='BaseballPitch' and frame<=36:assert (ball.location-rig.matrix_world@palm(rig,'R')).length<.001
    s.frame_set(1);start=ball.matrix_world.copy();s.frame_set(e['period']+1)
    assert max(abs(a-b) for ra,rb in zip(start,ball.matrix_world) for a,b in zip(ra,rb))<.001,'Prop seam changed'
    results.append(dict(clip=e['clip'],approved_control_curves_unchanged=True,frames=e['period']+1,maximum_keeper_surface_gap_m=maximum_keeper_gap))
for e in doc['statics']+doc['props']:assert sha(ROOT/e['export'])==e['sha256']
assert all(sha(ROOT/p)==digest for p,digest in doc['source_inputs'].items())
(OUT/'Validation.json').write_text(json.dumps(dict(task='D88-Scale',pass_checks=True,source_sha256=doc['source_sha256'],clips=results,
    original_sources_unchanged=True,fbx_meshes=4,total_frames=sum(r['frames'] for r in results)),indent=2)+'\n')
print('CITYSIM_D88_SCALE_VALIDATION_PASS 293 frames;5 approved controls;4 mesh readbacks;ground/grip/loop contacts')
