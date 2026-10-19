"""Focused factory entrance/hardware material contracts; no import side effects."""
STEEL_SLOT = 'IndustrialDoorMetal'
METALS = [
    dict(name='IndustrialDoorSteel', base_color=[.48,.52,.57], metallic=1., roughness=.22,
         native_material='/Game/CitySim/Art/LowDensity/Materials/M_IndustrialDoorSteel'),
    dict(name='IndustrialHardwareSteel', base_color=[.30,.34,.38], metallic=1., roughness=.18,
         native_material='/Game/CitySim/Art/LowDensity/Materials/M_IndustrialHardwareSteel')]

def blender_materials():
    import bpy
    result=[]
    for spec in METALS:
        m=bpy.data.materials.get(spec['name']) or bpy.data.materials.new(spec['name']);m.use_nodes=True
        p=m.node_tree.nodes.get('Principled BSDF')
        p.inputs['Base Color'].default_value=(*spec['base_color'],1)
        p.inputs['Metallic'].default_value=spec['metallic'];p.inputs['Roughness'].default_value=spec['roughness']
        m.diffuse_color=(*spec['base_color'],1);result.append(m)
    return result

def face_key(mesh,face):
    return tuple(sorted(tuple(round(float(x),5) for x in mesh.vertices[v].co) for v in face.vertices))

def apply_healthy(obj,reference,entrance,slots,bindings):
    door,hardware=blender_materials()
    assert STEEL_SLOT not in slots
    import importlib.util
    from pathlib import Path
    from CityBuildingEntrances import freight_door,personnel_door
    spec=importlib.util.spec_from_file_location('IndustrialMetalBuilder',Path(__file__).with_name('Create-CityLowDensityVariants.py'))
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    b=module.Builder(reference)
    for freight in entrance['freight']:
        freight_door(b,freight['center_x_m'],freight['width_m'],freight['floor_m'],freight['height_m'],
                     roller=freight['style']=='roller shutter',glazed=freight['style']=='glazed sectional')
    personnel=personnel_door(b,'ABCD'.index(obj.name[-1]))
    # The final six generated faces are the personnel path; retain its surface.
    excluded=set(personnel['faces'][-6:])
    keys={tuple(sorted(tuple(round(float(x),5) for x in b.vertices[v]) for v in face))
          for i,face in enumerate(b.faces) if b.slots[i]==1 and i not in excluded}
    selected=[p.index for p in obj.data.polygons if face_key(obj.data,p) in keys]
    assert len(selected)==len(keys) and len(selected)>30
    index=len(slots);obj.data.materials.append(door);slots.append(STEEL_SLOT)
    for i in selected:obj.data.polygons[i].material_index=index
    obj.data.materials[slots.index('IndustrialMechanical')]=hardware
    bindings[STEEL_SLOT]=METALS[0]['native_material'];bindings['IndustrialMechanical']=METALS[1]['native_material']
    return selected

def import_native_materials(specs):
    import unreal
    lib=unreal.MaterialEditingLibrary;assets=unreal.AssetToolsHelpers.get_asset_tools();result=[]
    for spec in specs:
        path=spec['native_material'];m=unreal.load_asset(path)
        if not m:m=assets.create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],unreal.Material,unreal.MaterialFactoryNew())
        lib.delete_all_material_expressions(m);m.set_editor_property('used_with_instanced_static_meshes',True)
        m.set_editor_property('use_hq_forward_reflections',True);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
        c=lib.create_material_expression(m,unreal.MaterialExpressionConstant3Vector)
        c.set_editor_property('constant',unreal.LinearColor(*spec['base_color'],1))
        assert lib.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
        for key,prop in [('metallic',unreal.MaterialProperty.MP_METALLIC),('roughness',unreal.MaterialProperty.MP_ROUGHNESS)]:
            n=lib.create_material_expression(m,unreal.MaterialExpressionConstant);n.set_editor_property('r',spec[key])
            assert lib.connect_material_property(n,'',prop)
        lib.layout_material_expressions(m);lib.recompile_material(m)
        stats=lib.get_statistics(m);assert stats.get_editor_property('num_pixel_shader_instructions')>0
        assert unreal.EditorAssetLibrary.save_loaded_asset(m)
        result.append(dict(spec,hq_forward_reflections=True,pixel_instructions=stats.get_editor_property('num_pixel_shader_instructions')))
    return result
