"""#125 explicit CI asset preparation; only new home/concrete packages.

Existing approved source/native assets and the catalog are read-only. CI keeps
the resulting products for native review and a subsequent scoped asset commit.
"""
from pathlib import Path
import hashlib, json, unreal
ROOT=Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.project_dir()).resolve()==(ROOT/'CitySimVR').resolve()
doc=json.loads((ROOT/'ArtSource/Manifests/city_home_ground_pilot.json').read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(ROOT/doc['export'])==doc['export_sha256']
assets=unreal.AssetToolsHelpers.get_asset_tools(); lib=unreal.MaterialEditingLibrary
scope='/Game/CitySim/Art/HomeGroundPilot'; changed=[]
def save(obj):
    assert unreal.EditorAssetLibrary.save_loaded_asset(obj)
    changed.append(obj.get_path_name().split('.')[0])
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
task=unreal.AssetImportTask()
for k,v in dict(filename=str(ROOT/doc['export']),destination_path=scope,destination_name='SM_CompactHome_GroundPilot',automated=True,replace_existing=True,save=False).items(): task.set_editor_property(k,v)
task.factory=unreal.FbxFactory(); options=unreal.FbxImportUI()
options.import_mesh=True; options.import_materials=False; options.import_textures=False; options.import_as_skeletal=False
data=options.static_mesh_import_data
for k,v in dict(combine_meshes=True,vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,convert_scene_unit=True,convert_scene=True,import_uniform_scale=1.,generate_lightmap_u_vs=True,auto_generate_collision=False,normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items(): data.set_editor_property(k,v)
task.options=options; assets.import_asset_tasks([task]); mesh=unreal.load_asset(doc['mesh_destination']); assert mesh
slots=[str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')]
assert set(slots)==set(doc['material_bindings']) and len(slots)==4
for i,s in enumerate(slots):
    mat=unreal.load_asset(doc['material_bindings'][s]); assert mat; mesh.set_material(i,mat)
assert mesh.get_num_triangles(0)==656
assert mesh.get_bounding_box().min.z>=-.1
save(mesh)
texfile=ROOT/'ArtSource/Textures/HomeGroundPilot/Concrete_BaseColor.png'
texdoc=json.loads((texfile.parent/'Concrete.json').read_text()); assert sha(texfile)==texdoc['sha256']
t=unreal.AssetImportTask()
for k,v in dict(filename=str(texfile),destination_path=scope,destination_name='T_Concrete',automated=True,replace_existing=True,save=False).items():t.set_editor_property(k,v)
t.factory=unreal.TextureFactory(); assets.import_asset_tasks([t]); texture=unreal.load_asset(scope+'/T_Concrete'); assert texture
texture.set_editor_property('srgb',True); texture.set_editor_property('address_x',unreal.TextureAddress.TA_WRAP); texture.set_editor_property('address_y',unreal.TextureAddress.TA_WRAP); save(texture)
material=unreal.load_asset(scope+'/M_Concrete') or assets.create_asset('M_Concrete',scope,unreal.Material,unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(material); material.set_editor_property('used_with_instanced_static_meshes',True)
def node(cls):return lib.create_material_expression(material,cls)
def link(a,b,pin,out=''):assert lib.connect_material_expressions(a,out,b,pin)
def scalar(v):
    n=node(unreal.MaterialExpressionConstant);n.set_editor_property('r',v);return n
# Standard tri-planar texture projection retains aggregate size as supports
# change height/width. Runtime removes the common miniature-board scale.
position=node(unreal.MaterialExpressionWorldPosition)
origin=node(unreal.MaterialExpressionActorPositionWS)
relative=node(unreal.MaterialExpressionSubtract);link(position,relative,'A');link(origin,relative,'B')
divide=node(unreal.MaterialExpressionDivide); link(relative,divide,'A')
scale=node(unreal.MaterialExpressionScalarParameter);scale.set_editor_property('parameter_name','BoardMetreCm');scale.set_editor_property('default_value',100.);link(scale,divide,'B')
normal=node(unreal.MaterialExpressionVertexNormalWS); absolute=node(unreal.MaterialExpressionAbs);link(normal,absolute,'')
power=node(unreal.MaterialExpressionPower);power.set_editor_property('const_exponent',8.);link(absolute,power,'Base')
def mask(src,channels):
    n=node(unreal.MaterialExpressionComponentMask)
    for c in ('r','g','b','a'):n.set_editor_property(c,c in channels)
    link(src,n,'');return n
terms=[];weights=[]
for uv,axis in [('gb','r'),('rb','g'),('rg','b')]:
    sample=node(unreal.MaterialExpressionTextureSample);sample.set_editor_property('texture',texture)
    sample.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    link(mask(divide,uv),sample,'')
    weight=mask(power,axis);weights.append(weight)
    mult=node(unreal.MaterialExpressionMultiply);link(sample,mult,'A');link(weight,mult,'B');terms.append(mult)
def add(a,b):
    n=node(unreal.MaterialExpressionAdd);link(a,n,'A');link(b,n,'B');return n
result=node(unreal.MaterialExpressionDivide);link(add(add(terms[0],terms[1]),terms[2]),result,'A');link(add(add(weights[0],weights[1]),weights[2]),result,'B')
assert lib.connect_material_property(result,'',unreal.MaterialProperty.MP_BASE_COLOR)
assert lib.connect_material_property(scalar(.9),'',unreal.MaterialProperty.MP_ROUGHNESS)
assert lib.connect_material_property(scalar(.25),'',unreal.MaterialProperty.MP_SPECULAR)
lib.layout_material_expressions(material);lib.recompile_material(material);save(material)
stats=lib.get_statistics(material)
assert stats.get_editor_property('num_pixel_shader_instructions')>0
out=ROOT/'Artifacts/CIAssetPreparation';out.mkdir(parents=True,exist_ok=True)
files=['CitySimVR/Content/'+p.removeprefix('/Game/')+'.uasset' for p in changed]
(out/'Files.txt').write_text('\n'.join(files)+'\n')
(out/'HomeGroundPilot.json').write_text(json.dumps(dict(issue=125,files=files,source=doc,texture=texdoc,triangles=mesh.get_num_triangles(0),slots=slots,errors=[]),indent=2)+'\n')
print('CITYSIM_HOME_GROUND_IMPORT_PASS',files)
