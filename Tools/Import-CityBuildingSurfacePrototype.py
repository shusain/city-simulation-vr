"""Import native-node D79 PBR prototypes into a dedicated Unreal folder only.

UnrealEditor-Cmd project -run=pythonscript -Script=<this> -AllowCommandletRendering
Does not change catalog references, common materials, maps or packaged games.
"""
from pathlib import Path
import hashlib, json
import unreal

root=Path(__file__).resolve().parents[1]
production='-PromoteSurfaceAssets' in unreal.SystemLibrary.get_command_line()
scope='/Game/CitySim/Art' if production else '/Game/CitySim/Art/Prototypes/BuildingSurfaces'
meshes_only='-SurfaceMeshesOnly' in unreal.SystemLibrary.get_command_line()
reuse_surfaces='-ReuseSurfaceAssets' in unreal.SystemLibrary.get_command_line()
mechanical_only='-MechanicalMeshesOnly' in unreal.SystemLibrary.get_command_line()
report_path=root/('Artifacts/AssetImport/D79-surfaces-production.json' if production else 'Artifacts/AssetImport/D79-surfaces-import.json')
report_path.parent.mkdir(parents=True,exist_ok=True)
if report_path.exists(): report_path.unlink()
manifest=json.loads((root/'ArtSource/Manifests/city_building_surface_prototype.json').read_text(encoding='utf-8'))
validated=json.loads((root/'Artifacts/D79/surfaces-roundtrip.json').read_text(encoding='utf-8'))
assert len(manifest['assets'])==18 and manifest['prototype_only']
assert {(e['category'],e['density'],e['damaged']) for e in manifest['assets']}=={
    (category,density,damaged) for category in ('Residential','Commercial','Industrial')
    for density in ('Low','Medium','High') for damaged in (False,True)}
mesh_entries=manifest['assets']+manifest.get('sign_variants',[])
assert {e['sha256'] for e in mesh_entries}=={r['sha256'] for r in validated if r['roundtrip_pass']}
if manifest.get('sign_variants'):
    assert len(manifest['sign_variants'])==48
    assert {(e['business'],e['density'],e['damaged']) for e in manifest['sign_variants']}=={
        (business,density,damaged) for business in ('market','bakery','cafe','books','pharmacy','repairs','outfitters','tech')
        for density in ('Low','Medium','High') for damaged in (False,True)}
if production:
    starter=json.loads((root/'ArtSource/Manifests/city_starter_kit.json').read_text(encoding='utf-8'))
    originals={e['asset_id']:e for e in starter['assets']}
    mesh_entries=[dict(e) for e in mesh_entries]
    for e in mesh_entries:
        e['destination']=scope+'/Meshes'
        e['material_slots']={name:path.replace('/Game/CitySim/Art/Prototypes/BuildingSurfaces',scope) for name,path in e['material_slots'].items()}
        e['production_name']=Path(e['export']).stem if e.get('business') else originals[e['asset_id'].removesuffix('.surfaces')]['mesh_destination'].rsplit('/',1)[1]
if mechanical_only:
    assert production and meshes_only, 'Focused mechanical import requires production mesh-only mode'
    mesh_entries=[e for e in mesh_entries if e['category']=='Industrial' or
                  (e['category']=='Residential' and e['density']=='High')]
    assert len(mesh_entries)==8
assets=unreal.AssetToolsHelpers.get_asset_tools(); lib=unreal.MaterialEditingLibrary
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
assert not unreal.SystemLibrary.get_console_variable_bool_value('Interchange.FeatureFlags.Import.FBX'), 'Legacy FBX import is required for explicit vertex-color replacement'
changed=[]
# Record existing content and catalog files before editor writes. This import
# uses a new folder; changes outside it indicate a scope violation.
protected=list((root/'CitySimVR/Content/CitySim/Art/Materials').glob('*.uasset'))
protected+=list((root/'CitySimVR/Content/CitySim/Art/Meshes').glob('SM_Building_*.uasset'))
protected+=list((root/'CitySimVR/Content/CitySim/Data').rglob('*.uasset'))
protected+=list((root/'CitySimVR/Content/CitySim/Maps').glob('*.umap'))
protected.append(root/'ArtSource/Manifests/city_starter_kit.json')
if production:
    selected={e['production_name']+'.uasset' for e in mesh_entries}
    authored_materials={path.rsplit('/',1)[1]+'.uasset' for e in mesh_entries for path in e['material_slots'].values()}
    authored_materials.update('M_'+name+'.uasset' for name in ('CityBrick','CityMechanical','CityRoof','StoreSigns'))
    protected=[p for p in protected if not (p.parent.name=='Meshes' and p.name in selected) and
               not (p.parent.name=='Materials' and p.name in authored_materials)]
