"""Freeze/verify user-accepted baseball/soccer action curves for D83 follow-ups."""
from pathlib import Path
import bpy, json, hashlib, sys
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'Artifacts/D83/AcceptedSports/curves.json'
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'ArtSource/Blender/CityCitizenRigPrototype.blend'))
clips=('BaseballPitch','BaseballSwing','SoccerRun','SoccerKick','GoalkeeperSave')
records={}
for action in bpy.data.actions:
    if not any(action.name.endswith('_'+clip) for clip in clips):continue
    curves=[]
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    points=[dict(co=[round(c,6) for c in point.co],
                                 left=[round(c,6) for c in point.handle_left],
                                 right=[round(c,6) for c in point.handle_right],
                                 interpolation=point.interpolation) for point in curve.keyframe_points]
                    curves.append(dict(path=curve.data_path,index=curve.array_index,points=points))
    curves.sort(key=lambda c:(c['path'],c['index']))
    records[action.name]=hashlib.sha256(json.dumps(curves,sort_keys=True).encode()).hexdigest()
assert len(records)==30,len(records)
if '--capture' in sys.argv and not BASE.exists():
    BASE.parent.mkdir(parents=True,exist_ok=True)
    BASE.write_text(json.dumps(records,indent=2)+'\n')
assert records==json.loads(BASE.read_text()), 'Accepted baseball/soccer curves changed'
print('CITYSIM_D83_ACCEPTED_MOTION_PASS 30 actions')
