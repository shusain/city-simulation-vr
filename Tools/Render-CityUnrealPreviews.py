"""Tick-driven offscreen Unreal captures from explicit paired-preview batch cameras."""
from pathlib import Path
import json
import math
import re
import sys
import time
import traceback
import unreal

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'Tools'))
from CityUnrealPreview import load_batch, unreal_cm, digest, environment


def start():
    match = re.search(r'-CityPreviewBatch=(?:"([^"]+)"|(\S+))', unreal.SystemLibrary.get_command_line())
    assert match
    batch = load_batch(match.group(1) or match.group(2)); out = ROOT/batch['output']
    assert Path(unreal.Paths.project_dir()).resolve() == (ROOT/batch['project']).parent.resolve()
    (out/'CaptureError.txt').unlink(missing_ok=True)
    assert unreal.SystemLibrary.get_console_variable_bool_value('r.ForwardShading')
    # Resolve the whole render set before starting timed captures. Compiled
    # statistics force material readiness instead of photographing fallbacks.
    loaded_meshes = {e['native_mesh']: unreal.load_asset(e['native_mesh']) for e in batch['renders']}
    assert all(loaded_meshes.values())
    material_paths = set()
    for entry in batch['renders']:
        material_paths.update(batch['material_mapping'][entry['category']].values())
        material_paths.update(entry['overrides'].values())
    for path in material_paths:
        material = unreal.load_asset(path); assert material, path
        parent = material
        while isinstance(parent, unreal.MaterialInstance): parent = parent.get_editor_property('parent')
        assert unreal.MaterialEditingLibrary.get_statistics(parent).get_editor_property('num_pixel_shader_instructions') > 0, path
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    # Reuse D79's existing dynamic-room/rough-metal policy: no black baked
    # indirect-light data should suppress the preview's reflection cubemap.
    unreal.SystemLibrary.execute_console_command(world, 'r.ReflectionEnvironmentLightmapMixing 0')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    def spawn(kind, location=(0, 0, 0), rotation=None):
        return actors.spawn_actor_from_class(kind, unreal.Vector(*location), rotation or unreal.Rotator())

    building = spawn(unreal.StaticMeshActor); component = building.static_mesh_component
    citizen = spawn(unreal.StaticMeshActor)
    citizen.static_mesh_component.set_static_mesh(unreal.load_asset('/Game/CitySim/Art/PC/Meshes/SM_Citizen_PC_Casual'))
    citizen.set_actor_rotation(unreal.Rotator(pitch=0, yaw=-90, roll=0), False)
    citizen.set_actor_scale3d(unreal.Vector(1.15, 1.15, 1.15))
    ground = spawn(unreal.StaticMeshActor)
    ground.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
    ground.set_actor_scale3d(unreal.Vector(800, 800, 1))
    floor = unreal.MaterialLibrary.create_dynamic_material_instance(world, unreal.load_asset('/Game/CitySim/Art/Materials/M_CityColor'))
    floor.set_vector_parameter_value('CityColor', unreal.LinearColor(.16, .19, .23, 1))
    ground.static_mesh_component.set_material(0, floor)
    sun = spawn(unreal.DirectionalLight, (0, 0, 2000), unreal.Rotator(pitch=-38, yaw=-132, roll=0))
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun.light_component.set_intensity(batch['lighting']['sun_intensity'])
    sky = spawn(unreal.SkyLight, (0, 0, 2000))
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    sky.light_component.set_cubemap(unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
    sky.light_component.set_intensity(batch['lighting']['sky_intensity']); sky.light_component.recapture_sky()
    # Forward shading needs a local reflection source for bare-metal surfaces.
    reflection = spawn(unreal.SphereReflectionCapture, (0, 0, 400))
    probe = reflection.get_component_by_class(unreal.SphereReflectionCaptureComponent)
    probe.set_editor_property('influence_radius', 10000)
    probe.set_editor_property('reflection_source_type', unreal.ReflectionSourceType.SPECIFIED_CUBEMAP)
    reflection_cube = environment(batch)
    probe.set_editor_property('cubemap', reflection_cube)
    probe.set_editor_property('brightness', 1)
    unreal.log('CITYSIM_PREVIEW_REFLECTION_CONTEXT '+str(dict(
        force_no_precomputed=world.get_world_settings().get_editor_property('force_no_precomputed_lighting'),
        reflection_environment=unreal.SystemLibrary.get_console_variable_int_value('r.ReflectionEnvironment'),
        static_lighting=unreal.SystemLibrary.get_console_variable_int_value('r.AllowStaticLighting'),
        cube=reflection_cube.get_path_name())))
    collection = unreal.load_asset('/Game/CitySim/Art/Materials/MPC_CityLighting')
    unreal.MaterialLibrary.set_scalar_parameter_value(world, collection, 'NightGlow', 0)
    capture = spawn(unreal.SceneCapture2D); cap = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
    size = batch['resolution']
    target = unreal.RenderingLibrary.create_render_target2d(world, size, size, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    target.set_editor_property('target_gamma', 2.2)
    for key, value in dict(texture_target=target, capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,
            capture_every_frame=False, capture_on_movement=False, projection_type=unreal.CameraProjectionMode.ORTHOGRAPHIC,
            always_persist_rendering_state=True).items(): cap.set_editor_property(key, value)
    pp = cap.get_editor_property('post_process_settings')
    for key, value in dict(override_auto_exposure_method=True, auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,
            override_auto_exposure_bias=True, auto_exposure_bias=batch['lighting']['exposure_bias'],
            override_auto_exposure_apply_physical_camera_exposure=True, auto_exposure_apply_physical_camera_exposure=False,
            override_bloom_intensity=True, bloom_intensity=0, override_motion_blur_amount=True, motion_blur_amount=0).items(): pp.set_editor_property(key, value)
    cap.set_editor_property('post_process_settings', pp)
    state = dict(phase=0, index=0, deadline=time.monotonic()+8, start=time.monotonic(), records=[])

    def tick(delta):
        try:
            now = time.monotonic()
            if now < state['deadline']: return
            if now-state['start'] > max(300, len(batch['renders'])*15): raise RuntimeError('Preview capture timed out')
            entry = batch['renders'][state['index']]
            directory = out/'Unreal'/entry['group']/entry['view']; directory.mkdir(parents=True, exist_ok=True)
            if state['phase'] == 0:
                mesh = loaded_meshes[entry['native_mesh']]
                component.set_static_mesh(mesh)
                slots = [str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')]
                # Reset every section between variants; overrides must never leak into the next render.
                for i, slot in enumerate(slots):
                    path = entry['overrides'].get(slot) or batch['material_mapping'][entry['category']][slot]
                    material = unreal.load_asset(path); assert material, path; component.set_material(i, material)
                ground.set_actor_location(unreal.Vector(0, 0, entry['floor_z_m']*100), False, True)
                b = citizen.static_mesh_component.static_mesh.get_bounding_box()
                citizen.set_actor_location(unreal.Vector(-400, -entry['ruler_front_m']*100, entry['ruler_feet_m']*100-b.min.z*1.15), False, True)
                origin, extent = citizen.get_actor_bounds(False)
                assert abs(extent.z*2-143.175) < .2 and abs(origin.z-extent.z-entry['ruler_feet_m']*100) < .2
                fit = entry['camera']; aim = unreal.Vector(*unreal_cm(fit['target_m']))
                reference_position = unreal_cm(fit['position_m'])
                # The orthographic near plane must start above the floor even
                # at its bottom edge. Recede on the SAME view axis: projection,
                # target and width stay identical to the Blender reference.
                offset = [p-a for p, a in zip(fit['position_m'], fit['target_m'])]
                length = math.sqrt(sum(v*v for v in offset))
                up_z = math.sqrt(1-(offset[2]/length)**2)
                scale = max(1, (entry['floor_z_m']+fit['ortho_width_m']*.5*up_z+1-fit['target_m'][2])/offset[2])
                position = [a+b*scale for a, b in zip(fit['target_m'], offset)]
                location = unreal.Vector(*unreal_cm(position))
                capture.set_actor_location(location, False, True)
                capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(location, aim), False)
                cap.set_editor_property('ortho_width', fit['ortho_width_m']*100)
                state['current'] = dict(**entry, camera_cm=unreal_cm(position), camera_reference_cm=reference_position,
                    camera_axis_recede_factor=scale, reflection_cubemap=reflection_cube.get_path_name().split('.')[0],
                    reflection_lightmap_mixing=unreal.SystemLibrary.get_console_variable_int_value('r.ReflectionEnvironmentLightmapMixing'),
                    material_paths=[component.get_material(i).get_path_name() for i in range(len(slots))],
                    ruler_height_cm=extent.z*2, ruler_feet_cm=origin.z-extent.z)
                state['phase'] = 1; state['deadline'] = now+1.5
            elif state['phase'] == 1:
                cap.capture_scene(); state['phase'] = 2; state['deadline'] = now+.5
            else:
                filename = entry['key']+'.png'; path = directory/filename
                unreal.RenderingLibrary.export_render_target(world, target, str(directory), filename)
                assert path.is_file() and path.stat().st_size > 1000
                record = state['current']; record.update(image=filename, sha256=digest(path))
                state['records'].append(record); state['index'] += 1
                unreal.log('CITYSIM_UNREAL_PREVIEW_FRAME '+str(state['index'])+'/'+str(len(batch['renders']))+' '+entry['key'])
                if state['index'] == len(batch['renders']):
                    for group, view in sorted({(e['group'], e['view']) for e in batch['renders']}):
                        rows = [e for e in state['records'] if e['group'] == group and e['view'] == view]
                        manifest = dict(schema_version=1, source=batch['source'], source_sha256=batch['source_sha256'],
                            source_unchanged=digest(ROOT/batch['source']) == batch['source_sha256'], scene='isolated asset studio', frame=1,
                            engine='Unreal D3D11 forward', renderer_label='Unreal', render_note='fixed daylight; manual exposure',
                            resolution=size, views=rows, limitations=batch['limitations'])
                        (out/'Unreal'/group/view/'PreviewManifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
                    (out/'Unreal/Report.json').write_text(json.dumps(dict(task=batch['task'], views=state['records'],
                        source_sha256=batch['source_sha256'], renderer='Windows D3D11 forward', production_changed=False,
                        physical_device_verified=False, lighting=batch['lighting'], preview_scene=dict(floor_extent_m=800,
                        reflection_cubemap=reflection_cube.get_path_name().split('.')[0], reflection_brightness=1, reflection_lightmap_mixing=0,
                        near_plane_recede='Same view axis; target/direction/orthographic width unchanged')), indent=2)+'\n')
                    unreal.log('CITYSIM_UNREAL_PREVIEWS_PASS '+str(len(state['records']))+' captures')
                    unreal.unregister_slate_post_tick_callback(state['handle']); unreal.SystemLibrary.quit_editor()
                else: state['phase'] = 0; state['deadline'] = now+.05
        except Exception:
            (out/'CaptureError.txt').write_text(traceback.format_exc())
            unreal.log_error(traceback.format_exc()); unreal.unregister_slate_post_tick_callback(state['handle']); unreal.SystemLibrary.quit_editor()

    state['handle'] = unreal.register_slate_post_tick_callback(tick)


try:
    start()
except Exception:
    match = re.search(r'-CityPreviewBatch=(?:"([^"]+)"|(\S+))', unreal.SystemLibrary.get_command_line())
    if match:
        batch = json.loads(Path(match.group(1) or match.group(2)).read_text())
        out = (ROOT/batch['output']).resolve()
        if out.is_relative_to(ROOT/'Artifacts'):
            out.mkdir(parents=True, exist_ok=True)
            (out/'CaptureError.txt').write_text(traceback.format_exc())
    unreal.log_error(traceback.format_exc())
    unreal.SystemLibrary.quit_editor()
