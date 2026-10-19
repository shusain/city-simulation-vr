"""Render all D61 source meshes before export; leave the .blend untouched."""
from pathlib import Path
import bpy
from mathutils import Vector
root = Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(root/'ArtSource/Blender/CityWasteKit.blend'))
scene = bpy.context.scene
scene.render.engine = 'BLENDER_EEVEE'
scene.world.use_nodes = True
scene.world.node_tree.nodes['Background'].inputs['Color'].default_value = (.3,.35,.4,1)
scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value = .5
sun = bpy.data.objects.new('PreviewSun', bpy.data.lights.new('PreviewSun', 'SUN'))
scene.collection.objects.link(sun)
sun.data.energy = 2.5
sun.rotation_euler = (.5,-.6,-.3)
scene.render.resolution_x = scene.render.resolution_y = 640
scene.render.resolution_percentage = 100
scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'MATERIAL'
scene.display.shading.show_shadows = scene.display.shading.show_cavity = True
scene.display.shading.background_type = 'WORLD'
scene.world.color = (.06,.07,.09)
scene.view_settings.view_transform = 'Standard'
objects = [obj for obj in scene.objects if obj.type == 'MESH']
for obj in objects:
    obj.hide_render = True
camera = bpy.data.objects.new('Preview', bpy.data.cameras.new('Preview'))
scene.collection.objects.link(camera)
scene.camera = camera
camera.data.type = 'ORTHO'
out = root/'Artifacts/ArtPreview/D61'
out.mkdir(parents=True,exist_ok=True)
for obj in objects:
    building = 'Building' in obj.name
    camera.location = (38,-46,35) if building else (9,-11,8)
    aim = Vector((0,0,4 if building else 1))
    camera.rotation_euler = (aim-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.ortho_scale = 43 if building else 10
    obj.hide_render = False
    scene.render.filepath = str(out/(obj.name+'.png'))
    bpy.ops.render.render(write_still=True)
    obj.hide_render = True
