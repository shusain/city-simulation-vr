"""D121 source-only front/side/all-outfit and translated foot-contact views."""
from pathlib import Path
import hashlib
import json
import math
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
DOC = json.loads((ROOT/'ArtSource/Manifests/city_citizen_walk_cycle_v2.json').read_text())
SOURCE = ROOT/DOC['source']
OUT = ROOT/'Artifacts/D121/V2/Preview'
OUT.mkdir(parents=True, exist_ok=True)
before = hashlib.sha256(SOURCE.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
if bpy.context.object and bpy.context.object.mode != 'OBJECT': bpy.ops.object.mode_set(mode='OBJECT')
scene = bpy.context.scene; camera = scene.camera
period = DOC['period_frames']
views = {}


def setup(view):
    for record in DOC['records']:
        mesh = bpy.data.objects[record['mesh']]; rig = mesh.parent
        mesh.hide_render = record['lod'] != 0 or (view != 'Outfits' and record['style'] != 'Casual')
        mesh.hide_set(False); rig.hide_set(False)
        rig.location = (0, {'Casual':-1.1,'Office':0,'Worker':1.1}[record['style']] if view == 'Outfits' else 0, 0)
    aim = Vector((0,0,.73))
    if view == 'Outfits': offset=(6,-7,2.8); width=4.4
    elif view == 'Front': offset=(7,0,1.0); width=2.5
    elif view == 'Travel': offset=(0,-8,1.0); width=4.8
    else: offset=(0,-8,1.0); width=2.5
    camera.location=aim+Vector(offset)
    camera.rotation_euler=(aim-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.ortho_scale=width


# Fixed ground lines make stance contact visible during the translating preview.
for x in range(-4,5):
    bpy.ops.mesh.primitive_cube_add(size=1,location=(x*.5,0,-.001))
    stripe=bpy.context.object;stripe.name='D121_GroundReference_'+str(x)
    stripe.scale=(.008,4,.001)
    mat=bpy.data.materials.get('D121_Stripe')
    if mat is None:
        mat=bpy.data.materials.new('D121_Stripe');mat.diffuse_color=(.30,.36,.40,1)
    stripe.data.materials.append(mat)
for view in ('Outfits','Side','Front','Travel'):
    setup(view)
    count=period*3 if view=='Travel' else period
    filenames=[]
    for index in range(0,count,2):
        frame=1+index%period
        scene.frame_set(frame)
        if view=='Travel':
            rig=bpy.data.objects['SK_Citizen_Rig_Casual'].parent
            rig.location.x=DOC['stride_preview_m']*(index/period-1.5)
        bpy.context.view_layer.update()
        path=OUT/f'{view}-{index:03}.png'
        scene.render.filepath=str(path)
        bpy.ops.render.render(write_still=True)
        filenames.append(path.name)
    views[view]=dict(files=filenames,frame_step=2,fps=DOC['fps'],
                     translation='constant forward carrier; no animation root motion' if view=='Travel' else 'in place')
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==before
(OUT/'RenderManifest.json').write_text(json.dumps(dict(task='D121',version=2,source_sha256=before,
    views=views,renderer='Blender Workbench source review; not in-game',source_unchanged=True),indent=2)+'\n')
print('CITYSIM_D121_RENDER_PASS',sum(len(v['files']) for v in views.values()),'source frames')
