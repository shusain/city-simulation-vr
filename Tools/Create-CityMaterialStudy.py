"""Separate D104 Blender material copies and temporary Unreal preview bridge."""
from pathlib import Path
import argparse
import copy
import json
import sys
import bpy
import bmesh
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'Tools'))
from CityBuildingStudyIO import checked_fbx, snapshot
from CityUnrealPreview import digest, camera


def native_winding(obj, source_name):
    """Fix the transposed gable/single veranda plane only on a bridge copy.

    D103's across=True swaps axes (negative determinant), without reversing
    winding. Blender renders these backfaces; Unreal's opaque roof culls them.
    Preserve positions and per-corner UV/RGBA while reversing those six faces.
    """
    if source_name != 'Home_D_Veranda': return 0
    mesh = bmesh.new(); mesh.from_mesh(obj.data)
    faces = [f for f in mesh.faces if f.material_index in (1, 2)
             and all(v.co.z >= 2.37 for v in f.verts)]
    assert len(faces) == 6, 'Expected only five transposed gable faces and one veranda plane'
    for face in faces: face.normal_flip()
    mesh.normal_update(); mesh.to_mesh(obj.data); mesh.free(); obj.data.update()
    return len(faces)


def material(name, surface, tint, metallic):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nodes = m.node_tree.nodes; links = m.node_tree.links
    shader = nodes.get('Principled BSDF')
    textures = {}
    for role, record in surface['maps'].items():
        image = bpy.data.images.load(str(ROOT/record['file']), check_existing=True)
        image.colorspace_settings.name = 'sRGB' if role == 'BaseColor' else 'Non-Color'
        texture = nodes.new('ShaderNodeTexImage'); texture.image = image
        textures[role] = texture
    mix = nodes.new('ShaderNodeVectorMath'); mix.operation = 'MULTIPLY'
    mix.inputs[1].default_value = tint
    links.new(textures['BaseColor'].outputs['Color'], mix.inputs[0])
    links.new(mix.outputs['Vector'], shader.inputs['Base Color'])
    channels = nodes.new('ShaderNodeSeparateColor')
    links.new(textures['ORM'].outputs['Color'], channels.inputs['Color'])
    links.new(channels.outputs['Green'], shader.inputs['Roughness'])
    shader.inputs['Metallic'].default_value = metallic
    # DirectX -> Blender's OpenGL normal convention, with no height geometry.
    split = nodes.new('ShaderNodeSeparateColor'); combine = nodes.new('ShaderNodeCombineColor')
    links.new(textures['Normal'].outputs['Color'], split.inputs['Color'])
    links.new(split.outputs['Red'], combine.inputs['Red']); links.new(split.outputs['Blue'], combine.inputs['Blue'])
    inv = nodes.new('ShaderNodeMath'); inv.operation = 'SUBTRACT'; inv.inputs[0].default_value = 1
    links.new(split.outputs['Green'], inv.inputs[1]); links.new(inv.outputs[0], combine.inputs['Green'])
    normal = nodes.new('ShaderNodeNormalMap'); links.new(combine.outputs['Color'], normal.inputs['Color'])
    links.new(normal.outputs['Normal'], shader.inputs['Normal'])
    return m


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'Artifacts/D104')
    parser.add_argument('--source-file', type=Path, default=ROOT/'ArtSource/Blender/Studies/CityBuildingMaterials.blend')
    parser.add_argument('--manifest-file', type=Path, default=ROOT/'ArtSource/Manifests/city_building_material_study.json')
    parser.add_argument('--textures', type=Path, default=ROOT/'ArtSource/Textures/MaterialStudy/Materials.json')
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    assert out.is_relative_to(ROOT/'Artifacts'), 'Temporary study bridge belongs in Artifacts'
    doc = json.loads((ROOT/'ArtSource/Manifests/city_low_density_variants_study.json').read_text())
    assert digest(ROOT/doc['source']) == doc['source_sha256']
    source = args.source_file.resolve(); manifest = args.manifest_file.resolve()
    assert source.is_relative_to(ROOT/'ArtSource/Blender/Studies') and source.suffix == '.blend'
    assert manifest.is_relative_to(ROOT/'ArtSource/Manifests') and manifest.suffix == '.json'
    assert source != (ROOT/doc['source']).resolve(), 'Approved geometry source is read-only'
    catalog = json.loads(args.textures.read_text())
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/doc['source']))
    meshes = []; renders = []; options = []
    exports = out/'PreviewBridge'; exports.mkdir(exist_ok=True)
    source_records = copy.deepcopy(doc['assets'])
    for record in source_records:
        obj = bpy.data.objects[record['name']]
        before = snapshot(obj)
        bridge = obj.copy(); bridge.data = obj.data.copy()
        bpy.context.scene.collection.objects.link(bridge); bridge.location = (0, 0, 0)
        winding_fix = native_winding(bridge, obj.name)
        export = exports/(obj.name+'.fbx')
        with checked_fbx(bridge, export, record['slots'], verify_alpha=True): pass
        bridge_data = bridge.data; bpy.data.objects.remove(bridge, do_unlink=True); bpy.data.meshes.remove(bridge_data)
        assert snapshot(obj) == before
        meshes.append(dict(**record, export=export.relative_to(ROOT).as_posix(), export_sha256=digest(export),
                           native_mesh='/Game/CityPreview/Meshes/'+obj.name, native_winding_corrected_faces=winding_fix))
        for view in ('iso', 'front', 'back', 'entrance'):
            if view == 'entrance' and not record['entrance']: continue
            renders.append(dict(key=obj.name+'_'+view, object=obj.name, source_object=obj.name,
                group='Variants', view=view, category=record['category'], native_mesh=meshes[-1]['native_mesh'],
                dimensions_m=record['dimensions_m'], triangles=record['triangles'], camera=camera(record, view),
                floor_z_m=record['bounds_min_m'][2]-.015, ruler_front_m=record['bounds_min_m'][1]-.6,
                ruler_feet_m=record['bounds_min_m'][2], overrides={}))
    for row, (category, source_name) in enumerate([('Home', 'Home_C_CrossGable'),
            ('Shop', 'Shop_C_SteppedFront'), ('Factory', 'Factory_C_TwinGable')]):
        record = next(r for r in meshes if r['name'] == source_name)
        obj = bpy.data.objects[source_name]
        wall = {'Home': 'ResidentialBrick', 'Shop': 'CommercialBrick', 'Factory': 'IndustrialBrick'}[category]
        for col in range(4):
            label = 'Brick reference'; name = category+'_A_Brick'; overrides = {}
            source_object = source_name
            if col:
                surface, label, tint, metallic = catalog['options'][category][col-1]
                material_name = category+'_'+surface
                m = material(material_name, catalog['surfaces'][surface], tint, metallic)
                name = category+'_'+chr(65+col)+'_'+surface
                variant = obj.copy(); variant.data = obj.data.copy(); variant.name = name
                bpy.context.scene.collection.objects.link(variant)
                for i, slot in enumerate(record['slots']):
                    if slot == wall: variant.data.materials[i] = m
                # Do not replace home brick chimney/detail alpha material or signs/glass/trim.
                before, after = snapshot(obj), snapshot(variant)
                assert before == after, 'Only material bindings may change'
                variant.location = (col*26, -100-row*32, 0)
                citizen = bpy.data.objects['Ruler_'+source_name].copy()
                bpy.context.scene.collection.objects.link(citizen); citizen.name = 'Ruler_'+name
                citizen.location = variant.location+(bpy.data.objects['Ruler_'+source_name].location-obj.location)
                native = '/Game/CityPreview/Materials/M_'+material_name
                overrides[wall] = native; source_object = variant.name
                options.append(dict(name=material_name, native_material=native, surface=surface,
                                    tint=tint, metallic=metallic, target_slot=wall, label=label))
            for view in ('iso', 'front'):
                renders.append(dict(key=name+'_'+view, object=name, source_object=source_object,
                    group='Materials', label=label, view=view, category=category, native_mesh=record['native_mesh'],
                    dimensions_m=record['dimensions_m'], triangles=record['triangles'], camera=camera(record, view),
                    floor_z_m=record['bounds_min_m'][2]-.015, ruler_front_m=record['bounds_min_m'][1]-.6,
                    ruler_feet_m=record['bounds_min_m'][2], overrides=overrides))
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(source))
    assert digest(ROOT/doc['source']) == doc['source_sha256']
    batch = dict(schema_version=1, task='D104', project=(out/'UnrealProject/CityPreview.uproject').relative_to(ROOT).as_posix(),
        output=out.relative_to(ROOT).as_posix(), source=source.relative_to(ROOT).as_posix(), source_sha256=digest(source),
        approved_source=doc['source'], approved_source_sha256=doc['source_sha256'],
        resolution=800, meshes=meshes, materials=options, textures=catalog['surfaces'],
        material_mapping=doc['material_contracts'], renders=renders,
        lighting=dict(sun_intensity=3, sky_intensity=1, exposure_bias=0),
        preview_environment=catalog.get('preview_environment'),
        limitations='Matched geometry/cameras/ruler. Blender Cycles AgX studio vs Unreal D3D11 forward daylight; lighting/tonemappers differ. No production/game/device changes.')
    (out/'Batch.json').write_text(json.dumps(batch, indent=2)+'\n')
    manifest.write_text(json.dumps(batch, indent=2)+'\n')
    print('CITYSIM_D104_STUDY_PASS 12 temporary mesh bridges; 9 material variants; 68 paired views; approved source unchanged')


if __name__ == '__main__': main()
