"""Import the pinned fleet only; preserve existing pools and civic pulse graph."""
from pathlib import Path
import hashlib,json,re,sys
import unreal
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Tools'))
from Assets.Manifests.CityApprovedSources import verified_export
PLAN=ROOT/'ArtSource/Manifests/city_vehicle_native345.json'
SCOPE='/Game/CitySim/Art/Vehicle345'
LIB=unreal.MaterialEditingLibrary
FILES=[]

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def create(path,kind,factory):
    obj=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],kind,factory)
    assert isinstance(obj,kind),path
    return obj
def save(obj):
    assert unreal.EditorAssetLibrary.save_loaded_asset(obj)
    path=obj.get_path_name().split('.')[0]
    FILES.append('CitySimVR/Content/'+path.removeprefix('/Game/')+'.uasset')
def main():
    plan=json.loads(PLAN.read_bytes());assert len(plan['assets'])==6 and plan['source_unchanged']
    for a in plan['assets']:
        verified_export(ROOT/a['source'],a['issue'],a['version'],a['approval_sha256'],'blender')
        assert sha(ROOT/a['export'])==a['export_sha256']
    parent=create(SCOPE+'/Materials/M_FleetSurface',unreal.Material,unreal.MaterialFactoryNew())
    # Only this new parent belongs to this importer. Existing civic light graph
    # and all its instances remain immutable and are reused below.
    if not LIB.get_material_property_input_node(parent,unreal.MaterialProperty.MP_BASE_COLOR):
        for name,kind,prop in [('CityColor',unreal.MaterialExpressionVectorParameter,unreal.MaterialProperty.MP_BASE_COLOR),('Roughness',unreal.MaterialExpressionScalarParameter,unreal.MaterialProperty.MP_ROUGHNESS),('Metallic',unreal.MaterialExpressionScalarParameter,unreal.MaterialProperty.MP_METALLIC)]:
            node=LIB.create_material_expression(parent,kind);node.set_editor_property('parameter_name',name)
            assert LIB.connect_material_property(node,'',prop)
        parent.set_editor_property('used_with_instanced_static_meshes',True)
        LIB.layout_material_expressions(parent);LIB.recompile_material(parent)
    save(parent)
    unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
    sub=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.new_object(unreal.StaticMeshEditorSubsystem)
    reports=[]
    for a in plan['assets']:
        bindings={}
        for slot,m in a['materials'].items():
            path=SCOPE+'/Materials/MI_'+a['key']+'_'+re.sub('[^A-Za-z0-9_]','_',slot)
            instance=create(path,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
            LIB.set_material_instance_parent(instance,parent)
            LIB.set_material_instance_vector_parameter_value(instance,'CityColor',unreal.LinearColor(*m['color']))
            LIB.set_material_instance_scalar_parameter_value(instance,'Roughness',m['roughness'])
            LIB.set_material_instance_scalar_parameter_value(instance,'Metallic',m['metallic'])
            save(instance);bindings[slot]=path
        if 'Roof' in a['slots']:
            bindings['Roof']='/Game/CitySim/Art/Materials/MI_City'+('FireDetail' if a['key']=='FireEngine' else 'PoliceDetail')
        existing=unreal.load_asset(a['native'])
        if existing:
            data=existing.get_editor_property('asset_import_data')
            if isinstance(data,unreal.FbxStaticMeshImportData):data.set_editor_property('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE)
        task=unreal.AssetImportTask();path=a['native'];folder,name=path.rsplit('/',1)
        for k,v in dict(filename=str(ROOT/a['export']),destination_path=folder,destination_name=name,automated=True,replace_existing=True,replace_existing_settings=True,save=False).items():task.set_editor_property(k,v)
        task.factory=unreal.FbxFactory();options=unreal.FbxImportUI()
        for k,v in dict(import_mesh=True,import_materials=False,import_textures=False,import_as_skeletal=False,reset_to_fbx_on_material_conflict=True).items():options.set_editor_property(k,v)
        for k,v in dict(combine_meshes=True,vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,convert_scene=True,convert_scene_unit=True,import_uniform_scale=1.,import_translation=unreal.Vector(0,0,0),import_rotation=unreal.Rotator(0,0,0),transform_vertex_to_absolute=True,bake_pivot_in_vertex=False,generate_lightmap_u_vs=False,build_nanite=False,auto_generate_collision=False,remove_degenerates=False,normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():options.static_mesh_import_data.set_editor_property(k,v)
        task.options=options;unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);mesh=unreal.load_asset(path);assert isinstance(mesh,unreal.StaticMesh)
        imported=mesh.get_editor_property('asset_import_data')
        assert isinstance(imported,unreal.FbxStaticMeshImportData)
        if imported.get_editor_property('vertex_color_import_option')!=unreal.VertexColorImportOption.REPLACE:
            imported.set_editor_property('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE)
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);mesh=unreal.load_asset(path)
        assert mesh.get_editor_property('asset_import_data').get_editor_property('vertex_color_import_option')==unreal.VertexColorImportOption.REPLACE
        settings=sub.get_lod_build_settings(mesh,0)
        for k,v in dict(recompute_normals=False,recompute_tangents=True,use_full_precision_u_vs=True,use_high_precision_tangent_basis=True,generate_lightmap_u_vs=False,remove_degenerates=False,build_scale3d=unreal.Vector(1,1,1)).items():settings.set_editor_property(k,v)
        sub.set_lod_build_settings(mesh,0,settings)
        slots=[str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')]
        assert len(slots)==len(a['slots']) and set(slots)==set(a['slots']),(a['key'],slots,a['slots'])
        for i,slot in enumerate(slots):
            material=unreal.load_asset(bindings[slot]);assert material;mesh.set_material(i,material)
        assert mesh.get_num_triangles(0)==a['triangles'],(a['key'],mesh.get_num_triangles(0),a['triangles'])
        vertices=[];sections=[]
        for section in range(mesh.get_num_sections(0)):
            v,t,n,u,g=unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh,0,section);vertices.extend(v)
            sections.append(dict(material=slots[sub.get_lod_material_slot(mesh,0,section)],triangles=len(t)//3))
        native={side:[fn(getattr(v,k) for v in vertices) for k in ('x','y','z')] for side,fn in [('min',min),('max',max)]}
        b=a['bounds_m'];expected=dict(min=[b['min'][0]*100,-b['max'][1]*100,b['min'][2]*100],max=[b['max'][0]*100,-b['min'][1]*100,b['max'][2]*100])
        assert max(abs(native[s][k]-expected[s][k]) for s in ('min','max') for k in range(3))<.2,(a['key'],native,expected)
        assert sub.get_num_uv_channels(mesh,0)>=1
        if a['key']=='FireEngine':
            socket=mesh.find_socket('WaterNozzle')
            if not socket:
                socket=unreal.new_object(unreal.StaticMeshSocket,outer=mesh)
                socket.set_editor_property('socket_name','WaterNozzle')
                mesh.add_socket(socket)
            # Reviewed v2 nozzle mouth, source metres -> native centimetres.
            socket.set_editor_property('relative_location',unreal.Vector(125,84,379))
        save(mesh);reports.append(dict(key=a['key'],source_issue=a['issue'],version=a['version'],native=path,slots=slots,bindings=bindings,triangles=a['triangles'],native_bounds_cm=native,sections=sections))
    out=ROOT/'Artifacts/CIAssetPreparation';out.mkdir(parents=True,exist_ok=True)
    (out/'Vehicle345.json').write_text(json.dumps(dict(issue=345,plan_sha256=sha(PLAN),assets=reports,files=[dict(file=p,sha256=sha(ROOT/p)) for p in dict.fromkeys(FILES)]),indent=2)+'\n')
    (out/'Files.txt').write_text('\n'.join(dict.fromkeys(FILES))+'\n')
    unreal.log('VEHICLE345_NATIVE_IMPORT_PASS 6')

if __name__=='__main__':main()