protected_hashes={p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
accepted_surfaces={}
if meshes_only or reuse_surfaces:
    for subfolder in ('Materials','Textures'):
        for path in (root/'CitySimVR/Content'/scope.removeprefix('/Game/')/subfolder).glob('*.uasset'):
            accepted_surfaces[path.relative_to(root).as_posix()]=hashlib.sha256(path.read_bytes()).hexdigest()
    assert len(accepted_surfaces)>=14, 'Reuse mode requires the accepted surface assets'

def scoped(path):
    assert path.startswith(scope+'/'), 'Asset write outside the selected surface scope'
    if production:
        relative=path.removeprefix(scope+'/').split('/')
        assert relative[0] in ('Meshes','Materials','Textures') and len(relative)<=2
    return path

def get(path,kind,factory):
    scoped(path)
    return unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],kind,factory)

def save(asset):
    path=asset.get_path_name().split('.')[0]; scoped(path)
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset), path
    changed.append(path)

texture_metadata=json.loads((root/'ArtSource/Textures/BuildingSurfaces/building_surfaces.json').read_text(encoding='utf-8'))
if manifest.get('roof_sign_textures'):
    roof_sign_metadata=json.loads((root/manifest['roof_sign_textures']).read_text(encoding='utf-8'))
    texture_metadata['textures']+=roof_sign_metadata['textures']
textures={}
for e in texture_metadata['textures']:
    path=root/e['file']; assert hashlib.sha256(path.read_bytes()).hexdigest()==e['sha256']
    name=path.stem
    if meshes_only or (reuse_surfaces and unreal.EditorAssetLibrary.does_asset_exist(scope+'/Textures/T_'+name)):
        textures[name]=unreal.EditorAssetLibrary.load_asset(scope+'/Textures/T_'+name)
        assert textures[name], name
        continue
    task=unreal.AssetImportTask()
    for key,value in dict(filename=str(path),destination_path=scoped(scope+'/Textures'),destination_name='T_'+name,automated=True,replace_existing=True,replace_existing_settings=True,save=False).items(): task.set_editor_property(key,value)
    # An explicit factory uses UE's legacy automated import path. Interchange
    # otherwise invokes Slate notification UI, unavailable in commandlets.
    task.set_editor_property('factory',unreal.TextureFactory())
    assets.import_asset_tasks([task]); texture=unreal.EditorAssetLibrary.load_asset(scope+'/Textures/T_'+name); assert texture
    normal=name.endswith('_Normal'); orm=name.endswith('_ORM')
    texture.set_editor_property('srgb',not(normal or orm))
    texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if normal else unreal.TextureCompressionSettings.TC_MASKS if orm else unreal.TextureCompressionSettings.TC_DEFAULT)
    address=unreal.TextureAddress.TA_WRAP if e.get('wrap',True) else unreal.TextureAddress.TA_CLAMP
    texture.set_editor_property('address_x',address); texture.set_editor_property('address_y',address)
    if normal: texture.set_editor_property('flip_green_channel',False) # Maps already use DirectX convention.
    save(texture); textures[name]=texture

def node(m,kind): return lib.create_material_expression(m,kind,0,0)
def link(a,b,pin,output=''): assert lib.connect_material_expressions(a,output,b,pin)
def prop(n,kind,output=''): assert lib.connect_material_property(n,output,kind)
def color(m,name,value):
    n=node(m,unreal.MaterialExpressionVectorParameter); n.set_editor_property('parameter_name',name); n.set_editor_property('default_value',unreal.LinearColor(*value,1)); return n
def scalar(m,name,value):
    n=node(m,unreal.MaterialExpressionScalarParameter); n.set_editor_property('parameter_name',name); n.set_editor_property('default_value',value); return n
def sample(m,name,sampler):
    n=node(m,unreal.MaterialExpressionTextureSample); n.set_editor_property('texture',textures[name]); n.set_editor_property('sampler_type',sampler); return n

