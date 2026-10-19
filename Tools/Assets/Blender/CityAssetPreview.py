"""Reusable Blender mesh preview helpers; importing creates no scene or files."""
from pathlib import Path
import hashlib
import math
import bpy
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view

VIEWS = {
    'iso': (1, -1.5, 1), 'front': (0, -1, 0), 'back': (0, 1, 0),
    'left': (-1, 0, 0), 'right': (1, 0, 0), 'top': (0, 0, 1),
}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def preview_scene(resolution, engine, color):
    scene = bpy.data.scenes.new('CityAssetPreview')
    scene.render.engine = 'BLENDER_WORKBENCH' if engine == 'workbench' else 'BLENDER_EEVEE'
    scene.render.resolution_x = scene.render.resolution_y = resolution
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGB'
    scene.render.film_transparent = False
    scene.render.use_compositing = False
    scene.render.use_sequencer = False
    scene.view_settings.view_transform = 'Standard'
    scene.world = bpy.data.worlds.new('CityAssetPreviewWorld')
    scene.world.color = (.065, .08, .105)
    if engine == 'eevee':
        scene.world.use_nodes = True
        scene.world.node_tree.nodes['Background'].inputs['Color'].default_value = (.065, .08, .105, 1)
        scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value = .6
    shading = scene.display.shading
    shading.light = 'STUDIO'
    shading.color_type = color.upper()
    shading.show_shadows = True
    shading.show_cavity = True
    shading.background_type = 'WORLD'
    camera = bpy.data.objects.new('CityAssetPreviewCamera', bpy.data.cameras.new('CityAssetPreviewCamera'))
    scene.collection.objects.link(camera)
    scene.camera = camera
    camera.data.type = 'ORTHO'
    return scene


def evaluated_copy(obj, source_scene, destination):
    """Bake the current evaluated mesh/pose while retaining world transforms."""
    bpy.context.window.scene = source_scene
    bpy.context.view_layer.update()
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = obj.evaluated_get(depsgraph)
    mesh = bpy.data.meshes.new_from_object(evaluated, preserve_all_data_layers=True, depsgraph=depsgraph)
    if not mesh.vertices:
        bpy.data.meshes.remove(mesh)
        raise ValueError('Mesh has no evaluated vertices: ' + obj.name)
    copy = bpy.data.objects.new('Preview_' + obj.name, mesh)
    copy.matrix_world = evaluated.matrix_world.copy()
    copy.color = obj.color
    destination.collection.objects.link(copy)
    points = [copy.matrix_world @ v.co for v in mesh.vertices]
    if any(not math.isfinite(value) for point in points for value in point):
        raise ValueError('Non-finite evaluated bounds: ' + obj.name)
    low = Vector(tuple(min(v[i] for v in points) for i in range(3)))
    high = Vector(tuple(max(v[i] for v in points) for i in range(3)))
    center = (low + high) / 2
    copy.matrix_world.translation -= center
    return copy, [p - center for p in points], list(high - low)


def fit_camera(scene, points, view):
    direction = Vector(VIEWS[view]).normalized()
    camera = scene.camera
    rotation = (-direction).to_track_quat('-Z', 'Y')
    projected = [rotation.inverted() @ p for p in points]
    low = Vector(tuple(min(p[i] for p in projected) for i in range(3)))
    high = Vector(tuple(max(p[i] for p in projected) for i in range(3)))
    span = high - low
    aspect = scene.render.resolution_x / scene.render.resolution_y
    camera.data.ortho_scale = max(span.y, span.x / aspect, .001) * 1.2
    radius = max(p.length for p in points)
    distance = max(radius * 4, .01)
    aim = rotation @ Vector(((low.x + high.x) / 2, (low.y + high.y) / 2, 0))
    camera.rotation_euler = rotation.to_euler()
    camera.location = aim + direction * distance
    camera.data.clip_start = max(distance * .0001, .000001)
    camera.data.clip_end = distance + max(radius * 4, .1)
    bpy.context.view_layer.update()
    ndc = [world_to_camera_view(scene, camera, p) for p in points]
    assert all(.01 <= p.x <= .99 and .01 <= p.y <= .99 and p.z > 0 for p in ndc), 'Camera failed to fit mesh'
    return dict(ortho_scale=camera.data.ortho_scale,
                projected_bounds=[[min(p.x for p in ndc), min(p.y for p in ndc)],
                                  [max(p.x for p in ndc), max(p.y for p in ndc)]])


def studio_lights(scene, points):
    radius = max(max(p.length for p in points), .01)
    lights = []
    for label, direction, power in [('Key', (2, -3, 4), 1200), ('Fill', (-3, -1, 2), 600), ('Rim', (1, 3, 3), 900)]:
        data = bpy.data.lights.new('Preview' + label, 'AREA')
        data.energy = power * (radius / 2) ** 2
        data.shape = 'DISK'
        data.size = radius * 2
        obj = bpy.data.objects.new(data.name, data)
        obj.location = Vector(direction).normalized() * radius * 4
        obj.rotation_euler = (-obj.location).to_track_quat('-Z', 'Y').to_euler()
        scene.collection.objects.link(obj)
        lights.append(obj)
    return lights


def render_asset(obj, source_scene, scene, destination, views, engine):
    copy, points, dimensions = evaluated_copy(obj, source_scene, scene)
    bpy.context.window.scene = scene
    lights = studio_lights(scene, points) if engine == 'eevee' else []
    rows = []
    try:
        copy.data.calc_loop_triangles()
        for view in views:
            fit = fit_camera(scene, points, view)
            path = destination(view)
            scene.render.filepath = str(path)
            bpy.ops.render.render(write_still=True)
            rows.append(dict(view=view, image=path.name, sha256=digest(path), dimensions_m=[d * source_scene.unit_settings.scale_length for d in dimensions],
                             triangles=len(copy.data.loop_triangles), camera_fit=fit))
    finally:
        mesh = copy.data
        bpy.data.objects.remove(copy, do_unlink=True)
        bpy.data.meshes.remove(mesh)
        for light in lights:
            data = light.data
            bpy.data.objects.remove(light, do_unlink=True)
            bpy.data.lights.remove(data)
        bpy.context.window.scene = source_scene
    return rows
