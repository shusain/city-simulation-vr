"""CI batch import: fifteen exact approved buildings, shared maps and explicit MIs."""
import copy,re
import unreal
from Assets.Unreal.CityApprovedCivicPlan import ROOT,PLAN,SCOPE,preflight,sha,write
from Assets.Unreal.CityApprovedUtilityNative import textures_and_guard,unchanged,object_path
from Assets.Unreal.CityApprovedUtilityMaterials import TemplateGraph,assign_and_verify
from Assets.Unreal.CityApprovedUtilityTemplates import instance_parameters
from Assets.Unreal.CityApprovedCivicMeshes import import_mesh,check_mesh
OUT=ROOT/'Artifacts/CIAssetPreparation'
LIB=unreal.MaterialEditingLibrary
FILES=[]
def scoped(path):
    assert path.startswith(SCOPE+'/') and re.fullmatch('/Game/[A-Za-z0-9_/]+',path),path
    return path
def save(obj):
    path=scoped(object_path(obj));assert unreal.EditorAssetLibrary.save_loaded_asset(obj)
    relative='CitySimVR/Content/'+path.removeprefix('/Game/')+'.uasset'
    FILES.append(relative)
def create(path,kind,factory):
    scoped(path)
    obj=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],kind,factory)
    assert isinstance(obj,kind);return obj
def normal_parent(compiled,textures):
    parent=create(compiled['parent'],unreal.Material,unreal.MaterialFactoryNew());LIB.delete_all_material_expressions(parent)
    for name,value in dict(used_with_instanced_static_meshes=True,blend_mode=unreal.BlendMode.BLEND_OPAQUE,tangent_space_normal=True,two_sided=False).items():parent.set_editor_property(name,value)
    g=TemplateGraph(parent,compiled,textures)
    base=g.blend(g.binary('Multiply',g.param('BaseColor'),g.param('SurfaceTint')),g.param('DamageSolidColor'),g.param('DamageSolidWeight'))
    rough=g.blend(g.param('Roughness'),g.scalar(.95),g.param('DamageRoughnessWeight'))
    decoded=g.sample('Normal');xy=g.binary('Multiply',g.append(g.mask(decoded,0),g.mask(decoded,1)),g.param('NormalStrength'))
    normal=g.unary('Normalize',g.append(xy,g.mask(decoded,2)))
    for v,prop in [(base,unreal.MaterialProperty.MP_BASE_COLOR),(rough,unreal.MaterialProperty.MP_ROUGHNESS),(g.param('Metallic'),unreal.MaterialProperty.MP_METALLIC),(normal,unreal.MaterialProperty.MP_NORMAL)]:assert LIB.connect_material_property(v[0],v[1],prop)
    assert set(g.parameters)==set(compiled['parameters'])
    LIB.layout_material_expressions(parent);LIB.recompile_material(parent);save(parent)
    assert LIB.get_statistics(parent).get_editor_property('num_pixel_shader_instructions')>0
    return parent
def main():
    plan=preflight();textures,_,shared,protected=textures_and_guard(plan)
    # Utility shared parents are read-only reusable stock shader structures.
    for name in ('Solid','PBR'):
        f=ROOT/('CitySimVR/Content/CitySim/Art/Utilities/ApprovedBatch/Materials/M_Template_'+name+'.uasset');protected[f]=sha(f)
    unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
    subsystem=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.new_object(unreal.StaticMeshEditorSubsystem)
    results=[];normal=None
    for a in plan['assets']:
        bindings={False:{},True:{}};materials=[]
        for region,compiled in a['compiled_materials'].items():
            if compiled['template']=='SolidNormal' and normal is None:normal=normal_parent(compiled,textures)
            parent=unreal.load_asset(compiled['parent']);assert isinstance(parent,unreal.Material) and not parent.get_editor_property('two_sided')
            for damaged in (False,True):
                path=SCOPE+'/Materials/MI_'+a['key']+'_'+region+('_Damaged' if damaged else '')
                instance=create(path,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew());LIB.set_material_instance_parent(instance,parent)
                params=instance_parameters(compiled,damaged)
                # SolidNormal retains its source normal map, with the same
                # charred color/roughness policy as the shared Solid template.
                observed=assign_and_verify(instance,params,textures);save(instance);bindings[damaged][region]=path
                materials.append(dict(region=region,damaged=damaged,compiled=compiled,readback=observed))
        mesh,healthy=import_mesh(a,bindings[False],subsystem,scope_path=scoped,save_asset=save)
        damaged=unreal.load_asset(a['damaged_target']) if unreal.EditorAssetLibrary.does_asset_exist(a['damaged_target']) else unreal.EditorAssetLibrary.duplicate_asset(a['target'],a['damaged_target'])
        assert isinstance(damaged,unreal.StaticMesh)
        for i,s in enumerate(damaged.get_editor_property('static_materials')):damaged.set_material(i,unreal.load_asset(bindings[True][str(s.get_editor_property('imported_material_slot_name'))]))
        damagecheck=check_mesh(damaged,a,bindings[True],subsystem);save(damaged)
        results.append(dict(key=a['key'],issue=a['issue'],version=a['version'],healthy=healthy,damaged=damagecheck,materials=materials))
        OUT.mkdir(parents=True,exist_ok=True);write(OUT/'ApprovedCivic-Progress.json',dict(plan_sha256=sha(PLAN),assets=results,status='partial'))
        unreal.log('APPROVED_CIVIC_NATIVE_READY '+a['key'])
    unchanged(protected)
    files=[dict(file=f,sha256=sha(ROOT/f),bytes=(ROOT/f).stat().st_size) for f in dict.fromkeys(FILES)]
    assert len(results)==15
    write(OUT/'ApprovedCivicNative.json',dict(issue=157,plan_sha256=sha(PLAN),assets=results,files=files,shared_textures=shared,shared_inputs_unchanged=True))
    (OUT/'Files.txt').write_text('\n'.join(f['file'] for f in files)+'\n',encoding='utf-8')
    unreal.log('APPROVED_CIVIC_NATIVE_PASS 15 buildings,30 whole/damaged meshes,45 LOD sources,90 material regions')

if __name__=='__main__':main()
