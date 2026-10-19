"""D86 matched native water views. Isolated UserDir; no map or city writes.

Run on CitySandbox with -ExecCmds="py ABSOLUTE_PATH" and -D86Before
for the original mesh, then without that flag after compiling the refinement.
"""
from pathlib import Path
import hashlib
import json
import time
import traceback
import unreal

root = Path(__file__).resolve().parents[1]
variant = 'Before' if '-D86Before' in unreal.SystemLibrary.get_command_line() else 'After'
out = root / 'Artifacts/D86' / variant
out.mkdir(parents=True, exist_ok=True)
assert not (out / 'Report.json').exists(), 'Preserve existing evidence; choose a fresh output'
fixtures = [root / 'Artifacts/Preview/D66-ResidentialGround-SlopedAutumn-City.json',
            root / 'Artifacts/D78/OriginalCity.json']
protected = [root / 'CitySimVR/Content/CitySim/Maps/CitySandbox.umap'] + fixtures
digests = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
views = [('Legacy-Overview', 0, False), ('Legacy-Close', 0, True),
         ('Organic-Overview', 1, False), ('Organic-Close', 1, True)]
phase, index = 0, 0
start = time.monotonic()
deadline = start + 4
samples = []


def finish(error=None):
    global phase, deadline
    (out / 'Report.json').write_text(json.dumps(dict(variant=variant, samples=samples, error=error), indent=2))
    level.editor_request_end_play()
    phase, deadline = 90, time.monotonic() + 3


def tick(delta):
    global phase, index, deadline, board, sim, ui, sample
    try:
        now = time.monotonic()
        if now < deadline:
            return
        if phase == 90:
            assert not level.is_in_play_in_editor(), 'PIE has not ended'
            assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in digests.items())
            data = json.loads((out / 'Report.json').read_text())
            data['protectedFilesUnchanged'] = digests
            (out / 'Report.json').write_text(json.dumps(data, indent=2))
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
            return
        if now - start > 150:
            raise RuntimeError('Water depth preview timed out')
        if phase == 0:
            level.editor_request_begin_play()
            phase, deadline = 1, now + 6
        elif phase == 1:
            world = editor.get_game_world()
            if not world:
                return
            actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            ui = next(a for a in actors if a.get_class().get_name() == 'CityVRToolController')
            board = next(a for a in actors if a.get_class().get_name() == 'CityBoardActor')
            sim = next(o for o in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if o.get_outer() == world)
            ui.return_to_city()
            ui.set_actor_hidden_in_game(True)
            for command in ('FOV 55', 'DisableAllScreenMessages', 't.IdleWhenNotForeground 0'):
                unreal.SystemLibrary.execute_console_command(world, command)
            phase, deadline = 2, now + 1
        elif phase == 2:
            label, fixture_index, close = views[index]
            error = sim.load_city_from_path(str(fixtures[fixture_index]), False)
            assert not error, str(error)
            sim.set_simulation_paused(True)
            board.set_tool_mode(unreal.CityToolMode.INSPECT)
            board.set_diagnostic_overlay(unreal.CityDiagnosticOverlay.NONE)
            board.set_lighting_preset(unreal.CityLightingPreset.DAY)
            # Same camera in both versions; focus on the lake in the lower board half.
            transform = board.get_actor_transform()
            target = unreal.MathLibrary.transform_location(transform, unreal.Vector(32000, 32000, -800))
            location = target + (unreal.Vector(75, -90, 110) if close else unreal.Vector(170, -240, 250))
            pawn = unreal.GameplayStatics.get_player_pawn(editor.get_game_world(), 0)
            controller = unreal.GameplayStatics.get_player_controller(editor.get_game_world(), 0)
            pawn.set_actor_location(location - unreal.Vector(0, 0, pawn.get_editor_property('base_eye_height')), False, True)
            controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(location, target))
            water = next(c for c in board.get_components_by_class(unreal.ProceduralMeshComponent) if c.get_name() == 'WaterSurface')
            sample = dict(view=label, fixture=str(fixtures[fixture_index]), material=water.get_material(0).get_path_name(),
                          camera=[location.x, location.y, location.z])
            phase, deadline = 3, now + 3
        elif phase == 3:
            path = out / (views[index][0] + '.png')
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(), f'HighResShot 1600x1200 filename="{path.as_posix()}"')
            phase, deadline = 4, now + 2
        elif phase == 4:
            assert (out / (views[index][0] + '.png')).is_file(), 'Native screenshot missing'
            samples.append(sample)
            index += 1
            if index == len(views):
                finish()
            else:
                phase, deadline = 2, now + .5
    except Exception:
        finish(traceback.format_exc())


handle = unreal.register_slate_post_tick_callback(tick)
