"""D88 render saved sports loops without changing the source."""
from pathlib import Path
import bpy,json,sys,hashlib
ROOT=Path(__file__).resolve().parents[1]
doc=json.loads((ROOT/'ArtSource/Manifests/city_citizen_sports_refinement.json').read_text())
SOURCE=ROOT/doc['source'];before=hashlib.sha256(SOURCE.read_bytes()).hexdigest()
OUT=ROOT/'Artifacts/D88/Preview';OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
jobs=doc['scenes']+[dict(scene=doc['baseball_pair_scene'],clip='BaseballPair',period=72)]
for record in jobs:
    s=bpy.data.scenes[record['scene']];bpy.context.window.scene=s
    if '--key-only' in sys.argv:
        frames=sorted(set((1,1+record['period']//4,record.get('impact_frame',record.get('release_frame',record.get('catch_frame',1+record['period']//2))),1+int(record['period']*.8))))
    else:frames=range(1,record['period']+1,2)
    for frame in frames:
        s.frame_set(frame);bpy.context.view_layer.update()
        s.render.filepath=str(OUT/f'{record["clip"]}-{frame:03}.png');bpy.ops.render.render(write_still=True)
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==before
print('CITYSIM_D88_RENDER_PASS',len(jobs),'scenes;source unchanged')
