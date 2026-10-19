"""D104 controlled reflective-plane test: visible cues vs reflection-only captures."""
from pathlib import Path
import json
import sys
import time
import traceback
import unreal

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'Artifacts/D104/Reflectivity'
BUILT = '-CityReflectionBuild' in unreal.SystemLibrary.get_command_line()
PERSPECTIVE = '-CityReflectionPerspective' in unreal.SystemLibrary.get_command_line()
CONTROLS = '-CityReflectionControls' in unreal.SystemLibrary.get_command_line()
POLISHED = '-CityReflectionPolished' in unreal.SystemLibrary.get_command_line()
if BUILT: OUT = OUT/('Polished' if POLISHED else 'PerspectiveControls' if CONTROLS else 'Perspective' if PERSPECTIVE else 'Built')
PROJECT = ROOT/'Artifacts/D104/UnrealProject'
sys.path.insert(0, str(ROOT/'Tools'))
from CityUnrealPreview import digest, environment


def start():
    assert Path(unreal.Paths.project_dir()).resolve() == PROJECT.resolve()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT/'Error.txt').unlink(missing_ok=True)
    assert unreal.SystemLibrary.get_console_variable_bool_value('r.ForwardShading')
    batch = json.loads((ROOT/'Artifacts/D104/Batch.json').read_text())
    assets = unreal.AssetToolsHelpers.get_asset_tools(); lib = unreal.MaterialEditingLibrary

    def material(name, color, rough=.02, unlit=False, hq=False):
        path = '/Game/CityPreview/Reflectivity/M_'+name
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            m = unreal.load_asset(path)
            assert lib.get_statistics(m).get_editor_property('num_pixel_shader_instructions') > 0
            return m
        m = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset('M_'+name,
            '/Game/CityPreview/Reflectivity', unreal.Material, unreal.MaterialFactoryNew())
        lib.delete_all_material_expressions(m)
        m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
        m.set_editor_property('use_hq_forward_reflections', hq)
        if unlit: m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        n = lib.create_material_expression(m, unreal.MaterialExpressionConstant3Vector)
        n.set_editor_property('constant', unreal.LinearColor(*color, 1))
        assert lib.connect_material_property(n, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR if unlit else unreal.MaterialProperty.MP_BASE_COLOR)
        if not unlit:
            for property, value in [(unreal.MaterialProperty.MP_ROUGHNESS, rough), (unreal.MaterialProperty.MP_METALLIC, 1)]:
                node = lib.create_material_expression(m, unreal.MaterialExpressionConstant); node.set_editor_property('r', value)
                assert lib.connect_material_property(node, '', property)
        lib.layout_material_expressions(m); lib.recompile_material(m)
        assert unreal.EditorAssetLibrary.save_loaded_asset(m)
        assert lib.get_statistics(m).get_editor_property('num_pixel_shader_instructions') > 0
        return m

    smooth = material('SmoothMetal', (.72, .72, .72), .02)
    rough = material('RoughMetal', (.72, .72, .72), .36)
    red = material('RedCue', (.9, .012, .005), unlit=True)
    green = material('GreenCue', (.005, .85, .018), unlit=True)
    corrugated = unreal.load_asset('/Game/CityPreview/Materials/M_Factory_Corrugated'); assert corrugated
    assert lib.get_statistics(corrugated).get_editor_property('num_pixel_shader_instructions') > 0
    if BUILT:
        smooth_hq = material('SmoothMetalHQ', (.72, .72, .72), .02, hq=True)
        rough_hq = material('RoughMetalHQ', (.72, .72, .72), .36, hq=True)
        copy_path = '/Game/CityPreview/Reflectivity/M_CorrugatedHQ'
        if not unreal.EditorAssetLibrary.does_asset_exist(copy_path):
            corrugated_hq = assets.duplicate_asset('M_CorrugatedHQ', '/Game/CityPreview/Reflectivity', corrugated)
            corrugated_hq.set_editor_property('use_hq_forward_reflections', True)
            lib.recompile_material(corrugated_hq); assert unreal.EditorAssetLibrary.save_loaded_asset(corrugated_hq)
        else: corrugated_hq = unreal.load_asset(copy_path)
        if POLISHED:
            smooth_polished = material('SmoothMetal005HQ', (.72, .72, .72), .05, hq=True)
            copy_path = '/Game/CityPreview/Reflectivity/M_CorrugatedPolishedHQ'
            if not unreal.EditorAssetLibrary.does_asset_exist(copy_path):
                corrugated_polished = assets.duplicate_asset('M_CorrugatedPolishedHQ', '/Game/CityPreview/Reflectivity', corrugated_hq)
                node=lib.create_material_expression(corrugated_polished, unreal.MaterialExpressionConstant)
                node.set_editor_property('r', .05)
                assert lib.connect_material_property(node, '', unreal.MaterialProperty.MP_ROUGHNESS)
                lib.recompile_material(corrugated_polished); assert unreal.EditorAssetLibrary.save_loaded_asset(corrugated_polished)
            else: corrugated_polished = unreal.load_asset(copy_path)
        for surface in [smooth_hq,rough_hq,corrugated_hq]+([smooth_polished,corrugated_polished] if POLISHED else []):
            assert lib.get_statistics(surface).get_editor_property('num_pixel_shader_instructions') > 0
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    def spawn(kind, location=(0, 0, 0), rotation=None):
        return actor_subsystem.spawn_actor_from_class(kind, unreal.Vector(*location), rotation or unreal.Rotator())

    def mesh(path, location, scale, surface):
        obj = spawn(unreal.StaticMeshActor, location)
        obj.static_mesh_component.set_static_mesh(unreal.load_asset(path))
        obj.static_mesh_component.set_mobility(unreal.ComponentMobility.STATIC)
        obj.set_actor_scale3d(unreal.Vector(*scale)); obj.static_mesh_component.set_material(0, surface)
        return obj

    panel = mesh('/Engine/BasicShapes/Plane', (0, 0, 0), (6, 6, 1), smooth)
    red_cube = mesh('/Engine/BasicShapes/Cube', (-120, 40, 150), (1, 1, 1), red)
    green_sphere = mesh('/Engine/BasicShapes/Sphere', (120, 40, 150), (1, 1, 1), green)
    if BUILT:
        red_cube.set_actor_location(unreal.Vector(-120, 180, 100), False, False)
        green_sphere.set_actor_location(unreal.Vector(120, 180, 100), False, False)
    sun = spawn(unreal.DirectionalLight, (0, 0, 1000), unreal.Rotator(pitch=-50, yaw=-110, roll=0))
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE); sun.light_component.set_intensity(3)
    sky = spawn(unreal.SkyLight, (0, 0, 1000))
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    sky.light_component.set_cubemap(unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
    sky.light_component.set_intensity(1); sky.light_component.recapture_sky()
    capture = spawn(unreal.SceneCapture2D, (0, -900, 550))
    aim = unreal.Vector(0, 0, 20)
    capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(capture.get_actor_location(), aim), False)
    cap = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
    rt = unreal.RenderingLibrary.create_render_target2d(world, 1000, 1000, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    rt.set_editor_property('target_gamma', 2.2)
    for key, value in dict(texture_target=rt, capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,
            capture_every_frame=False, capture_on_movement=False, always_persist_rendering_state=True,
            projection_type=unreal.CameraProjectionMode.ORTHOGRAPHIC, ortho_width=850).items(): cap.set_editor_property(key, value)
    if PERSPECTIVE:
        cap.set_editor_property('projection_type', unreal.CameraProjectionMode.PERSPECTIVE)
        cap.set_editor_property('fov_angle', 50)
    pp = cap.get_editor_property('post_process_settings')
    for key, value in dict(override_auto_exposure_method=True, auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,
            override_auto_exposure_bias=True, auto_exposure_bias=0,
            override_auto_exposure_apply_physical_camera_exposure=True, auto_exposure_apply_physical_camera_exposure=False,
            override_bloom_intensity=True, bloom_intensity=0, override_motion_blur_amount=True, motion_blur_amount=0).items(): pp.set_editor_property(key, value)
    cap.set_editor_property('post_process_settings', pp)
    materials = [('Smooth_002', smooth), ('Rough_036', rough), ('D104_Corrugated', corrugated)]
    conditions = [dict(name='A_NoProbe', probe=None, mixing=0),
        dict(name='B_UniformCube', probe='uniform', mixing=0),
        dict(name='C_CapturedScene', probe='scene', mixing=0),
        dict(name='D_CapturedScene_Mixing', probe='scene', mixing=1)]
    if BUILT:
        materials = [('Smooth_002', smooth), ('Smooth_002_HQ', smooth_hq), ('Rough_036_HQ', rough_hq),
            ('D104_Corrugated', corrugated), ('D104_Corrugated_HQ', corrugated_hq)]
        conditions = [dict(name='E_Automatic', probe='scene', mixing=0),
            dict(name='F_Built', probe='scene', mixing=0)]
    if CONTROLS:
        materials = [('Smooth_002_HQ', smooth_hq), ('Rough_036_HQ', rough_hq), ('D104_Corrugated_HQ', corrugated_hq)]
        conditions = [dict(name='G_NoProbe', probe=None, mixing=0), dict(name='H_UniformCube', probe='uniform', mixing=0),
            dict(name='I_CapturedScene', probe='scene', mixing=0), dict(name='J_CapturedScene_Mixing', probe='scene', mixing=1)]
    if POLISHED:
        materials = [('Smooth_005_HQ', smooth_polished), ('D104_Corrugated_Polished_HQ', corrugated_polished)]
        conditions = [dict(name='K_CapturedScene', probe='scene', mixing=0)]
    jobs = [(c, name, surface, hidden) for c in conditions for name, surface in materials for hidden in (False, True)]
    state = dict(index=0, phase=0, deadline=time.monotonic()+8, start=time.monotonic(), rows=[], probe=None, condition=None)

    def tick(delta):
        try:
            now = time.monotonic()
            if now < state['deadline']: return
            if now-state['start'] > 480: raise RuntimeError('Controlled reflection test timed out')
            condition, name, surface, hidden = jobs[state['index']]
            if state['phase'] == 0:
                if state['condition'] != condition['name']:
                    if condition['name'] != 'F_Built' and state['probe']: actor_subsystem.destroy_actor(state['probe']); state['probe'] = None
                    if condition['probe'] and condition['name'] != 'F_Built':
                        actor = spawn(unreal.SphereReflectionCapture, (0, 0, 20 if BUILT else 100)); state['probe'] = actor
                        probe = actor.get_component_by_class(unreal.SphereReflectionCaptureComponent)
                        probe.set_editor_property('influence_radius', 500 if BUILT else 2000)
                        if condition['probe'] == 'uniform':
                            probe.set_editor_property('reflection_source_type', unreal.ReflectionSourceType.SPECIFIED_CUBEMAP)
                            probe.set_editor_property('cubemap', environment(batch))
                        else: probe.set_editor_property('reflection_source_type', unreal.ReflectionSourceType.CAPTURED_SCENE)
                        probe.set_editor_property('brightness', 1)
                    unreal.SystemLibrary.execute_console_command(world, 'r.ReflectionEnvironmentLightmapMixing '+str(condition['mixing']))
                    if condition['name'] == 'F_Built':
                        # Lightmass pumps Slate; prevent this callback re-entering the build.
                        state['deadline'] = float('inf')
                        cap.clear_hidden_components()
                        panel.static_mesh_component.set_material(0, smooth)
                        assert unreal.EditorLoadingAndSavingUtils.save_map(world, '/Game/CityPreview/Maps/ReflectivityTest')
                        unreal.log('CITYSIM_REFLECTION_TEST_BUILD_START')
                        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).build_light_maps(unreal.LightingBuildQuality.QUALITY_PREVIEW, True)
                        unreal.log('CITYSIM_REFLECTION_TEST_BUILD_RETURNED')
                        now = time.monotonic()
                    state['condition'] = condition['name']; state['deadline'] = now+8; return
                panel.static_mesh_component.set_material(0, surface)
                cap.clear_hidden_components()
                if hidden:
                    cap.hide_actor_components(red_cube); cap.hide_actor_components(green_sphere)
                state['current'] = dict(condition=condition['name'], material=name, material_path=surface.get_path_name(),
                    roughness=.05 if POLISHED else .02 if name.startswith('Smooth') else .36, cue_hidden_from_camera=hidden,
                    hq_forward_reflections=surface.get_editor_property('use_hq_forward_reflections'),
                    projection='perspective' if PERSPECTIVE else 'orthographic',
                    cue_remains_in_reflection_scene=True, mixing=condition['mixing'], probe=condition['probe'],
                    plane_size_cm=[600, 600], camera_cm=[0, -900, 550], ortho_width_cm=850,
                    red_cube_cm=list(red_cube.get_actor_location().to_tuple()), green_sphere_cm=list(green_sphere.get_actor_location().to_tuple()),
                    probe_cm=[0, 0, 20 if BUILT else 100], influence_radius_cm=500 if BUILT else 2000,
                    baked=condition['name']=='F_Built', cooked=False, renderer='D3D11 forward')
                state['phase'] = 1; state['deadline'] = now+1.5
            elif state['phase'] == 1:
                cap.capture_scene(); state['phase'] = 2; state['deadline'] = now+.5
            else:
                filename=condition['name']+'_'+name+('_ReflectionOnly' if hidden else '_ObjectsVisible')+'.png'
                unreal.RenderingLibrary.export_render_target(world, rt, str(OUT), filename)
                assert (OUT/filename).is_file()
                state['current'].update(image=filename, sha256=digest(OUT/filename))
                state['rows'].append(state['current']); state['index'] += 1
                unreal.log('CITYSIM_REFLECTION_TEST_FRAME '+str(state['index'])+'/'+str(len(jobs))+' '+filename)
                if state['index'] == len(jobs):
                    result=dict(task='D104', experiment='Flat metal plus static red cube/green sphere', views=state['rows'],
                        objects_only_hidden_from_scene_capture=True, lightmass_built=BUILT and not (CONTROLS or POLISHED), map_saved=BUILT and not (CONTROLS or POLISHED), cooked=False,
                        reflection_updates='Preview lighting build in isolated test map' if BUILT and not (CONTROLS or POLISHED) else 'Editor automatic probe update; explicit bake not invoked',
                        forward_shading=True, scene='isolated saved Editor test map' if BUILT and not (CONTROLS or POLISHED) else 'isolated unsaved blank Editor map', production_changed=False)
                    (OUT/'Report.json').write_text(json.dumps(result, indent=2)+'\n')
                    unreal.log('CITYSIM_REFLECTION_TEST_PASS '+str(len(jobs))+' controlled captures')
                    unreal.unregister_slate_post_tick_callback(state['handle']); unreal.SystemLibrary.quit_editor()
                else: state['phase']=0; state['deadline']=now+.1
        except Exception:
            (OUT/'Error.txt').write_text(traceback.format_exc()); unreal.log_error(traceback.format_exc())
            unreal.unregister_slate_post_tick_callback(state['handle']); unreal.SystemLibrary.quit_editor()
    state['handle']=unreal.register_slate_post_tick_callback(tick)


try: start()
except Exception:
    OUT.mkdir(parents=True, exist_ok=True); (OUT/'Error.txt').write_text(traceback.format_exc())
    unreal.log_error(traceback.format_exc()); unreal.SystemLibrary.quit_editor()
