"""Import the approved low-density source, surfaces and presentation-only catalog."""
from pathlib import Path
import json, hashlib, traceback, sys
import unreal
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/LowDensity/Integration'
SCOPE='/Game/CitySim/Art/LowDensity'
sys.path.insert(0,str(ROOT/'Tools'))
from CityIndustrialMetalMaterials import import_native_materials
lib=unreal.MaterialEditingLibrary;assets=unreal.AssetToolsHelpers.get_asset_tools()
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(o):assert unreal.EditorAssetLibrary.save_loaded_asset(o)
def create(path,kind,factory):
    return unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],kind,factory)
def node(m,kind):return lib.create_material_expression(m,kind)
def scalar(m,v):
    n=node(m,unreal.MaterialExpressionConstant);n.set_editor_property('r',v);return n
def connect(a,b,pin,output=''):assert lib.connect_material_expressions(a,output,b,pin)
def prop(a,p,output=''):assert lib.connect_material_property(a,output,p)
def finish(m):
    lib.layout_material_expressions(m);lib.recompile_material(m);save(m)
    s=lib.get_statistics(m);r={k:s.get_editor_property(k) for k in ('num_pixel_shader_instructions','num_vertex_shader_instructions','num_samplers')}
    assert r['num_pixel_shader_instructions']>0 and r['num_samplers']<16;return r
def sample(m,t,role):
    n=node(m,unreal.MaterialExpressionTextureSample);n.set_editor_property('texture',t)
    n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if role=='Normal' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS if role=='ORM' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    return n

