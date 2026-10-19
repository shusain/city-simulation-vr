"""Paired D104 Blender previews; reuse approved D103 references verbatim."""
from pathlib import Path
import argparse
import json
import shutil
import sys
import bpy
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'Tools'))
from CityUnrealPreview import load_batch, digest
from CityAssetPreview import studio_lights


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--batch', type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    batch = load_batch(args.batch); out = ROOT/batch['output']
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/batch['source']))
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'; scene.cycles.samples = 24; scene.cycles.use_denoising = True
    scene.render.resolution_x = scene.render.resolution_y = batch['resolution']; scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'; scene.view_settings.view_transform = 'AgX'
    scene.world.use_nodes = True
    scene.world.node_tree.nodes['Background'].inputs['Color'].default_value = (.20, .25, .33, 1)
    scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value = .65
    cam = bpy.data.objects.new('D104_ReviewCamera', bpy.data.cameras.new('D104_ReviewCamera'))
    scene.collection.objects.link(cam); scene.camera = cam; cam.data.type = 'ORTHO'
    studio_lights(scene, [Vector((10, 10, 10))])
    bpy.ops.mesh.primitive_plane_add(size=160)
    ground = bpy.context.object
    m = bpy.data.materials.new('D104_Ground'); m.use_nodes = True
    shader = m.node_tree.nodes['Principled BSDF']; shader.inputs['Base Color'].default_value = (.16, .19, .23, 1)
    shader.inputs['Roughness'].default_value = .85; ground.data.materials.append(m)
    records = []; fresh = 0
    for entry in batch['renders']:
        directory = out/'Blender'/entry['group']/entry['view']; directory.mkdir(parents=True, exist_ok=True)
        path = directory/(entry['key']+'.png')
        if not entry['overrides']:
            prior_dir = ROOT/'Artifacts/D103/Revision3/Preview'/entry['view']
            prior = json.loads((prior_dir/'PreviewManifest.json').read_text())
            old = next(e for e in prior['views'] if e['object'] == entry['source_object'])
            assert digest(prior_dir/old['image']) == old['sha256']
            assert prior['source_sha256'] == batch['approved_source_sha256']
            shutil.copyfile(prior_dir/old['image'], path)
        else:
            obj = bpy.data.objects[entry['source_object']]; citizen = bpy.data.objects['Ruler_'+obj.name]
            location = obj.location.copy(); offset = citizen.location-obj.location
            obj.location = (0, 0, 0); citizen.location = offset
            ground.location.z = entry['floor_z_m']
            for mesh in scene.objects:
                if mesh.type == 'MESH': mesh.hide_render = mesh not in (obj, citizen, ground)
            cam.location = entry['camera']['position_m']; aim = Vector(entry['camera']['target_m'])
            cam.rotation_euler = (aim-cam.location).to_track_quat('-Z', 'Y').to_euler()
            cam.data.ortho_scale = entry['camera']['ortho_width_m']; bpy.context.view_layer.update()
            projected = [world_to_camera_view(scene, cam, obj.matrix_world@v.co) for v in obj.data.vertices]
            assert all(.015 <= v.x <= .985 and .015 <= v.y <= .985 and v.z > 0 for v in projected)
            scene.render.filepath = str(path); bpy.ops.render.render(write_still=True); fresh += 1
            obj.location = location; citizen.location = location+offset
        records.append(dict(**entry, image=path.name, sha256=digest(path), reused_approved_reference=not bool(entry['overrides'])))
    assert digest(ROOT/batch['source']) == batch['source_sha256']
    for group, view in sorted({(e['group'], e['view']) for e in records}):
        rows = [e for e in records if e['group'] == group and e['view'] == view]
        (out/'Blender'/group/view/'PreviewManifest.json').write_text(json.dumps(dict(schema_version=1,
            source=batch['source'], source_sha256=batch['source_sha256'], source_unchanged=True,
            scene='D103 studio cameras', frame=1, engine='cycles', renderer_label='Blender',
            resolution=batch['resolution'], views=rows, limitations=batch['limitations']), indent=2)+'\n')
    (out/'Blender/Report.json').write_text(json.dumps(dict(views=records, fresh_renders=fresh,
        approved_references_reused=len(records)-fresh, source_sha256=batch['source_sha256']), indent=2)+'\n')
    print('CITYSIM_D104_BLENDER_PREVIEWS_PASS', len(records), 'views;', fresh, 'fresh material renders')


if __name__ == '__main__': main()
