"""Render the saved civic source kit without modifying it."""
from pathlib import Path
import bpy
from mathutils import Vector
root = Path(__file__).resolve().parents[1]
out = root / 'Artifacts/ArtPreview/D46'
out.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(root / 'ArtSource/Blender/CitySchoolKit.blend'))
scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
scene.render.resolution_x = scene.render.resolution_y = 512
scene.render.resolution_percentage = 100
scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'MATERIAL'
# Color ordinary faces by material, while retaining the authored roof-cap RGB.
for obj in scene.objects:
    if obj.type != 'MESH': continue
    colors=obj.data.color_attributes.get('WindowMask')
    if not colors: continue
    for poly in obj.data.polygons:
        for index in poly.loop_indices:
            if colors.data[index].color[3] > .5: colors.data[index].color=obj.data.materials[poly.material_index].diffuse_color
scene.display.shading.show_shadows = scene.display.shading.show_cavity = True
scene.display.shading.background_type = 'WORLD'
scene.world.color = (.06,.07,.09)
scene.view_settings.view_transform = 'Standard'
objects = list(scene.objects)
for obj in objects: obj.hide_render = True
cam = bpy.data.cameras.new('CivicPreviewCamera')
camera = bpy.data.objects.new('CivicPreviewCamera',cam)
scene.collection.objects.link(camera)
scene.camera = camera
cam.type = 'ORTHO'
for obj in objects:
    building = 'Building' in obj.name
    camera.location = (32,-43,30) if building else (8,-10,7)
    aim = Vector((0,0,4 if building else 1))
    camera.rotation_euler = (aim-camera.location).to_track_quat('-Z','Y').to_euler()
    cam.ortho_scale = 34 if building else 9
    obj.hide_render = False
    scene.render.filepath = str(out / (obj.name+'.png'))
    bpy.ops.render.render(write_still=True)
    obj.hide_render = True