def main():
    (OUT/'ImportError.txt').unlink(missing_ok=True)
    assert Path(unreal.Paths.project_dir()).resolve()==(ROOT/'CitySimVR').resolve()
    doc=json.loads((ROOT/'ArtSource/Manifests/city_low_density_buildings.json').read_text())
    assert digest(ROOT/doc['source'])==doc['source_sha256'] and len(doc['assets'])==24
    assert digest(ROOT/doc['approved_source'])==doc['approved_source_sha256']
    unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
    textures={};stats={};changed=[]
    for family,surface in doc['textures'].items():
        textures[family]={}
        for role,r in surface['maps'].items():
            assert digest(ROOT/r['file'])==r['sha256']
            path=SCOPE+'/Textures/T_'+family+'_'+role
            task=unreal.AssetImportTask()
            for k,v in dict(filename=str(ROOT/r['file']),destination_path=path.rsplit('/',1)[0],destination_name=path.rsplit('/',1)[1],automated=True,replace_existing=True,save=False).items():task.set_editor_property(k,v)
            task.factory=unreal.TextureFactory();assets.import_asset_tasks([task]);t=unreal.load_asset(path);assert t
            t.set_editor_property('srgb',role=='BaseColor');t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if role=='Normal' else unreal.TextureCompressionSettings.TC_MASKS if role=='ORM' else unreal.TextureCompressionSettings.TC_DEFAULT)
            t.set_editor_property('address_x',unreal.TextureAddress.TA_WRAP);t.set_editor_property('address_y',unreal.TextureAddress.TA_WRAP)
            if role=='Normal':t.set_editor_property('flip_green_channel',False)
            save(t);textures[family][role]=t;changed.append(path)
    for option in doc['materials']:
        if '-D104ReuseMaterials' in unreal.SystemLibrary.get_command_line():
            m=unreal.load_asset(option['native_material']);assert m and m.get_editor_property('used_with_instanced_static_meshes')
            assert m.get_editor_property('use_hq_forward_reflections')==option['hq_forward_reflections']
            s=lib.get_statistics(m);stats[option['name']]={k:s.get_editor_property(k) for k in ('num_pixel_shader_instructions','num_vertex_shader_instructions','num_samplers')}
            assert stats[option['name']]['num_pixel_shader_instructions']>0;changed.append(option['native_material']);continue
        m=create(option['native_material'],unreal.Material,unreal.MaterialFactoryNew());lib.delete_all_material_expressions(m)
        m.set_editor_property('used_with_instanced_static_meshes',True);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
        m.set_editor_property('use_hq_forward_reflections',option['hq_forward_reflections'])
        samples={role:sample(m,t,role) for role,t in textures[option['surface']].items()}
        tint=node(m,unreal.MaterialExpressionConstant3Vector);tint.set_editor_property('constant',unreal.LinearColor(*option['tint'],1))
        n=node(m,unreal.MaterialExpressionMultiply);connect(samples['BaseColor'],n,'A');connect(tint,n,'B');prop(n,unreal.MaterialProperty.MP_BASE_COLOR)
        prop(samples['Normal'],unreal.MaterialProperty.MP_NORMAL);prop(samples['ORM'],unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,'R')
        n=node(m,unreal.MaterialExpressionMultiply);connect(samples['ORM'],n,'A','G');connect(scalar(m,option['roughness_scale']),n,'B');prop(n,unreal.MaterialProperty.MP_ROUGHNESS)
        prop(scalar(m,option['metallic']),unreal.MaterialProperty.MP_METALLIC)
        stats[option['name']]=finish(m);changed.append(option['native_material'])
    if doc.get('industrial_metals'):import_native_materials(doc['industrial_metals'])
    # Offset baked market UVs into the existing4x2 atlas without changing mesh
    # geometry or losing business identity at higher density / after repair.
    m=create(SCOPE+'/Materials/M_StoreSigns',unreal.Material,unreal.MaterialFactoryNew());lib.delete_all_material_expressions(m)
    m.set_editor_property('used_with_instanced_static_meshes',True);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
    sign=sample(m,unreal.load_asset('/Game/CitySim/Art/Textures/T_StoreSigns_Tall_BaseColor'),'BaseColor')
    uv=node(m,unreal.MaterialExpressionTextureCoordinate);offset=node(m,unreal.MaterialExpressionVectorParameter)
    offset.set_editor_property('parameter_name','AtlasOffset');offset.set_editor_property('default_value',unreal.LinearColor(0,0,0,0))
    mask=node(m,unreal.MaterialExpressionComponentMask);mask.set_editor_property('r',True);mask.set_editor_property('g',True);connect(offset,mask,'')
    n=node(m,unreal.MaterialExpressionAdd);connect(uv,n,'A');connect(mask,n,'B')
    sign_inputs=lib.get_material_expression_input_names(sign);assert sign_inputs
    unreal.log('CITYSIM_D104_SIGN_INPUTS '+str(sign_inputs));connect(n,sign,str(sign_inputs[0]))
    prop(sign,unreal.MaterialProperty.MP_BASE_COLOR);prop(scalar(m,.65),unreal.MaterialProperty.MP_ROUGHNESS);stats['Signs']=finish(m);changed.append(m.get_path_name().split('.')[0])
    signs=json.loads((ROOT/'ArtSource/Textures/RoofsSigns/roofs_signs.json').read_text())['signs'];sign_materials={}
    for r in signs:
        path=SCOPE+'/Materials/MI_Sign_'+r['id'];mi=create(path,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        lib.set_material_instance_parent(mi,m);index=r['atlas_index']
        lib.set_material_instance_vector_parameter_value(mi,'AtlasOffset',unreal.LinearColor((index%4)*.25,(index//4)*.5,0,0));save(mi)
        sign_materials[r['id']]=mi;changed.append(path)
    records=[]
    for e in doc['assets']:
        assert digest(ROOT/e['export'])==e['export_sha256']
        package,name=e['mesh_destination'].rsplit('/',1)
        existing=unreal.load_asset(e['mesh_destination']) if unreal.EditorAssetLibrary.does_asset_exist(e['mesh_destination']) else None
        if existing:existing.get_editor_property('asset_import_data').set_editor_property('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE)
        task=unreal.AssetImportTask()
        for k,v in dict(filename=str(ROOT/e['export']),destination_path=package,destination_name=name,automated=True,replace_existing=True,replace_existing_settings=True,save=False).items():task.set_editor_property(k,v)
        task.factory=unreal.FbxFactory();ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_materials=False;ui.import_textures=False;ui.import_as_skeletal=False
        ui.set_editor_property('reset_to_fbx_on_material_conflict',True)
        for k,v in dict(combine_meshes=True,vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,convert_scene_unit=True,convert_scene=True,auto_generate_collision=False,generate_lightmap_u_vs=False,normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():ui.static_mesh_import_data.set_editor_property(k,v)
        task.options=ui;assets.import_asset_tasks([task]);mesh=unreal.load_asset(e['mesh_destination']);assert mesh
        slots=[str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')]
        assert len(slots)==len(e['slots']) and set(slots)==set(e['slots'])
        for i,slot in enumerate(slots):
            material=unreal.load_asset(e['material_bindings'][slot]);assert material,(e['name'],slot);mesh.set_material(i,material)
        assert mesh.get_num_triangles(0)==e['triangles'] and mesh.get_num_sections(0)==e['sections']
        b=mesh.get_bounding_box();size=b.max-b.min
        assert all(abs(a-b*100)<.2 for a,b in zip(size.to_tuple(),e['dimensions_m']))
        assert abs(b.min.z-e['bounds_min_m'][2]*100)<.2
        assert mesh.get_editor_property('asset_import_data').get_editor_property('vertex_color_import_option')==unreal.VertexColorImportOption.REPLACE
        save(mesh);changed.append(e['mesh_destination']);records.append(dict(name=e['name'],destination=e['mesh_destination'],triangles=mesh.get_num_triangles(0),sections=mesh.get_num_sections(0),dimensions_cm=list(size.to_tuple()),slots=slots))
    catalog=unreal.load_asset('/Game/CitySim/Data/DA_CityBuildingCatalog');assert catalog
    definitions=[e.export_text() for e in catalog.get_editor_property('definitions')]
    commercial=[e.export_text() for e in catalog.get_editor_property('commercial_presentation_variants')]
    variants=[];signed=[]
    for row in doc['variants']:
        v=unreal.CityLowDensityPresentationVariant()
        for k,value in dict(definition_id=row['definition_id'],appearance_id=row['appearance_id'],presentation_mesh=unreal.load_asset(row['presentation_mesh']),damaged_mesh=unreal.load_asset(row['damaged_mesh'])).items():v.set_editor_property(k,value)
        if row['definition_id']=='Shop':
            source_mesh=unreal.load_asset(row['presentation_mesh']);businesses={}
            for business in sign_materials:
                mesh=source_mesh
                if business!='market':
                    package,name=row['presentation_mesh'].rsplit('/',1);name+='_'+business
                    if unreal.EditorAssetLibrary.does_asset_exist(package+'/'+name):unreal.EditorAssetLibrary.delete_asset(package+'/'+name)
                    mesh=assets.duplicate_asset(name,package,source_mesh);assert mesh
                    slots=[str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')]
                    mesh.set_material(slots.index('StoreSignTall'),sign_materials[business]);save(mesh);changed.append(package+'/'+name)
                businesses[business]=mesh;signed.append(dict(appearance=row['appearance_id'],business=business,mesh=mesh.get_path_name(),sign_material=sign_materials[business].get_path_name()))
            v.set_editor_property('business_meshes',businesses)
        variants.append(v)
    catalog.set_editor_property('low_density_presentation_variants',variants)
    assert [e.export_text() for e in catalog.get_editor_property('definitions')]==definitions
    assert [e.export_text() for e in catalog.get_editor_property('commercial_presentation_variants')]==commercial
    save(catalog)
    (OUT/'CatalogBefore.json').write_text(json.dumps(dict(definitions=definitions,commercial=commercial),indent=2)+'\n')
    (OUT/'Import.json').write_text(json.dumps(dict(task='D104',source_sha256=doc['source_sha256'],meshes=records,commercial_sign_meshes=signed,variants=doc['variants'],shader_statistics=stats,changed=changed,gameplay_definitions_unchanged=True,commercial_identity_unchanged=True,production_import=True),indent=2)+'\n')
    unreal.log('CITYSIM_D104_LOW_DENSITY_IMPORT_PASS 24 source meshes +28 signed copies;12 catalog appearances')

try:main()
except Exception:
    OUT.mkdir(parents=True,exist_ok=True);(OUT/'ImportError.txt').write_text(traceback.format_exc());raise
