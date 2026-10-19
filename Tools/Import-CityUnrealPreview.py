"""Import the explicit meshes/PBR options from a batch into its isolated UE project."""
from pathlib import Path
import json
import re
import sys
import unreal

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'Tools'))
from CityUnrealPreview import load_batch, digest, environment


def batch_argument():
    match = re.search(r'-CityPreviewBatch=(?:"([^"]+)"|(\S+))', unreal.SystemLibrary.get_command_line())
    assert match, 'Pass -CityPreviewBatch=<Batch.json>'
    return Path(match.group(1) or match.group(2))


def main():
    batch = load_batch(batch_argument()); out = ROOT/batch['output']
    assert Path(unreal.Paths.project_dir()).resolve() == (ROOT/batch['project']).parent.resolve()
    environment(batch, refresh=True)
    lib = unreal.MaterialEditingLibrary; assets = unreal.AssetToolsHelpers.get_asset_tools()

    def create(path, kind, factory):
        return unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(path.rsplit('/', 1)[1], path.rsplit('/', 1)[0], kind, factory)

    def save(obj): assert unreal.EditorAssetLibrary.save_loaded_asset(obj)
    def node(m, kind): return lib.create_material_expression(m, kind)
    def link(a, b, pin, output=''): assert lib.connect_material_expressions(a, output, b, pin)
    def prop(a, p, output=''): assert lib.connect_material_property(a, output, p)

    textures = {}
    for name, surface in batch['textures'].items():
        textures[name] = {}
        for role, record in surface['maps'].items():
            path = ROOT/record['file']; assert digest(path) == record['sha256']
            task = unreal.AssetImportTask()
            for key, value in dict(filename=str(path), destination_path='/Game/CityPreview/Textures',
                    destination_name='T_'+name+'_'+role, automated=True, replace_existing=True, save=False).items():
                task.set_editor_property(key, value)
            task.factory = unreal.TextureFactory(); assets.import_asset_tasks([task])
            texture = unreal.load_asset('/Game/CityPreview/Textures/T_'+name+'_'+role); assert texture
            texture.set_editor_property('srgb', role == 'BaseColor')
            texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP if role == 'Normal' else unreal.TextureCompressionSettings.TC_MASKS if role == 'ORM' else unreal.TextureCompressionSettings.TC_DEFAULT)
            texture.set_editor_property('address_x', unreal.TextureAddress.TA_WRAP)
            texture.set_editor_property('address_y', unreal.TextureAddress.TA_WRAP)
            if role == 'Normal': texture.set_editor_property('flip_green_channel', False)
            save(texture); textures[name][role] = texture
    stats = {}
    for option in batch['materials']:
        m = create(option['native_material'], unreal.Material, unreal.MaterialFactoryNew()); assert m
        lib.delete_all_material_expressions(m)
        m.set_editor_property('used_with_instanced_static_meshes', True)
        m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
        samplers = {}
        for role, texture in textures[option['surface']].items():
            n = node(m, unreal.MaterialExpressionTextureSample); n.set_editor_property('texture', texture)
            n.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if role == 'BaseColor' else unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if role == 'Normal' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
            samplers[role] = n
        tint = node(m, unreal.MaterialExpressionConstant3Vector); tint.set_editor_property('constant', unreal.LinearColor(*option['tint'], 1))
        multiply = node(m, unreal.MaterialExpressionMultiply)
        link(tint, multiply, 'A'); link(samplers['BaseColor'], multiply, 'B')
        prop(multiply, unreal.MaterialProperty.MP_BASE_COLOR)
        prop(samplers['Normal'], unreal.MaterialProperty.MP_NORMAL)
        prop(samplers['ORM'], unreal.MaterialProperty.MP_ROUGHNESS, 'G')
        prop(samplers['ORM'], unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, 'R')
        metal = node(m, unreal.MaterialExpressionConstant); metal.set_editor_property('r', option['metallic'])
        prop(metal, unreal.MaterialProperty.MP_METALLIC)
        lib.layout_material_expressions(m); lib.recompile_material(m); save(m)
        s = lib.get_statistics(m)
        stats[option['name']] = {k: s.get_editor_property(k) for k in ('num_pixel_shader_instructions', 'num_vertex_shader_instructions', 'num_samplers')}
        assert stats[option['name']]['num_pixel_shader_instructions'] > 0
    records = []
    for e in batch['meshes']:
        path = ROOT/e['export']; assert digest(path) == e['export_sha256']
        task = unreal.AssetImportTask()
        for k, v in dict(filename=str(path), destination_path='/Game/CityPreview/Meshes', destination_name=e['name'],
                automated=True, replace_existing=True, replace_existing_settings=True, save=False).items(): task.set_editor_property(k, v)
        task.factory = unreal.FbxFactory(); options = unreal.FbxImportUI()
        options.import_mesh = True; options.import_materials = False; options.import_textures = False; options.import_as_skeletal = False
        options.set_editor_property('reset_to_fbx_on_material_conflict', True)
        data = options.static_mesh_import_data
        for k, v in dict(combine_meshes=True, vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,
                convert_scene_unit=True, auto_generate_collision=False, normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items(): data.set_editor_property(k, v)
        existing = unreal.load_asset(e['native_mesh']) if unreal.EditorAssetLibrary.does_asset_exist(e['native_mesh']) else None
        if existing: existing.get_editor_property('asset_import_data').set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
        task.options = options; assets.import_asset_tasks([task]); mesh = unreal.load_asset(e['native_mesh']); assert mesh
        slots = [str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')]
        assert set(slots) == set(e['slots']), (slots, e['slots'])
        for i, slot in enumerate(slots):
            mat = unreal.load_asset(batch['material_mapping'][e['category']][slot]); assert mat, slot
            mesh.set_material(i, mat)
        assert mesh.get_num_triangles(0) == e['triangles'] and mesh.get_num_sections(0) == e['sections']
        b = mesh.get_bounding_box(); size = b.max-b.min
        assert all(abs(a-c*100) < .2 for a, c in zip((size.x, size.y, size.z), e['dimensions_m']))
        assert abs(b.min.z-e['bounds_min_m'][2]*100) < .2
        assert mesh.get_editor_property('asset_import_data').get_editor_property('vertex_color_import_option') == unreal.VertexColorImportOption.REPLACE
        save(mesh); records.append(dict(name=e['name'], native_mesh=e['native_mesh'], triangles=mesh.get_num_triangles(0),
            sections=mesh.get_num_sections(0), dimensions_cm=[size.x, size.y, size.z], slots=slots))
    (out/'Import.json').write_text(json.dumps(dict(task=batch['task'], meshes=records, shader_statistics=stats,
        source_sha256=batch['source_sha256'], production_changed=False), indent=2)+'\n')
    unreal.log('CITYSIM_UNREAL_PREVIEW_IMPORT_PASS '+str(len(records))+' meshes; '+str(len(stats))+' material options')


if __name__ == '__main__': main()
