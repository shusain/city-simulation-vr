"""D67 native close shadow/contact comparisons and automatic sun-path views.

Run UnrealEditor-Cmd on CitySandbox with -nohmd -d3d11 -RenderOffscreen,
-ExecCmds="py <this path>" and an isolated -UserDir. Uses the reviewed
D66 flat/sloped fixtures; never saves the map or writes player city files.
"""
from pathlib import Path
import json
import time
import traceback
import unreal

root = Path(__file__).resolve().parents[1]
motion = '-CityVRShadowMotion' in unreal.SystemLibrary.get_command_line()
contact = motion and '-CityVRShadowContactOnly' in unreal.SystemLibrary.get_command_line()
out = root / ('Artifacts/D67-VRShadowContact-Preview' if contact else 'Artifacts/D67-VRShadowMotion-Preview' if motion else 'Artifacts/D67-SunPath-Preview')
out.mkdir(parents=True, exist_ok=True)
(out / 'Error.txt').unlink(missing_ok=True)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
ground = json.loads((root / 'Artifacts/Preview/D66-ResidentialGround-PIE.json').read_text())
views = [(terrain + '-' + lighting + '-' + profile, terrain, lighting, profile)
         for terrain in ('Flat', 'Sloped') for lighting in ('Day', 'Dusk')
         for profile in ('Previous', 'Refined')]
views += [('Auto-' + lighting, 'Flat', lighting, 'Refined') for lighting in ('Sunset', 'Sunrise', 'Night')]
if motion:
    views = [(profile + '-' + view, 'Flat', view, profile) for profile in ('PreviousVR', 'RefinedVR')
             for view in ('Day', 'Dusk', 'Light0', 'Light1', 'Light2', 'View1', 'View2', 'MaxBoard')]
    if contact:
        views = [view for view in views if view[2] in ('Day', 'Dusk')]
elif '-CityLightingContactOnly' in unreal.SystemLibrary.get_command_line():
    views = views[:4]
elif '-CityLightingLowSunOnly' in unreal.SystemLibrary.get_command_line():
    views = [views[7], views[9]]
start = time.monotonic()
deadline = start + 4
phase = 0
index = 0
report = []
map_path = root / 'CitySimVR/Content/CitySim/Maps/CitySandbox.umap'
map_bytes = map_path.read_bytes()


def finish(error=None):
    global phase, deadline
    if error:
        (out / 'Error.txt').write_text(error)
    (out / 'Report.json').write_text(json.dumps(dict(samples=report, error=error), indent=2))
    level.editor_request_end_play()
    phase, deadline = 90, time.monotonic() + 3


