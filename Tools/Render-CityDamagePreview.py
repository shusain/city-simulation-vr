from pathlib import Path
import bpy
from mathutils import Vector
root=Path(__file__).resolve().parents[1]; out=root/'Artifacts/ArtPreview/D44Damage'; out.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(root/'ArtSource/Blender/CityDamageKit.blend'))
scene=bpy.context.scene; scene.render.engine='BLENDER_WORKBENCH'; scene.render.resolution_x=scene.render.resolution_y=512
scene.display.shading.light='STUDIO'; scene.display.shading.color_type='MATERIAL'; scene.display.shading.show_cavity=True
scene.display.shading.background_type='WORLD'; scene.world.color=(.15,.18,.21)
scene.view_settings.view_transform='Standard'
objs=list(scene.objects)
for o in objs: o.hide_render=True
cam=bpy.data.cameras.new('DamagePreview'); camera=bpy.data.objects.new('DamagePreview',cam); scene.collection.objects.link(camera); scene.camera=camera; cam.type='ORTHO'
for o in objs:
    o.hide_render=False; aim=o.location+Vector((0,0,o.dimensions.z*.4))
    camera.location=aim+Vector((32,-43,30)); camera.rotation_euler=(aim-camera.location).to_track_quat('-Z','Y').to_euler()
    cam.ortho_scale=max(32,o.dimensions.z*1.6); scene.render.filepath=str(out/(o.name+'.png')); bpy.ops.render.render(write_still=True); o.hide_render=True
