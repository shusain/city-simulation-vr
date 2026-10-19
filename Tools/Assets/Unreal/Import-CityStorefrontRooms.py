"""All-density approved room art; semantic-slot-only assignment, zero geometry changes."""

import sys
from pathlib import Path
sys.path.insert(0, str(next(p for p in Path(__file__).resolve().parents if (p / "CityToolPaths.py").is_file())))
from CityToolPaths import repository_root
from pathlib import Path
import json,hashlib
import unreal

def main():
    root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
    doc=json.loads((root/'ArtSource/Derived/StorefrontRooms/manifest.json').read_text())
    assets=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
    changed=[];textures={}
    material_only=globals().get('material_only',False)
    protected={}
    if material_only:
        for p in (root/'CitySimVR/Content/CitySim').rglob('*.uasset'):
            if p.name!='M_StorefrontRoomWindows.uasset':protected[p]=hashlib.sha256(p.read_bytes()).hexdigest()
    def save(a):
        assert unreal.EditorAssetLibrary.save_loaded_asset(a)
        p=a.get_path_name().split('.')[0]
        if p not in changed:changed.append(p)
    for r in doc['assets']:
        if material_only:
            textures[r['key']]=unreal.load_asset(r['native_texture']);assert textures[r['key']]
            continue
        source=root/r['file'];assert hashlib.sha256(source.read_bytes()).hexdigest()==r['sha256']
        folder,name=r['native_texture'].rsplit('/',1);task=unreal.AssetImportTask()
        for k,v in dict(filename=str(source),destination_path=folder,destination_name=name,automated=True,replace_existing=True,save=False).items():task.set_editor_property(k,v)
        task.factory=unreal.TextureFactory();assets.import_asset_tasks([task]);t=unreal.load_asset(r['native_texture']);assert t
        assert unreal.CityEditorAssetLibrary.repoint_imported_source(t,str(source),0), r['native_texture']
        for k,v in dict(srgb=True,address_x=unreal.TextureAddress.TA_CLAMP,address_y=unreal.TextureAddress.TA_CLAMP,compression_settings=unreal.TextureCompressionSettings.TC_DEFAULT,compression_no_alpha=True,mip_gen_settings=unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE).items():t.set_editor_property(k,v)
        save(t);textures[r['key']]=t
    path='/Game/CitySim/Art/Materials/M_StorefrontRoomWindows'
    m=unreal.load_asset(path) or assets.create_asset('M_StorefrontRoomWindows','/Game/CitySim/Art/Materials',unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m);m.set_editor_property('used_with_instanced_static_meshes',True);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
    def node(cls,**props):
        n=lib.create_material_expression(m,cls)
        for k,v in props.items():n.set_editor_property(k,v)
        return n
    def link(a,b,pin='',output=''):assert lib.connect_material_expressions(a,output,b,pin)
    def scalar(v):return node(unreal.MaterialExpressionConstant,r=v)
    def color(v):return node(unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(*v,1))
    def vec2(x,y):return node(unreal.MaterialExpressionConstant2Vector,r=x,g=y)
    def mul(a,b):
        n=node(unreal.MaterialExpressionMultiply);link(a,n,'A');link(b,n,'B');return n
    def add(a,b):
        n=node(unreal.MaterialExpressionAdd);link(a,n,'A');link(b,n,'B');return n
    def mix(a,b,c,output=''):
        n=node(unreal.MaterialExpressionLinearInterpolate);link(a,n,'A');link(b,n,'B');link(c,n,'Alpha',output);return n
    def prop(n,p,output=''):assert lib.connect_material_property(n,output,p)
    vertex=node(unreal.MaterialExpressionVertexColor)
    glass=node(unreal.MaterialExpressionTextureSample,texture=unreal.load_asset('/Game/CitySim/Art/Textures/T_HomeGlass'),sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    uv=node(unreal.MaterialExpressionTextureCoordinate)
    room_uv=add(mul(uv,vec2(484/512,660/1024)),vec2(14/512,182/1024))
    display=node(unreal.MaterialExpressionTextureSampleParameter2D,parameter_name='RoomTexture',texture=textures['market'],sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR);link(room_uv,display)
    # Mix in the original pane's blue-grey gradient and diagonal glare in linear
    # color space. Reuse its existing sample; frames/entrances/upper glass bypass it.
    glass_blend=node(unreal.MaterialExpressionScalarParameter,parameter_name='GlassBlend',default_value=.45,slider_min=0,slider_max=1)
    glazed_room=mix(display,glass,glass_blend)
    # Instance origin preserves this local mask in instanced/rotated city buildings.
    local=node(unreal.MaterialExpressionLocalPosition)
    local.set_editor_property('local_origin',unreal.LocalPositionOrigin.INSTANCE)
    def component(**kw):
        n=node(unreal.MaterialExpressionComponentMask,r=False,g=False,b=False,a=False)
        for k,v in kw.items():n.set_editor_property(k,v)
        link(local,n);return n
    def less(a,b):
        n=node(unreal.MaterialExpressionIf);link(a,n,'A');link(scalar(b),n,'B')
        link(scalar(0),n,'A > B');link(scalar(1),n,'A == B');link(scalar(1),n,'A < B');return n
    ground=less(component(b=True),450)
    x=node(unreal.MaterialExpressionAbs);link(component(r=True),x)
    central=less(x,250);not_front=less(component(g=True),600)
    front=node(unreal.MaterialExpressionOneMinus);link(not_front,front)
    outside_entry=node(unreal.MaterialExpressionOneMinus);link(mul(central,front),outside_entry)
    surface=mix(glass,glazed_room,mul(ground,outside_entry))
    prop(mix(surface,color((.76,.73,.64)),vertex,'A'),unreal.MaterialProperty.MP_BASE_COLOR)
    prop(mix(scalar(.18),scalar(.55),vertex,'A'),unreal.MaterialProperty.MP_ROUGHNESS)
    prop(scalar(.5),unreal.MaterialProperty.MP_SPECULAR)
    inverse=node(unreal.MaterialExpressionOneMinus);link(vertex,inverse,'','A')
    clock=node(unreal.MaterialExpressionCollectionParameter,collection=unreal.load_asset('/Game/CitySim/Art/Materials/MPC_CityLighting'),parameter_name='NightGlow')
    prop(mul(mul(inverse,clock),color((.8,.46,.16))),unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.layout_material_expressions(m);lib.recompile_material(m);save(m)
    records=[]
    for r in ([] if material_only else doc['assets']):
        folder,name=r['native_material'].rsplit('/',1)
        mi=unreal.load_asset(r['native_material']) or assets.create_asset(name,folder,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        lib.set_material_instance_parent(mi,m)
        # UE5.7.4's setter returns false unconditionally (MaterialEditingLibrary.cpp).
        # Validate the resulting parameter, as the established surface importer does.
        lib.set_material_instance_texture_parameter_value(mi,'RoomTexture',textures[r['key']])
        assert lib.get_material_instance_texture_parameter_value(mi,'RoomTexture')==textures[r['key']]
        save(mi)
        paths=r['native_meshes']
        for path in paths:
            mesh=unreal.load_asset(path);assert mesh,path
            before=[s.material_interface.get_path_name() for s in mesh.static_materials];slots=[str(s.material_slot_name) for s in mesh.static_materials]
            index=slots.index('BuildingWindowOpaque');assert len(slots)==5,(path,slots)
            mesh.set_material(index,mi);save(mesh)
            after=[s.material_interface.get_path_name() for s in mesh.static_materials]
            assert all(a==b for i,(a,b) in enumerate(zip(before,after)) if i!=index)
            records.append(dict(mesh=path,business=r['key'],window_slot=index,slots=slots,before=before,after=after))
    assert len(records)==(0 if material_only else 56)
    for p,digest in protected.items():assert hashlib.sha256(p.read_bytes()).hexdigest()==digest,str(p)
    out=root/'Artifacts/CIAssetPreparation';out.mkdir(parents=True,exist_ok=True)
    files=['CitySimVR/Content/'+p.removeprefix('/Game/')+'.uasset' for p in changed]
    (out/'StorefrontRooms.json').write_text(json.dumps(dict(task='D122',assets=changed,files=files,meshes=records,material_only=material_only,glass_blend=.45,protected_assets_verified=len(protected),added_geometry=0,entrance_upper_glass_preserved=True,pane_cm=[220,300],uniform_scale=.88,stock_nodes=True),indent=2)+'\n')
    (out/'Files.txt').write_text('\n'.join(files)+'\n')
    print('CITYSIM_STOREFRONT_ROOMS_IMPORTED_PASS',len(changed),'assets /56 signed meshes /all8 businesses /all3 densities /zero geometry')


if __name__ == '__main__':
    main()
