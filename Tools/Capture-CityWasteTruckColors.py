"""Verify live fleet routing/materials, then isolate actual truck pools for PIE QA.

Uses the read-only D61 investigation save. Restores every local save byte, never
saves the map, and captures day/night views. Desktop QA does not certify Quest.
"""
from pathlib import Path
import json
import time
import traceback
import unreal

root = Path(__file__).resolve().parents[1]
out = root/'Artifacts/Preview'
out.mkdir(parents=True, exist_ok=True)
saved = root/'CitySimVR/Saved/SaveGames'
prior = {p: p.read_bytes() for p in saved.rglob('*') if p.is_file()}
fixture = root/'Artifacts/D61-TruckColor-Investigation/Cities/37dba01673904bdfb4ffca75a6e1a0cd.json'
outputs = [out/f'D61-TruckColors-{name}.png' for name in ('Day', 'Night')]
report = out/'D61-TruckColors.json'
error = report.with_suffix('.error.txt')
for p in outputs+[report, error]:
    p.unlink(missing_ok=True)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
start = time.monotonic()
deadline = start+5
phase = 0
samples = []


def finish():
    level.editor_request_end_play()
    for p in saved.rglob('*'):
        if p.is_file() and p not in prior:
            p.unlink()
    for p, data in prior.items():
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)
    assert {p: p.read_bytes() for p in saved.rglob('*') if p.is_file()} == prior
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(delta):
    global phase, deadline, board, world, keep
    try:
        now = time.monotonic()
        if now-start > 120:
            raise RuntimeError('Waste truck color preview timed out')
        if now < deadline:
            return
        if phase == 0:
            level.editor_request_begin_play()
            phase, deadline = 1, now+8
        elif phase == 1:
            world = editor.get_game_world()
            if not world or not level.is_in_play_in_editor():
                return
            actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            ui = next(a for a in actors if a.get_class().get_name() == 'CityVRToolController')
            board = next(a for a in actors if a.get_class().get_name() == 'CityBoardActor')
            sim = next(o for o in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if o.get_outer() == world)
            saved.mkdir(parents=True, exist_ok=True)
            (saved/'CitySaveV1.json').write_bytes(fixture.read_bytes())
            ui.return_to_city()
            if not sim.load_city():
                raise RuntimeError('Native fleet fixture failed to load')
            sim.set_simulation_paused(True)
            ui.set_actor_tick_enabled(False)
            board.set_lighting_preset(unreal.CityLightingPreset.DAY)
            phase, deadline = 2, now+3
        elif phase == 2:
            pools = {c.get_name(): c for c in board.get_components_by_class(unreal.InstancedStaticMeshComponent)}
            keep = [pools['GarbageTruckInstances'], pools['RecyclingTruckInstances']]
            doc = json.loads(fixture.read_text(encoding='utf-8'))
            # The actual saved recycling fleet has two pickup legs and a return
            # leg whose origin is a factory, proving classification by ownership.
            kinds = {b['entityId']: b['definitionId'] for b in doc['buildings']}
            for pool, kind, material in zip(keep, ['WasteDepot', 'Recycling'], ['MI_CityIndustrial', 'MI_CityService']):
                expected = sum(kinds[t['depot']] == kind for t in doc['waste']['trucks'])
                actual = pool.get_instance_count()
                assert actual == expected == 3, (kind, expected, actual)
                mesh = pool.get_editor_property('static_mesh')
                body = mesh.get_material_index('Service')
                assert body >= 0 and pool.get_material(body).get_name() == material
                roof = mesh.get_material_index('Roof')
                assert pool.get_material(roof) == mesh.get_material(roof)
                samples.append({'pool': pool.get_name(), 'liveFleetCount': actual, 'bodyMaterial': material, 'trimPreserved': True})
            board.set_actor_tick_enabled(False)
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
                for component in actor.get_components_by_class(unreal.PrimitiveComponent):
                    if component not in keep:
                        component.set_visibility(False, False)
            for pool, x in zip(keep, [-85, 85]):
                pool.clear_instances()
                pool.set_visibility(True, False)
                pool.set_world_transform(unreal.Transform(location=unreal.Vector(x, 0, 420), scale=unreal.Vector(.2, .2, .2)), False, True)
                pool.add_instance(unreal.Transform(), False)
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            location = unreal.Vector(320, -510, 760)
            pawn.set_actor_location(location-unreal.Vector(0, 0, pawn.get_editor_property('base_eye_height')), False, True)
            controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(location, unreal.Vector(0, 0, 445)))
            for command in ['FOV 45', 'DisableAllScreenMessages', 't.IdleWhenNotForeground 0', 'r.EyeAdaptationQuality 0']:
                unreal.SystemLibrary.execute_console_command(world, command)
            phase, deadline = 3, now+4
        elif phase == 3:
            unreal.SystemLibrary.execute_console_command(world, f'HighResShot 1400x1000 filename="{outputs[0].as_posix()}"')
            phase, deadline = 4, now+3
        elif phase == 4:
            board.set_lighting_preset(unreal.CityLightingPreset.NIGHT)
            phase, deadline = 5, now+4
        elif phase == 5:
            unreal.SystemLibrary.execute_console_command(world, f'HighResShot 1400x1000 filename="{outputs[1].as_posix()}"')
            phase, deadline = 6, now+3
        elif all(p.exists() and p.stat().st_size > 0 for p in outputs):
            report.write_text(json.dumps({'source': 'Native live fleet pools, then isolated enlarged day/night PIE views', 'samples': samples, 'savedMap': False, 'localSaveBytesRestored': True}, indent=2), encoding='utf-8')
            finish()
    except Exception:
        error.write_text(traceback.format_exc(), encoding='utf-8')
        finish()


handle = unreal.register_slate_post_tick_callback(tick)
