"""Render a repeatable contact-sheet source set from the saved Blender kit."""
import bpy
import os
from mathutils import Vector
root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
out = os.path.join(root, "Artifacts", "ArtPreview", "D38")
os.makedirs(out, exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=os.path.join(root,"ArtSource","Blender","CityStarterKit.blend"))
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.render.resolution_x = 512
scene.render.resolution_y = 512
scene.render.resolution_percentage = 100
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "MATERIAL"
scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True
scene.display.shading.background_type = "WORLD"
scene.world.color = (0.055,0.065,0.085)
scene.view_settings.view_transform = "Standard"
bpy.data.materials['Roof'].diffuse_color = (0.10,0.14,0.19,1)
for obj in list(scene.objects):
    obj.hide_render = True
    # Match the manifest's dark detail slot for commercial/industrial facades.
    if 'Building_Shop' in obj.name or 'Building_Factory' in obj.name:
        for slot in obj.material_slots:
            if slot.material.name == 'Trim': slot.material = bpy.data.materials['Roof']
camdata = bpy.data.cameras.new('PreviewCamera')
camera = bpy.data.objects.new('PreviewCamera',camdata)
scene.collection.objects.link(camera)
scene.camera = camera
camera.location = (42,-58,44)
camera.rotation_euler = (Vector((0,0,17))-camera.location).to_track_quat('-Z','Y').to_euler()
camdata.type = 'ORTHO'
camdata.ortho_scale = 52
for family in ('Residential','Shop','Factory'):
    for density in ('','Medium_','High_'):
        name = 'SM_Building_'+family+'_'+density+'A'
        obj = bpy.data.objects[name]
        obj.hide_render = False
        scene.render.filepath = os.path.join(out,name+'.png')
        bpy.ops.render.render(write_still=True)
        obj.hide_render = True

# Construction assembly preview; transformations affect this preview session only.
site = bpy.data.objects['SM_Building_Residential_Construction_A']
jib = bpy.data.objects['SM_Construction_Crane_Jib_A']
hook = bpy.data.objects['SM_Construction_Crane_Hook_A']
site.hide_render = jib.hide_render = hook.hide_render = False
jib.location.z = 20
hook.location = (6, 0, 17)
camera.location = (32,-42,30)
camera.rotation_euler = (Vector((0,0,10))-camera.location).to_track_quat('-Z','Y').to_euler()
camdata.ortho_scale = 32
# Preview the runtime pooled cable joining jib and hook.
bpy.ops.mesh.primitive_cube_add(location=(6,0,18.5))
cable = bpy.context.object
cable.dimensions = (.12,.12,3.0)
cable.data.materials.append(bpy.data.materials['Roof'])
scene.render.filepath = os.path.join(out,'Construction.png')
bpy.ops.render.render(write_still=True)

site.hide_render = jib.hide_render = hook.hide_render = cable.hide_render = True
camera.location = (30,-40,32)
camera.rotation_euler = (Vector((0,0,7))-camera.location).to_track_quat('-Z','Y').to_euler()
camdata.ortho_scale = 28
for name in ('SM_Building_Power_A', 'SM_Building_Service_A'):
    obj = bpy.data.objects[name]
    obj.hide_render = False
    scene.render.filepath = os.path.join(out, name+'.png')
    bpy.ops.render.render(write_still=True)
    obj.hide_render = True
