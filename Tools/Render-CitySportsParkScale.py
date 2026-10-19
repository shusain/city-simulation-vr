"""Read-only Blender comparison of goals and whole field at actual keeper position."""
from pathlib import Path
import bpy,json,hashlib
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D88/Scale/Preview';OUT.mkdir(parents=True,exist_ok=True)
doc=json.loads((ROOT/'ArtSource/Manifests/city_citizen_sports_scale.json').read_text());source=ROOT/doc['source']
assert hashlib.sha256(source.read_bytes()).hexdigest()==doc['source_sha256']
bpy.ops.wm.open_mainfile(filepath=str(source))
for name in ('Goal-Before','Goal-After','Soccer-Field'):
    s=bpy.data.scenes[name];bpy.context.window.scene=s;s.render.filepath=str(OUT/(name+'.png'));bpy.ops.render.render(write_still=True)
s=bpy.data.scenes['D88_BaseballPair'];bpy.context.window.scene=s;s.frame_set(49)
s.render.resolution_x=1200;s.render.resolution_y=850;s.camera.data.ortho_scale=24
s.render.filepath=str(OUT/'Baseball-Diamond.png');bpy.ops.render.render(write_still=True)
assert hashlib.sha256(source.read_bytes()).hexdigest()==doc['source_sha256']
print('CITYSIM_D88_SCALE_RENDER_PASS 4 source views')