parents={}; shader_statistics={}
for mechanical,prefix in ((False,'CityBrick'),(True,'CityMechanical')):
    if meshes_only or reuse_surfaces:
        parents[prefix]=unreal.EditorAssetLibrary.load_asset(scope+'/Materials/M_'+prefix)
        assert parents[prefix], prefix
        assert len(lib.get_used_textures(parents[prefix]))==3
        stats=lib.get_statistics(parents[prefix])
        shader_statistics[prefix]={key:stats.get_editor_property(key) for key in (
            'num_vertex_shader_instructions','num_pixel_shader_instructions',
            'num_samplers','num_pixel_texture_samples','num_vertex_texture_samples')}
        assert shader_statistics[prefix]['num_pixel_shader_instructions']>0
        continue
    mat=get(scope+'/Materials/M_'+prefix,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('used_with_instanced_static_meshes',True)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
    base=sample(mat,prefix+'_BaseColor',unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal=sample(mat,prefix+'_Normal',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    orm=sample(mat,prefix+'_ORM',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    tint=color(mat,'CityColor',(.22,.58,.32))
    if mechanical:
        vertex=node(mat,unreal.MaterialExpressionVertexColor)
        bare=color(mat,'BareMetalColor',(.68,.72,.76))
        mix=node(mat,unreal.MaterialExpressionLinearInterpolate); link(tint,mix,'A'); link(bare,mix,'B'); link(vertex,mix,'Alpha','R'); tint=mix
        prop(vertex,unreal.MaterialProperty.MP_METALLIC,'R')
        rough=node(mat,unreal.MaterialExpressionLinearInterpolate)
        paint_rough=scalar(mat,'PaintRoughness',.70); link(paint_rough,rough,'A'); link(orm,rough,'B','G'); link(vertex,rough,'Alpha','R')
        prop(rough,unreal.MaterialProperty.MP_ROUGHNESS)
    else: prop(orm,unreal.MaterialProperty.MP_ROUGHNESS,'G')
    multiply=node(mat,unreal.MaterialExpressionMultiply); link(base,multiply,'A'); link(tint,multiply,'B'); prop(multiply,unreal.MaterialProperty.MP_BASE_COLOR)
    prop(normal,unreal.MaterialProperty.MP_NORMAL); prop(orm,unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,'R')
    lib.layout_material_expressions(mat); lib.recompile_material(mat); save(mat)
    assert len(lib.get_used_textures(mat))==3
    # Native GetStatistics finishes compilation before returning its desktop
    # shader statistics. A failed material has no valid instruction count.
    stats=lib.get_statistics(mat)
    shader_statistics[prefix]={key:stats.get_editor_property(key) for key in (
        'num_vertex_shader_instructions','num_pixel_shader_instructions',
        'num_samplers','num_pixel_texture_samples','num_vertex_texture_samples')}
    assert shader_statistics[prefix]['num_pixel_shader_instructions']>0, 'Material shader did not compile'
    parents[prefix]=mat

if manifest.get('roof_sign_textures'):
    def parameter_sample(mat,param,texture_name,sampler):
        n=node(mat,unreal.MaterialExpressionTextureSampleParameter2D)
        n.set_editor_property('parameter_name',param);n.set_editor_property('texture',textures[texture_name])
        n.set_editor_property('sampler_type',sampler);return n
    for prefix in ('CityRoof','StoreSigns'):
        path=scope+'/Materials/M_'+prefix
        if meshes_only or (reuse_surfaces and unreal.EditorAssetLibrary.does_asset_exist(path)):
            mat=unreal.EditorAssetLibrary.load_asset(path);assert mat
        else:
            mat=get(path,unreal.Material,unreal.MaterialFactoryNew());lib.delete_all_material_expressions(mat)
            mat.set_editor_property('used_with_instanced_static_meshes',True)
            mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
            if prefix=='CityRoof':
                base=parameter_sample(mat,'RoofBaseColor','RoofMembrane_BaseColor',unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
                normal=parameter_sample(mat,'RoofNormal','RoofMembrane_Normal',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
                orm=parameter_sample(mat,'RoofORM','RoofMembrane_ORM',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
                tint=color(mat,'CityColor',(.19,.23,.27))
                multiply=node(mat,unreal.MaterialExpressionMultiply);link(base,multiply,'A');link(tint,multiply,'B')
                prop(multiply,unreal.MaterialProperty.MP_BASE_COLOR)
                prop(normal,unreal.MaterialProperty.MP_NORMAL);prop(orm,unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,'R')
                prop(orm,unreal.MaterialProperty.MP_ROUGHNESS,'G');prop(orm,unreal.MaterialProperty.MP_METALLIC,'B')
            else:
                base=parameter_sample(mat,'SignAtlas','StoreSigns_Wide_BaseColor',unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
                prop(base,unreal.MaterialProperty.MP_BASE_COLOR)
                prop(scalar(mat,'SignRoughness',.65),unreal.MaterialProperty.MP_ROUGHNESS)
            lib.layout_material_expressions(mat);lib.recompile_material(mat);save(mat)
        assert len(lib.get_used_textures(mat))==(3 if prefix=='CityRoof' else 1)
        stats=lib.get_statistics(mat)
        shader_statistics[prefix]={key:stats.get_editor_property(key) for key in (
            'num_vertex_shader_instructions','num_pixel_shader_instructions','num_samplers',
            'num_pixel_texture_samples','num_vertex_texture_samples')}
        assert shader_statistics[prefix]['num_pixel_shader_instructions']>0
        parents[prefix]=mat

window=unreal.EditorAssetLibrary.load_asset('/Game/CitySim/Art/Materials/M_CityWindowDetail')
flat=unreal.EditorAssetLibrary.load_asset('/Game/CitySim/Art/Materials/M_CityColor')
assert window and flat, 'Existing shared window/color parents are required, read-only'
instances={}
for name,parent,tint in (
    ('ResidentialBrick',parents['CityBrick'],(.22,.58,.32)),('Roof',window,(.10,.14,.19)),
    ('CommercialBrick',parents['CityBrick'],(.12,.42,.78)),('IndustrialBrick',parents['CityBrick'],(.86,.64,.10)),
    ('IndustrialMechanical',parents['CityMechanical'],(.86,.64,.10)),
    ('Mechanical',parents['CityMechanical'],(.22,.58,.32)),('CharredBrick',parents['CityBrick'],(.11,.095,.08)),
    ('CharredTrim',flat,(.065,.06,.055)),('CharredMechanical',parents['CityMechanical'],(.12,.10,.08))):
    path=scope+'/Materials/MI_'+name
    if meshes_only or (reuse_surfaces and unreal.EditorAssetLibrary.does_asset_exist(path)):
        instances[path]=unreal.EditorAssetLibrary.load_asset(path)
        assert instances[path] and instances[path].get_editor_property('parent')==parent, path
        continue
    instance=get(path,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(instance,parent)
    lib.set_material_instance_vector_parameter_value(instance,'CityColor',unreal.LinearColor(*tint,1))
    if name=='CharredMechanical':
        lib.set_material_instance_vector_parameter_value(instance,'BareMetalColor',unreal.LinearColor(.20,.18,.16,1))
        lib.set_material_instance_scalar_parameter_value(instance,'PaintRoughness',.94)
    save(instance); instances[path]=instance

if manifest.get('roof_sign_textures'):
    roof_tints={'RoofShingles':(.24,.29,.33),'RoofMembrane':(.19,.23,.27),'RoofMetal':(.36,.40,.42)}
    new_instances=[]
    for texture_prefix,tint in roof_tints.items():
        for damaged in (False,True):
            new_instances.append((('Charred' if damaged else '')+texture_prefix,parents['CityRoof'],
                (.12,.105,.09) if damaged else tint,
                {'RoofBaseColor':texture_prefix+'_BaseColor','RoofNormal':texture_prefix+'_Normal','RoofORM':texture_prefix+'_ORM'}))
    for aspect in ('Wide','Tall'):
        new_instances.append(('StoreSign'+aspect,parents['StoreSigns'],None,{'SignAtlas':'StoreSigns_'+aspect+'_BaseColor'}))
    for name,parent,tint,parameters in new_instances:
        path=scope+'/Materials/MI_'+name
        if meshes_only or (reuse_surfaces and unreal.EditorAssetLibrary.does_asset_exist(path)):
            instance=unreal.EditorAssetLibrary.load_asset(path);assert instance and instance.get_editor_property('parent')==parent
        else:
            instance=get(path,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
            lib.set_material_instance_parent(instance,parent)
            if tint:lib.set_material_instance_vector_parameter_value(instance,'CityColor',unreal.LinearColor(*tint,1))
            for parameter,texture_name in parameters.items():
                lib.set_material_instance_texture_parameter_value(instance,parameter,textures[texture_name])
            save(instance)
        # Instances must actually point at each roof type/aspect rather than
        # silently inheriting the master's default map.
        for parameter,texture_name in parameters.items():
            assert lib.get_material_instance_texture_parameter_value(instance,parameter)==textures[texture_name]
        instances[path]=instance

measurements=[]
for e in mesh_entries:
    path=root/e['export']; assert hashlib.sha256(path.read_bytes()).hexdigest()==e['sha256']
    destination=scoped(e['destination']); name=e.get('production_name',path.stem)
    existing=unreal.EditorAssetLibrary.load_asset(destination+'/'+name)
    if existing:
        # Legacy FBX reimport reads the mesh's persisted import data rather than
        # the fresh task UI. Replace the source colors explicitly: otherwise
        # the old white RGB survives while the old window alpha looks correct.
        existing_data=existing.get_editor_property('asset_import_data')
        if isinstance(existing_data,unreal.FbxStaticMeshImportData):
            existing_data.set_editor_property('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE)
    task=unreal.AssetImportTask()
    for key,value in dict(filename=str(path),destination_path=destination,destination_name=name,automated=True,replace_existing=True,replace_existing_settings=True,save=False).items(): task.set_editor_property(key,value)
    task.set_editor_property('factory',unreal.FbxFactory())
    options=unreal.FbxImportUI()
    options.set_editor_property('reset_to_fbx_on_material_conflict',True)
    for key,value in dict(import_mesh=True,import_materials=False,import_textures=False,import_as_skeletal=False).items(): options.set_editor_property(key,value)
    data=options.get_editor_property('static_mesh_import_data')
    for key,value in dict(combine_meshes=True,vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,convert_scene_unit=True,generate_lightmap_u_vs=True,auto_generate_collision=False).items(): data.set_editor_property(key,value)
    data.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task.set_editor_property('options',options); assets.import_asset_tasks([task])
    mesh=unreal.EditorAssetLibrary.load_asset(destination+'/'+name); assert mesh
    imported_data=mesh.get_editor_property('asset_import_data')
    assert isinstance(imported_data,unreal.FbxStaticMeshImportData), 'FBX task unexpectedly routed through Interchange'
    if imported_data.get_editor_property('vertex_color_import_option')!=unreal.VertexColorImportOption.REPLACE:
        # The first migration from Interchange can restore its old color policy
        # onto the new legacy data. Reimport once with the persisted policy fixed.
        imported_data.set_editor_property('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE)
        assets.import_asset_tasks([task])
        mesh=unreal.EditorAssetLibrary.load_asset(destination+'/'+name)
    assert mesh.get_editor_property('asset_import_data').get_editor_property('vertex_color_import_option')==unreal.VertexColorImportOption.REPLACE
    slots=[str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')]
    assert len(slots)==e['sections'] and set(slots)==set(e['material_slots']),(name,slots,list(e['material_slots']))
    for i,slot in enumerate(slots): mesh.set_material(i,instances[e['material_slots'][slot]])
    assert mesh.get_num_triangles(0)==e['triangles'] and mesh.get_num_sections(0)==e['sections']
    bounds=mesh.get_bounding_box(); extent=bounds.max-bounds.min
    actual=[extent.x/100,extent.y/100,extent.z/100]
    assert all(abs(a-b)<.02 for a,b in zip(sorted(actual[:2])+actual[2:],sorted(e['dimensions_m'][:2])+e['dimensions_m'][2:]))
    assert abs(bounds.min.z)<.1
    save(mesh)
    measurements.append(dict(asset_id=e['asset_id'],mesh=destination+'/'+name,triangles=e['triangles'],sections=e['sections'],dimensions_m=actual,materials=[mesh.get_material(i).get_path_name() for i in range(len(slots))]))

assert all(hashlib.sha256((root/p).read_bytes()).hexdigest()==digest for p,digest in protected_hashes.items()), 'Existing content changed during prototype import'
assert all(hashlib.sha256((root/p).read_bytes()).hexdigest()==digest for p,digest in accepted_surfaces.items()), 'Accepted prototype surfaces changed during reuse import'
report_path.write_text(json.dumps(dict(task='D79',prototype_only=not production,scope=scope,
    production_meshes_updated=production,production_references_changed=False,existing_content_hashes_unchanged=len(protected_hashes),
    meshes_only=meshes_only,reuse_surfaces=reuse_surfaces,accepted_surface_hashes_unchanged=len(accepted_surfaces),
    changed=changed,textures=texture_metadata['textures'],desktop_shader_statistics=shader_statistics,
    meshes=measurements),indent=2)+'\n',encoding='utf-8')
unreal.log('CITYSIM_BUILDING_SURFACE_IMPORT_PASS '+str(len(measurements))+' meshes; per-mesh triangle/section guards passed')
