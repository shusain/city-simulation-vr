"""D67 native low-angle enclosure/floor previews using isolated PIE and existing city fixtures.

Run on CitySandbox with -ExecCmds="py ABSOLUTE_PATH", -nohmd -d3d11
-RenderOffscreen and an isolated -UserDir. Does not save maps or player cities.
"""
from pathlib import Path
import json
import time
import traceback
import unreal

root = Path(__file__).resolve().parents[1]
out = root / 'Artifacts/D67-TerrainBase-Preview'
out.mkdir(parents=True, exist_ok=True)
(out / 'Error.txt').unlink(missing_ok=True)
views = [('Flat-Dusk-Before', 'Flat', 'Dusk', 'Before'),
         ('Flat-Dusk-Enclosed', 'Flat', 'Dusk', 'After'),
         ('Sloped-Dusk-Enclosed', 'Sloped', 'Dusk', 'After'),
         ('Sloped-Day-Underside', 'Sloped', 'Day', 'Below'),
         ('Sloped-Day-MovedScaled', 'Sloped', 'Day', 'Moved')]
for view in views:
    (out / (view[0] + '.png')).unlink(missing_ok=True)
legacy_floor = unreal.EditorAssetLibrary.load_asset('/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray')
assert legacy_floor
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
map_path = root / 'CitySimVR/Content/CitySim/Maps/CitySandbox.umap'
map_bytes = map_path.read_bytes()
phase, index = 0, 0
start = time.monotonic()
deadline = start + 4
report = []


def finish(error=None):
    global phase, deadline
    if error:
        (out / 'Error.txt').write_text(error)
    (out / 'Report.json').write_text(json.dumps(dict(samples=report, error=error), indent=2))
    level.editor_request_end_play()
    phase, deadline = 90, time.monotonic() + 3


def tick(delta):
    global phase, index, deadline, board, sim, floor, base, initial_transform, material, sample
    try:
        now = time.monotonic()
        if phase != 90 and now - start > 150:
            raise RuntimeError('Terrain base preview timed out')
        if now < deadline:
            return
        if phase == 90:
            data = json.loads((out / 'Report.json').read_text())
            data['mapBytesUnchanged'] = map_path.read_bytes() == map_bytes
            assert data['mapBytesUnchanged'], 'Map bytes changed'
            (out / 'Report.json').write_text(json.dumps(data, indent=2))
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
            floor = next(c for c in environment.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'Floor')
            base = next(c for c in board.get_components_by_class(unreal.ProceduralMeshComponent) if c.get_name() == 'TerrainBase')
            assert floor.get_material(0).get_path_name().startswith('/Game/CitySim/Art/Materials/M_CityRoomFloor')
            material = floor.get_material(0)
            initial_transform = board.get_actor_transform()
            sim = next(o for o in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if o.get_outer() == world)
            ui.return_to_city()
            ui.set_actor_hidden_in_game(True)
            for command in ('FOV 55', 'DisableAllScreenMessages', 't.IdleWhenNotForeground 0'):
                unreal.SystemLibrary.execute_console_command(world, command)
            phase, deadline = 2, now + 1
        elif phase == 2:
            label, terrain, lighting, variant = views[index]
            fixture = root / ('Artifacts/Preview/D66-ResidentialGround-' + terrain + 'Autumn-City.json')
            error = sim.load_city_from_path(str(fixture), False)
            assert not error, str(error)
            sim.set_simulation_paused(True)
            board.set_tool_mode(unreal.CityToolMode.INSPECT)
            board.set_diagnostic_overlay(unreal.CityDiagnosticOverlay.NONE)
            board.set_lighting_preset(unreal.CityLightingPreset.DUSK if lighting == 'Dusk' else unreal.CityLightingPreset.DAY)
            board.set_actor_transform(initial_transform, False, True)
            if variant == 'Moved':
                board.set_actor_rotation(unreal.Rotator(pitch=0, yaw=35, roll=0), True)
                board.set_actor_scale3d(board.get_actor_scale3d() * 1.35)
                board.set_actor_location(board.get_actor_location() + unreal.Vector(30, -20, 25), False, True)
            base.set_visibility(variant != 'Before')
            floor.set_material(0, legacy_floor if variant == 'Before' else material)
            center = unreal.MathLibrary.transform_location(board.get_actor_transform(), unreal.Vector(25600, 25600, 0))
            target = center + unreal.Vector(0, 0, -10 if variant == 'Below' else 0)
            location = center + (unreal.Vector(130, -180, -50) if variant == 'Below' else unreal.Vector(210, -300, 70))
            if variant == 'Moved':
                location = center + unreal.Vector(300, -380, 90)
            pawn = unreal.GameplayStatics.get_player_pawn(editor.get_game_world(), 0)
            controller = unreal.GameplayStatics.get_player_controller(editor.get_game_world(), 0)
            pawn.set_actor_location(location - unreal.Vector(0, 0, pawn.get_editor_property('base_eye_height')), False, True)
            controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(location, target))
            casters = [c for c in board.get_components_by_class(unreal.InstancedStaticMeshComponent)
                       if c.get_name() in ('FoundationInstances', 'ResidentialFoundationInstances')]
            assert all(c.get_editor_property('cast_shadow') for c in casters)
            sample = dict(view=label, fixture=str(fixture), floorMaterial=floor.get_material(0).get_path_name(),
                baseVisible=base.is_visible(), baseCastsShadow=base.get_editor_property('cast_shadow'),
                cityFoundationsCastShadow=True, baseMaterial=base.get_material(0).get_path_name(),
                camera=[location.x, location.y, location.z])
            rotation = board.get_actor_rotation()
            scale = board.get_actor_scale3d()
            sample.update(boardRotation=[rotation.pitch, rotation.yaw, rotation.roll],
                boardScale=[scale.x, scale.y, scale.z])
            phase, deadline = 3, now + 2
        elif phase == 3:
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
