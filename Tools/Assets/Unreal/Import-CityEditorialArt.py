"""CI/native import of pinned editorial derived textures and repairs-only material mapping."""

import sys
from pathlib import Path
sys.path.insert(0, str(next(p for p in Path(__file__).resolve().parents if (p / "CityToolPaths.py").is_file())))
from CityToolPaths import repository_root
from Assets.Manifests.CityEditorialArt import editorial_entries
from pathlib import Path
import json,hashlib
import unreal


def main():
    root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
    doc=json.loads((root/'ArtSource/Derived/EditorialArt/manifest.json').read_text())
    assets=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
    changed=[];textures={}
    repairs_only=globals().get('repairs_only',False)
    def save(asset):
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
        path=asset.get_path_name().split('.')[0]
        if path not in changed:changed.append(path)
    for entry in editorial_entries(doc).values():
        if repairs_only and entry['usage']!='repair_display':continue
        source=root/entry['file']
        assert hashlib.sha256(source.read_bytes()).hexdigest()==entry['sha256']
        folder,name=entry['native'].rsplit('/',1)
        task=unreal.AssetImportTask()
        for k,v in dict(filename=str(source),destination_path=folder,destination_name=name,automated=True,replace_existing=True,save=False).items():task.set_editor_property(k,v)
        task.factory=unreal.TextureFactory()
        assets.import_asset_tasks([task])
        t=unreal.load_asset(entry['native']);assert t
        assert unreal.CityEditorAssetLibrary.repoint_imported_source(t,str(source),0), entry['native']
        t.set_editor_property('srgb',True)
        t.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP)
        t.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
        if entry['usage']=='newspaper_illustration':
            t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            t.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
            t.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        else:
            t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_DEFAULT)
            t.set_editor_property('compression_no_alpha',False)
        save(t);textures[entry['key']]=t

    path='/Game/CitySim/Art/Materials/M_RepairDisplayWindows'
    m=unreal.load_asset(path) or assets.create_asset('M_RepairDisplayWindows','/Game/CitySim/Art/Materials',unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('used_with_instanced_static_meshes',True)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
    def node(cls,**props):
        n=lib.create_material_expression(m,cls)
        for k,v in props.items():n.set_editor_property(k,v)
        return n
    def link(src,dst,pin='',output=''):
        assert lib.connect_material_expressions(src,output,dst,pin)
    def scalar(v):return node(unreal.MaterialExpressionConstant,r=v)
    def color(v):return node(unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(*v,1))
    def vec2(x,y):return node(unreal.MaterialExpressionConstant2Vector,r=x,g=y)
    def mul(a,b):
        n=node(unreal.MaterialExpressionMultiply);link(a,n,'A');link(b,n,'B');return n
    def add(a,b):
        n=node(unreal.MaterialExpressionAdd);link(a,n,'A');link(b,n,'B');return n
    def mix(a,b,alpha,output=''):
        n=node(unreal.MaterialExpressionLinearInterpolate);link(a,n,'A');link(b,n,'B');link(alpha,n,'Alpha',output);return n
    def prop(n,p,output=''):assert lib.connect_material_property(n,output,p)
    vertex=node(unreal.MaterialExpressionVertexColor)
    glass=node(unreal.MaterialExpressionTextureSample,texture=unreal.load_asset('/Game/CitySim/Art/Textures/T_HomeGlass'),sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    # Per-instance choice is cosmetic, fixed within a draw; no simulation or tick.
    random=node(unreal.MaterialExpressionPerInstanceRandom)
    index=node(unreal.MaterialExpressionFloor);link(mul(random,scalar(3.999)),index)
    row=node(unreal.MaterialExpressionFloor);link(mul(index,scalar(.5)),row)
    negative=mul(row,scalar(-2));column=add(index,negative)
    quadrant=node(unreal.MaterialExpressionAppendVector);link(column,quadrant,'A');link(row,quadrant,'B')
    uv=node(unreal.MaterialExpressionTextureCoordinate)
    uv=add(add(mul(uv,vec2(242/512,330/1024)),vec2(7/512,91/1024)),mul(quadrant,scalar(.5)))
    display=node(unreal.MaterialExpressionTextureSample,texture=textures['repairs'],sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR);link(uv,display)
    local=node(unreal.MaterialExpressionLocalPosition)
    z=node(unreal.MaterialExpressionComponentMask,r=False,g=False,b=True,a=False);link(local,z)
    ground=node(unreal.MaterialExpressionIf);link(z,ground,'A');link(scalar(450),ground,'B')
    link(scalar(0),ground,'A > B');link(scalar(1),ground,'A == B');link(scalar(1),ground,'A < B')
    # FBX inspection pins regular panes220x300cm. Unreal converts Blender's
    # front(-Y) to+Y; only the central front entrance lies inside this box.
    # The box includes the door leaves, transom and all four variants' sidelights.
    x=node(unreal.MaterialExpressionComponentMask,r=True,g=False,b=False,a=False);link(local,x)
    y=node(unreal.MaterialExpressionComponentMask,r=False,g=True,b=False,a=False);link(local,y)
    absolute=node(unreal.MaterialExpressionAbs);link(x,absolute)
    central=node(unreal.MaterialExpressionIf);link(absolute,central,'A');link(scalar(250),central,'B')
    link(scalar(0),central,'A > B');link(scalar(1),central,'A == B');link(scalar(1),central,'A < B')
    front=node(unreal.MaterialExpressionIf);link(y,front,'A');link(scalar(600),front,'B')
    link(scalar(1),front,'A > B');link(scalar(0),front,'A == B');link(scalar(0),front,'A < B')
    outside_entry=node(unreal.MaterialExpressionOneMinus);link(mul(central,front),outside_entry)
    display_alpha=node(unreal.MaterialExpressionMultiply)
    link(mul(ground,outside_entry),display_alpha,'A');link(display,display_alpha,'B','A')
    surface=mix(glass,display,display_alpha)
    prop(mix(surface,color((.76,.73,.64)),vertex,'A'),unreal.MaterialProperty.MP_BASE_COLOR)
    prop(mix(scalar(.18),scalar(.55),vertex,'A'),unreal.MaterialProperty.MP_ROUGHNESS)
    prop(scalar(.5),unreal.MaterialProperty.MP_SPECULAR)
    inverse=node(unreal.MaterialExpressionOneMinus);link(vertex,inverse,'','A')
    clock=node(unreal.MaterialExpressionCollectionParameter,collection=unreal.load_asset('/Game/CitySim/Art/Materials/MPC_CityLighting'),parameter_name='NightGlow')
    prop(mul(mul(inverse,clock),color((.8,.46,.16))),unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.layout_material_expressions(m);lib.recompile_material(m);save(m)
    mesh_records=[]
    paths=['/Game/CitySim/Art/Meshes/SM_Commercial'+d+'_StoreRepairs' for d in ('Low','Medium','High')]
    # Current low-density appearances override the older commercial fallback mesh.
    # Follow their actual signed repair copies without rewriting the catalog.
    paths += ['/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Shop_'+v+'_repairs' for v in ('A','B','C','D')]
    for path in paths:
        mesh=unreal.load_asset(path);assert mesh,path
        before=[s.material_interface.get_path_name() for s in mesh.static_materials]
        slots=[str(s.material_slot_name) for s in mesh.static_materials]
        index=slots.index('BuildingWindowOpaque')
        if not repairs_only:mesh.set_material(index,m);save(mesh)
        else:assert mesh.get_material(index)==m,'Existing repairs mapping changed'
        after=[s.material_interface.get_path_name() for s in mesh.static_materials]
        assert all(a==b for i,(a,b) in enumerate(zip(before,after)) if i!=index)
        mesh_records.append(dict(mesh=mesh.get_path_name(),slot=index,slots=slots,before=before,after=after))
    output=root/'Artifacts/CIAssetPreparation';output.mkdir(parents=True,exist_ok=True)
    files=['CitySimVR/Content/'+p.removeprefix('/Game/')+'.uasset' for p in changed]
    receipt=dict(task='D122',assets=changed,files=files,repairs=mesh_records,ground_display_top_cm=450,entrance_excluded=dict(abs_local_x_lt_cm=250,local_y_gt_cm=600),pane_cm=[220,300],fit='uniform contain; alpha reveals original glass',upper_glass_frames_night_glow_preserved=True,geometry_changed=False)
    (output/'EditorialArt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    (output/'Files.txt').write_text('\n'.join(files)+'\n')
    print('CITYSIM_EDITORIAL_ART_IMPORTED_PASS',len(changed),'assets; entrance excluded; uniform contain; 7 repair meshes; no geometry/gameplay/catalog change')


if __name__ == '__main__':
    main()