def tick(delta):
    global phase, deadline, index, ui, board, sim, environment, sun, sample, initial_board
    try:
        now = time.monotonic()
        if phase != 90 and now - start > 200:
            raise RuntimeError('Native lighting preview timed out')
        if now < deadline:
            return
        if phase == 90:
            payload = json.loads((out / 'Report.json').read_text())
            payload['mapBytesUnchanged'] = map_path.read_bytes() == map_bytes
            if not payload['mapBytesUnchanged']:
                payload['error'] = 'Map bytes changed'
                (out / 'Error.txt').write_text(payload['error'])
            (out / 'Report.json').write_text(json.dumps(payload, indent=2))
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
        elif phase == 0:
            level.editor_request_begin_play()
            phase, deadline = 1, now + 6
        elif phase == 1:
            world = editor.get_game_world()
            if not world:
                return
            actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            ui = next(a for a in actors if a.get_class().get_name() == 'CityVRToolController')
            board = next(a for a in actors if a.get_class().get_name() == 'CityBoardActor')
            environment = next(a for a in actors if a.get_class().get_name() == 'CitySandboxEnvironment')
            sun = environment.get_component_by_class(unreal.DirectionalLightComponent)
            initial_board = board.get_actor_transform()
            sim = next(o for o in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if o.get_outer() == world)
            ui.return_to_city()
            ui.set_debug_mode_enabled(False)
            ui.set_city_summary_visible(False)
            ui.set_actor_hidden_in_game(True)
            for command in ('FOV 100' if motion else 'FOV 45', 'DisableAllScreenMessages', 't.IdleWhenNotForeground 0'):
                unreal.SystemLibrary.execute_console_command(world, command)
            phase, deadline = 2, now + 1
        elif phase == 2:
            label, terrain, lighting, profile = views[index]
            path = root / ('Artifacts/Preview/D66-ResidentialGround-' + terrain + 'Autumn-City.json')
            automatic = lighting in ('Sunset', 'Sunrise', 'Night')
            error = sim.load_city_from_path(str(path), False)
            if error:
                raise RuntimeError(str(error))
            board.set_tool_mode(unreal.CityToolMode.INSPECT)
            if motion:
                board.set_actor_transform(initial_board, False, True)
                if lighting == 'MaxBoard':
                    board.set_actor_scale3d(board.get_actor_scale3d() * 2)
            board.set_diagnostic_overlay(unreal.CityDiagnosticOverlay.NONE)
            sim.set_simulation_paused(not automatic)
            if automatic:
                sim.set_simulation_speed(5)
            board.set_lighting_preset(unreal.CityLightingPreset.DAY)
            board.set_lighting_preset(unreal.CityLightingPreset.AUTOMATIC if automatic else
                (unreal.CityLightingPreset.DUSK if lighting == 'Dusk' or lighting.startswith('Light') else unreal.CityLightingPreset.DAY))
            sun.set_shadow_bias(.5 if profile == 'Previous' else .35)
            sun.set_shadow_slope_bias(.5)
            sun.set_editor_property('shadow_sharpen', 0 if profile == 'Previous' else .5)
            resolution = 2048 if profile == 'Previous' else 4096
            if motion:
                sun.set_dynamic_shadow_distance_movable_light(600 if profile == 'RefinedVR' else 2000)
                sun.set_cascade_transition_fraction(.25 if profile == 'RefinedVR' else .1)
                if lighting.startswith('Light'):
                    step = int(lighting[-1])
                    rotation = sun.get_world_rotation()
                    sun.set_world_rotation(unreal.Rotator(pitch=rotation.pitch + step * .08,
                        yaw=rotation.yaw + step * .18, roll=0), False, True)
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(), 'r.Shadow.MaxCSMResolution ' + str(resolution))
            data = next(s['selectedHome'] for s in ground['samples'] if s['view'] ==
                ('FlatAutumnOverview' if terrain == 'Flat' else 'SlopedHome1Overview'))
            owner = data['owner']
            center = unreal.MathLibrary.transform_location(board.get_actor_transform(),
                unreal.Vector((owner['x'] + 1) * 800, (owner['y'] + 1) * 800, data['padTopCm'] + 250))
            location = center + (unreal.Vector(25, -80, 65) if motion else unreal.Vector(18, -28, 25))
            if motion and lighting in ('View1', 'View2'):
                location += unreal.Vector(20 if lighting == 'View1' else 40, 0, 0)
            pawn = unreal.GameplayStatics.get_player_pawn(editor.get_game_world(), 0)
            controller = unreal.GameplayStatics.get_player_controller(editor.get_game_world(), 0)
            pawn.set_actor_location(location - unreal.Vector(0, 0, pawn.get_editor_property('base_eye_height')), False, True)
            controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(location, center))
            pools = board.get_components_by_class(unreal.InstancedStaticMeshComponent)
            foundations = [p for p in pools if p.get_name() in ('FoundationInstances', 'ResidentialFoundationInstances')]
            assert all(p.get_editor_property('cast_shadow') for p in foundations), 'Foundation shadows disabled'
            sample = dict(view=label, fixture=str(path), automatic=automatic, shadowBias=sun.get_editor_property('shadow_bias'),
                shadowSlopeBias=sun.get_editor_property('shadow_slope_bias'), shadowSharpen=sun.get_editor_property('shadow_sharpen'), csmResolution=resolution,
                actualCSMResolution=unreal.SystemLibrary.get_console_variable_int_value('r.Shadow.MaxCSMResolution'),
                filterMethod=unreal.SystemLibrary.get_console_variable_int_value('r.Shadow.FilterMethod'),
                foundationsCastShadow=True, camera=[location.x, location.y, location.z])
            if motion:
                sample.update(fieldOfView=100, sunDistanceCm=sun.get_editor_property('dynamic_shadow_distance_movable_light'),
                    cascadeTransition=sun.get_editor_property('cascade_transition_fraction'),
                    controlledSmallLightChanges=lighting.startswith('Light'), boardScale=board.get_actor_scale3d().x)
                sample['actualFieldOfView'] = unreal.GameplayStatics.get_player_camera_manager(editor.get_game_world(), 0).get_fov_angle()
            phase, deadline = 3, now + (dict(Sunset=5, Sunrise=19, Night=12)[lighting] if automatic else 2)
        elif phase == 3:
            sim.set_simulation_paused(True)
            rotation = sun.get_world_rotation()
            sample.update(lightRotation=[rotation.pitch, rotation.yaw, rotation.roll], intensity=sun.get_editor_property('intensity'))
            path = out / (views[index][0] + '.png')
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(), f'HighResShot 1600x1200 filename="{path.as_posix()}"')
            phase, deadline = 4, now + 2
        elif phase == 4:
            assert (out / (views[index][0] + '.png')).is_file(), 'Native screenshot missing'
            report.append(sample)
            index += 1
            if index == len(views):
                finish()
            else:
                phase, deadline = 2, now + .5
    except Exception:
        finish(traceback.format_exc())


handle = unreal.register_slate_post_tick_callback(tick)
