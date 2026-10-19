"""#192 nuclear-only CI preparation. Invoke only in #125's agreed window.

No catalog, map, runtime source or existing shared material/texture saves.
"""
from pathlib import Path
import hashlib
import json
import sys
import unreal

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'Tools'))
from Assets.Manifests.CityApprovedSources import verified_export, restore_approved_json_checkout
from CityBuildingAdoptionMaterials import create,node,scalar,color,link,prop,finish

SCOPE = '/Game/CitySim/Art/Utilities/Nuclear'
LIB = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def parameter(material,kind,name,value):
    expr=node(material,kind)
    expr.set_editor_property('parameter_name',name)
    expr.set_editor_property('default_value',value)
    return expr

def master(name):
    material=create(SCOPE+'/Materials/'+name,unreal.Material,unreal.MaterialFactoryNew())
    LIB.delete_all_material_expressions(material)
    material.set_editor_property('used_with_instanced_static_meshes',True)
    material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
    return material

def materials(doc):
    solid=master('M_UtilitySolid')
    tint=parameter(solid,unreal.MaterialExpressionVectorParameter,'BaseColor',unreal.LinearColor(.5,.5,.5,1))
    rough=parameter(solid,unreal.MaterialExpressionScalarParameter,'Roughness',.8)
    metal=parameter(solid,unreal.MaterialExpressionScalarParameter,'Metallic',0.)
    prop(tint,unreal.MaterialProperty.MP_BASE_COLOR)
    prop(rough,unreal.MaterialProperty.MP_ROUGHNESS)
    prop(metal,unreal.MaterialProperty.MP_METALLIC)
    stats={'M_UtilitySolid':finish(solid)}
    pbr=master('M_UtilityPBR')
    coord=node(pbr,unreal.MaterialExpressionTextureCoordinate)
    scale=parameter(pbr,unreal.MaterialExpressionScalarParameter,'UVScale',.5)
    uv=node(pbr,unreal.MaterialExpressionMultiply)
    link(coord,uv,'A');link(scale,uv,'B')
    samples={}
    for suffix,sampler in [('BaseColor',unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),('Normal',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),('ORM',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)]:
        texture=unreal.load_asset('/Game/CitySim/Art/Textures/T_CivicAsphalt_'+suffix)
        assert texture
        sample=node(pbr,unreal.MaterialExpressionTextureSampleParameter2D)
        sample.set_editor_property('parameter_name',suffix)
        sample.set_editor_property('texture',texture)
        sample.set_editor_property('sampler_type',sampler)
        # MaterialEditingLibrary matches the shortened graph pin name (UE5.7
        # MaterialGraphNode::GetShortenPinName maps Coordinates to UVs).
        link(uv,sample,'UVs');samples[suffix]=sample
    prop(samples['BaseColor'],unreal.MaterialProperty.MP_BASE_COLOR)
    prop(samples['ORM'],unreal.MaterialProperty.MP_ROUGHNESS,'G')
    prop(samples['ORM'],unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,'R')
    metal_weight=parameter(pbr,unreal.MaterialExpressionScalarParameter,'MetallicFromORM',0.)
    metallic=node(pbr,unreal.MaterialExpressionMultiply)
    link(samples['ORM'],metallic,'A','B');link(metal_weight,metallic,'B')
    prop(metallic,unreal.MaterialProperty.MP_METALLIC)
    normal=node(pbr,unreal.MaterialExpressionMultiply)
    link(samples['Normal'],normal,'A');link(color(pbr,(.35,.35,1)),normal,'B')
    prop(normal,unreal.MaterialProperty.MP_NORMAL)
    stats['M_UtilityPBR']=finish(pbr)
    bindings={}
    params=doc['material_contract']['material_parameters']
    for region in doc['material_regions']:
        destination=SCOPE+'/Materials/MI_Nuclear_'+region
        instance=create(destination,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        textured=region in ('RoofService','EquipmentMetal')
        LIB.set_material_instance_parent(instance,pbr if textured else solid)
        if textured:
            prefix='CivicAsphalt' if region=='RoofService' else 'CityMechanical'
            for suffix in ('BaseColor','Normal','ORM'):
                texture=unreal.load_asset('/Game/CitySim/Art/Textures/T_'+prefix+'_'+suffix)
                assert texture
                assert texture.get_editor_property('srgb')==(suffix=='BaseColor')
                expected=unreal.TextureCompressionSettings.TC_NORMALMAP if suffix=='Normal' else unreal.TextureCompressionSettings.TC_MASKS if suffix=='ORM' else unreal.TextureCompressionSettings.TC_DEFAULT
                assert texture.get_editor_property('compression_settings')==expected
                LIB.set_material_instance_texture_parameter_value(instance,suffix,texture)
            LIB.set_material_instance_scalar_parameter_value(instance,'UVScale',.5)
            LIB.set_material_instance_scalar_parameter_value(instance,'MetallicFromORM',1. if region=='EquipmentMetal' else 0.)
        else:
            info=params[region]
            LIB.set_material_instance_vector_parameter_value(instance,'BaseColor',unreal.LinearColor(*info['base_color']))
            LIB.set_material_instance_scalar_parameter_value(instance,'Roughness',info['roughness'])
            LIB.set_material_instance_scalar_parameter_value(instance,'Metallic',info['metallic'])
        assert unreal.EditorAssetLibrary.save_loaded_asset(instance)
        bindings[region]=destination
    return bindings,stats

def main():
    assert Path(unreal.Paths.project_dir()).resolve()==(ROOT/'CitySimVR').resolve()
    doc=json.loads((ROOT/'ArtSource/Manifests/city_nuclear_integration.json').read_text())
    checkout_repairs=restore_approved_json_checkout(ROOT/doc['approved_directory'],doc['approval_sha256'])
    verified_export(ROOT/doc['approved_directory'],153,6,doc['approval_sha256'],kind='blender')
    assert digest(ROOT/doc['source'])==doc['source_sha256']
    for lod in doc['lods']:
        assert digest(ROOT/lod['export'])==lod['export_sha256']
    # Existing shared textures are read-only inputs, including their import policy.
    textures=list((ROOT/'CitySimVR/Content/CitySim/Art/Textures').glob('T_CivicAsphalt_*.uasset'))+list((ROOT/'CitySimVR/Content/CitySim/Art/Textures').glob('T_CityMechanical_*.uasset'))
    assert len(textures)==6
    before={p:digest(p) for p in textures}
    bindings,stats=materials(doc)
    unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
    destination=doc['native_target'];assert destination==SCOPE+'/SM_NuclearFacility_v6'
    task=unreal.AssetImportTask()
    for key,value in dict(filename=str(ROOT/doc['lods'][0]['export']),destination_path=SCOPE,destination_name='SM_NuclearFacility_v6',automated=True,replace_existing=True,replace_existing_settings=True,save=False).items():
        task.set_editor_property(key,value)
    task.factory=unreal.FbxFactory()
    options=unreal.FbxImportUI()
    options.import_mesh,options.import_materials,options.import_textures,options.import_as_skeletal=True,False,False,False
    for key,value in dict(combine_meshes=True,vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,convert_scene_unit=True,convert_scene=True,import_uniform_scale=1.,generate_lightmap_u_vs=True,auto_generate_collision=False,normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():
        options.static_mesh_import_data.set_editor_property(key,value)
    task.options=options;TOOLS.import_asset_tasks([task])
    mesh=unreal.load_asset(destination);assert mesh
    slots=[str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')]
    assert len(slots)==5 and set(slots)==set(bindings)
    for index,slot in enumerate(slots):mesh.set_material(index,unreal.load_asset(bindings[slot]))
    subsystem=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.new_object(unreal.StaticMeshEditorSubsystem)
    for lod in doc['lods'][1:]:
        assert subsystem.import_lod(mesh,lod['lod'],str(ROOT/lod['export']))==lod['lod']
    subsystem.set_lod_screen_sizes(mesh,[1.,.12,.025])
    mesh.set_minimum_lod_for_platform('Android',1)
    for lod in doc['lods']:
        assert mesh.get_num_triangles(lod['lod'])==lod['triangles']
        assert mesh.get_num_sections(lod['lod'])==5
    assert len(mesh.get_editor_property('static_materials'))==5
    bounds=mesh.get_bounding_box();size=bounds.max-bounds.min
    assert max(abs(a-b*100) for a,b in zip((size.x,size.y,size.z),doc['lods'][0]['dimensions_m']))<.2
    assert abs(bounds.min.z)<.1
    assert [mesh.get_material(i).get_path_name().split('.')[0] for i in range(5)]==[bindings[s] for s in slots]
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    assert all(digest(p)==value for p,value in before.items())
    paths=[destination]+list(bindings.values())+[SCOPE+'/Materials/M_UtilitySolid',SCOPE+'/Materials/M_UtilityPBR']
    files=['CitySimVR/Content/'+p.removeprefix('/Game/')+'.uasset' for p in paths]
    assert all((ROOT/p).is_file() for p in files)
    output=ROOT/'Artifacts/CIAssetPreparation';output.mkdir(parents=True,exist_ok=True)
    listed=output/'Files.txt';previous=listed.read_text().splitlines() if listed.exists() else []
    listed.write_text('\n'.join(dict.fromkeys(previous+files))+'\n',encoding='utf-8')
    receipt=dict(issue=192,source_issue=153,source_version=6,approval_sha256=doc['approval_sha256'],native_target=destination,
                 files=[dict(file=p,sha256=digest(ROOT/p),bytes=(ROOT/p).stat().st_size) for p in files],
                 lod_triangles=[mesh.get_num_triangles(i) for i in range(3)],slots=slots,bindings=bindings,shader_statistics=stats,
                 shared_textures_unchanged=True,catalog_runtime_unchanged=True,headset_appearance='unconfirmed',
                 exact_json_checkout_repairs=checkout_repairs,
                 concrete_bump='Source procedural noise omitted in this initial shared solid material; review native appearance before acceptance',
                 lod_thresholds='Initial [1,.12,.025], Android minimum1; review transitions in integrated views')
    (output/'NuclearFacility.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
    print('NUCLEAR_FACILITY_IMPORT_PASS',receipt['lod_triangles'],len(files),'scoped packages')

if __name__=='__main__':
    main()
