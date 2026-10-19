"""Soft CPU sprite materials using standard Unreal nodes; no custom shader."""
import unreal
from pathlib import Path
lib=unreal.MaterialEditingLibrary; assets=unreal.EditorAssetLibrary; tools=unreal.AssetToolsHelpers.get_asset_tools()
for name, additive in (('M_CityFlameParticle',True),('M_CitySmokeParticle',False),('M_CityWaterParticle',False)):
    path='/Game/CitySim/Art/Materials/'+name
    m=assets.load_asset(path) or tools.create_asset(name,'/Game/CitySim/Art/Materials',unreal.Material,unreal.MaterialFactoryNew())
    # These materials can be rooted by particle CDO references. Update the
    # existing output without destroying expressions referenced by the engine.
    old = lib.get_material_property_input_node(m, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if old:
        if old.get_editor_property('desc') == 'CitySafetyBrightness':
            brightness = old
        else:
            output = lib.get_material_property_input_node_output_name(m, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            brightness = lib.create_material_expression(m, unreal.MaterialExpressionMultiply, 500, -100)
            brightness.set_editor_property('desc', 'CitySafetyBrightness')
            lib.connect_material_expressions(old, output, brightness, 'A')
            lib.connect_material_property(brightness, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        brightness.set_editor_property('const_b', 3.0 if additive else 1.6 if 'Water' in name else 1.0)
        lib.recompile_material(m)
        if not assets.save_loaded_asset(m): raise RuntimeError('Particle material save failed')
        continue
    lib.delete_all_material_expressions(m)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE if additive else unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided',True); m.set_editor_property('used_with_particle_sprites',True)
    uv=lib.create_material_expression(m,unreal.MaterialExpressionTextureCoordinate,-600,100)
    center=lib.create_material_expression(m,unreal.MaterialExpressionSubtract,-400,100); center.set_editor_property('const_b',.5)
    lib.connect_material_expressions(uv,'',center,'A')
    dot=lib.create_material_expression(m,unreal.MaterialExpressionDotProduct,-200,100)
    lib.connect_material_expressions(center,'',dot,'A'); lib.connect_material_expressions(center,'',dot,'B')
    scale=lib.create_material_expression(m,unreal.MaterialExpressionMultiply,0,100); scale.set_editor_property('const_b',4)
    lib.connect_material_expressions(dot,'',scale,'A')
    inv=lib.create_material_expression(m,unreal.MaterialExpressionOneMinus,150,100); lib.connect_material_expressions(scale,'',inv,'')
    particle=lib.create_material_expression(m,unreal.MaterialExpressionParticleColor,-100,-100)
    alpha=lib.create_material_expression(m,unreal.MaterialExpressionMultiply,300,100)
    lib.connect_material_expressions(inv,'',alpha,'A'); lib.connect_material_expressions(particle,'A',alpha,'B')
    lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
    brightness=lib.create_material_expression(m,unreal.MaterialExpressionMultiply,300,-100)
    brightness.set_editor_property('desc','CitySafetyBrightness')
    brightness.set_editor_property('const_b',3.0 if additive else 1.6 if 'Water' in name else 1.0)
    lib.connect_material_expressions(particle,'RGB',brightness,'A')
    lib.connect_material_property(brightness,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.layout_material_expressions(m); lib.recompile_material(m)
    if not assets.save_loaded_asset(m): raise RuntimeError('Particle material save failed')
(root:=Path(__file__).resolve().parents[1]).joinpath('Artifacts/Diagnostics/D44-safety-material-ready.txt').write_text('ready',encoding='utf-8')
