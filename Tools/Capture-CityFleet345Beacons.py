"""Capture actual emergency response components in fresh desktop PIE.

Run UnrealEditor with -NoHMD -ExecCmds="py <absolute script path>".
Temporarily isolates and enlarges the actual ambulance response pool for material QA; never saves
or edits the map. Captures both peaks, both crossovers and the non-emitting parked material.
Desktop screenshots do not establish Quest rendering or comfort.
"""
from pathlib import Path
import json
import time
import traceback
import unreal

root = Path(__file__).resolve().parents[1]
folder = root / 'Artifacts/VehicleIntegration345/Beacons-R4'
folder.mkdir(parents=True, exist_ok=True)
outputs = [folder / f'Fleet345-Beacon-{name}.png' for name in ['Red', 'Other', 'CrossA', 'CrossB', 'Parked']]
report = folder / 'Fleet345-Beacon-Preview.json'
error = folder / 'Fleet345-Beacon-Preview.error.txt'
for p in outputs + [report, error]: p.unlink(missing_ok=True)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
saved=root/'CitySimVR/Saved/SaveGames'
prior={p:p.read_bytes() for p in saved.rglob('*') if p.is_file()}
start = time.monotonic()
phase = 0
deadline = start + 5
samples = []
world = None
keep = []

def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    level.editor_request_end_play()
    for p in saved.rglob('*'):
        if p.is_file() and p not in prior:p.unlink()
    for p,data in prior.items():
        p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
    assert {p:p.read_bytes() for p in saved.rglob('*') if p.is_file()}==prior
    unreal.SystemLibrary.quit_editor()

def tick(delta):
    global phase, deadline, world, keep
    try:
        now = time.monotonic()
        if now - start > 300: raise RuntimeError('Emergency PIE capture timed out')
        if now < deadline: return
        if phase == 0:
            level.editor_request_begin_play()
            phase = 1
            deadline = now + 8
        elif phase == 1:
            world = editor.get_game_world()
            if not level.is_in_play_in_editor() or not world: return
            board_class = unreal.load_class(None, '/Script/CitySimVR.CityBoardActor')
            boards = unreal.GameplayStatics.get_all_actors_of_class(world, board_class)
            if not boards: raise RuntimeError('PIE has no CityBoardActor')
            board = boards[0]
            ui = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor) if a.get_class().get_name()=="CityVRToolController")
            ui.return_to_city()
            ui.set_actor_hidden_in_game(True)
            board.set_actor_hidden_in_game(False)
            board.set_actor_tick_enabled(False)
            pools = {c.get_name(): c for c in board.get_components_by_class(unreal.InstancedStaticMeshComponent)}
            keep = [pools[n] for n in ['PoliceResponseInstances','FireResponseInstances','AmbulanceResponseInstances']]
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
                for component in actor.get_components_by_class(unreal.PrimitiveComponent):
                    if component not in keep: component.set_visibility(False, False)
            for pool, x in zip(keep, [-160,0,160]):
                pool.clear_instances()
                pool.set_visibility(True, False)
                pool.set_hidden_in_game(False, False)
                pool.set_world_transform(unreal.Transform(location=unreal.Vector(x, 0, 420), scale=unreal.Vector(.2,.2,.2)), False, True)
                pool.add_instance(unreal.Transform(), False)
                mesh = pool.get_editor_property('static_mesh')
                roof = mesh.get_material_index('Roof')
                material = pool.get_material(roof)
                expected = 'MI_CityFireResponseDetail' if pool.get_name()=='FireResponseInstances' else 'MI_CityPoliceResponseDetail'
                if material.get_name() != expected: raise RuntimeError(f'Wrong moving roof material: {material.get_name()}')
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            location = unreal.Vector(0, -650, 800)
            rotation = unreal.MathLibrary.find_look_at_rotation(location, unreal.Vector(0, 0, 448))
            pawn.set_actor_location(location - unreal.Vector(0, 0, pawn.get_editor_property('base_eye_height')), False, True)
            controller.set_control_rotation(rotation)
            for command in ['FOV 60', 'DisableAllScreenMessages', 'showhud', 't.IdleWhenNotForeground 0', 'r.EyeAdaptationQuality 0']:
                unreal.SystemLibrary.execute_console_command(world, command)
            phase = 2
            deadline = now + 5
        elif phase in [2, 3, 4, 5]:
            game_time = unreal.GameplayStatics.get_time_seconds(world)
            target = [.5, 1.5, 0, 1][phase - 2]
            difference = abs((game_time % 2 - target + 1) % 2 - 1)
            if difference > .045: return
            output = outputs[phase - 2]
            samples.append({'file':output.name, 'game_seconds':game_time, 'cycle_seconds':game_time % 2, 'mode':'response'})
            unreal.SystemLibrary.execute_console_command(world, f'HighResShot 1280x960 filename="{output.as_posix()}"')
            phase += 1
            deadline = now + .25
        elif phase == 6:
            # Compare the same lenses under identical lighting using their original
            # mesh material, exactly as on decorative station vehicles.
            for pool in keep:
                mesh = pool.get_editor_property('static_mesh')
                roof = mesh.get_material_index('Roof')
                pool.set_material(roof, mesh.get_material(roof))
            phase = 7
            deadline = now + 1
        elif phase == 7:
            game_time = unreal.GameplayStatics.get_time_seconds(world)
            output = outputs[4]
            samples.append({'file':output.name, 'game_seconds':game_time, 'mode':'parked mesh roof material'})
            unreal.SystemLibrary.execute_console_command(world, f'HighResShot 1280x960 filename="{output.as_posix()}"')
            phase = 8
            deadline = now + .25
        elif all(p.exists() and p.stat().st_size > 0 for p in outputs):
            report.write_text(json.dumps({'source':'actual response pools in desktop PIE', 'samples':samples, 'saved_map':False,'localSaveCatalogPreserved':True}, indent=2), encoding='utf-8')
            finish()
    except Exception:
        error.write_text(traceback.format_exc(), encoding='utf-8')
        finish()

handle = unreal.register_slate_post_tick_callback(tick)
