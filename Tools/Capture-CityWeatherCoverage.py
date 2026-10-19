"""Native D65 terrain material sweep and actual manager lifecycle checks.

Launch CitySandbox with -ExecCmds="py .../Capture-CityWeatherCoverage.py".
Images drive only the existing terrain MID uniforms, with the board tick frozen
to hold reproducible early/partial/full/melting values. Actual pause/menu/load
checks below restore normal board ticking and use the native weather manager.
This verifies desktop rendering/lifecycle, not Quest stereo or GPU performance.
Player saves are restored byte-for-byte; no map or content assets are saved.
"""
from pathlib import Path
import copy
import hashlib
import json
import time
import traceback
import unreal

root = Path(__file__).resolve().parents[1]
out = root/'Artifacts/Preview'
out.mkdir(parents=True, exist_ok=True)
saved = root/'CitySimVR/Saved/SaveGames'
prior = {p: p.read_bytes() for p in saved.rglob('*') if p.is_file()}
backup = out/f'D65-LocalSaveBackup-{time.time_ns()}'
backup.mkdir()
for path, data in prior.items():
    destination = backup/path.relative_to(saved)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data)
(backup/'manifest.json').write_text(json.dumps([
    {'path': str(path.relative_to(saved)), 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
    for path, data in prior.items()], indent=2), encoding='utf-8')
report_path = out/'D65-Coverage-PIE.json'
error_path = out/'D65-Coverage-PIE.error.txt'
report_path.unlink(missing_ok=True)
error_path.unlink(missing_ok=True)
source = out/'D62-Mountains.json'
if not source.exists():
    source = out/'D62-Snow.json'
base = json.loads(source.read_text(encoding='utf-8'))
fixtures = {}
for name, season, temperature, precipitation in [('Clear', 1, 1, False), ('Rain', 1, 1, True), ('Snow', 3, 0, True)]:
    doc = copy.deepcopy(base)
    doc['cityName'] = 'D65 '+name+' isolated preview'
    doc['schemaVersion'] = 58
    doc['terrain']['waterRulesVersion'] = 0
    doc['terrain']['sceneryVersion'] = 1
    doc['economy']['paused'] = True
    doc['environment'].update(season=season, temperature=temperature, precipitation=precipitation,
                              windStrength=2, nextWeatherChangeStep='999999')
    fixture = out/f'D65-{name}.json'
    fixture.write_text(json.dumps(doc, indent=2), encoding='utf-8')
    fixtures[name] = fixture

states = [('Clear', 0.0, 0.0), ('Wet', 1.0, 0.0), ('SnowEarly', 0.0, .25),
          ('SnowPartial', 0.0, .45), ('SnowFull', 0.0, 1.0), ('Melt', .2, .25)]
views = [(lighting, name, wet, snow) for lighting in ('Day', 'Night') for name, wet, snow in states]
for lighting, name, _, _ in views:
    (out/f'D65-{name}-{lighting}-PIE.png').unlink(missing_ok=True)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
start = time.monotonic()
deadline = start+5
phase = 0
index = 0
samples = []
lifecycle = {}
finishing = False
final_report = None


def finish():
    global finishing, phase, deadline
    if not finishing:
        finishing = True
        level.editor_request_end_play()
        phase, deadline = 99, time.monotonic()+3


def restore_saves():
    global phase, deadline
    for p in saved.rglob('*'):
        if p.is_file() and p not in prior:
            p.unlink()
    for p, data in prior.items():
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)
    assert {p: p.read_bytes() for p in saved.rglob('*') if p.is_file()} == prior
    phase, deadline = 100, time.monotonic()+3


def verify_and_quit():
    assert not level.is_in_play_in_editor(), 'PIE unexpectedly restarted after save restoration'
    assert {p: p.read_bytes() for p in saved.rglob('*') if p.is_file()} == prior, 'Local saves changed after delayed restoration check'
    if final_report is not None and not error_path.exists():
        final_report['local_saves_byte_identical'] = True
        final_report['local_save_count'] = len(prior)
        final_report['crash_recovery_backup'] = str(backup)
        final_report['pie_stopped_before_restore'] = True
        final_report['delayed_save_reverification'] = True
        report_path.write_text(json.dumps(final_report, indent=2), encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def load_fixture(name):
    error = sim.load_city_from_path(str(fixtures[name]), False)
    if error:
        raise RuntimeError(str(error))


def particle_state():
    return {p.get_name(): {'live': p.get_num_active_particles(), 'active': p.is_active(),
                           'visible': p.is_visible(), 'tick': p.is_component_tick_enabled()}
            for p in board.get_components_by_class(unreal.CityWeatherParticleComponent)}


def values():
    return {'wet': material.get_scalar_parameter_value('WeatherWet'),
            'snow': material.get_scalar_parameter_value('WeatherSnow')}


def tick(delta):
    global phase, deadline, index, ui, board, world, sim, material, frozen, final_report
    try:
        now = time.monotonic()
        if now < deadline:
            return
        if phase == 99:
            if level.is_in_play_in_editor():
                level.editor_request_end_play()
                deadline = now+.5
                return
            restore_saves()
            return
        if phase == 100:
            verify_and_quit()
            return
        if now-start > 180:
            raise RuntimeError('D65 native terrain capture timed out')
        if phase == 0:
            level.editor_request_begin_play()
            phase, deadline = 1, now+8
        elif phase == 1:
            world = editor.get_game_world()
            if not world:
                return
            actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            ui = next(a for a in actors if a.get_class().get_name() == 'CityVRToolController')
            board = next(a for a in actors if a.get_class().get_name() == 'CityBoardActor')
            sim = next(o for o in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if o.get_outer() == world)
            ui.return_to_city()
            ui.set_debug_mode_enabled(False)
            ui.set_city_summary_visible(False)
            load_fixture('Clear')
            sim.set_simulation_paused(True)
            for command in ('FOV 60', 'DisableAllScreenMessages', 't.IdleWhenNotForeground 0', 'r.EyeAdaptationQuality 0'):
                unreal.SystemLibrary.execute_console_command(world, command)
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            center = unreal.MathLibrary.transform_location(board.get_actor_transform(), unreal.Vector(25600, 25600, 0))
            location = center+unreal.Vector(0, -225, 260)
            pawn.set_actor_location(location-unreal.Vector(0, 0, pawn.get_editor_property('base_eye_height')), False, True)
            controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(location, center))
            board.set_tool_mode(unreal.CityToolMode.INSPECT)
            phase, deadline = 2, now+3
        elif phase == 2:
            surface = next(p for p in board.get_components_by_class(unreal.ProceduralMeshComponent) if p.get_name() == 'BoardSurface')
            material = surface.get_material(0)
            if not isinstance(material, unreal.MaterialInstanceDynamic):
                raise RuntimeError('Terrain surface did not bind a native dynamic material')
            board.set_actor_tick_enabled(False)
            phase, deadline = 3, now+.2
        elif phase == 3:
            lighting, name, wet, snow = views[index]
            board.set_lighting_preset(unreal.CityLightingPreset.DAY if lighting == 'Day' else unreal.CityLightingPreset.NIGHT)
            material.set_scalar_parameter_value('WeatherWet', wet)
            material.set_scalar_parameter_value('WeatherSnow', snow)
            phase, deadline = 4, now+1
        elif phase == 4:
            lighting, name, wet, snow = views[index]
            path = out/f'D65-{name}-{lighting}-PIE.png'
            actual = values()
            assert abs(actual['wet']-wet) < 1e-5 and abs(actual['snow']-snow) < 1e-5, actual
            unreal.SystemLibrary.execute_console_command(world, f'HighResShot 1600x1200 filename="{path.as_posix()}"')
            samples.append({'lighting': lighting, 'view': name, 'uniforms': actual,
                            'stimulus': 'native terrain MID material sweep, board tick held', 'image': str(path)})
            phase, deadline = 5, now+2
        elif phase == 5:
            lighting, name, _, _ = views[index]
            assert (out/f'D65-{name}-{lighting}-PIE.png').exists(), 'Missing native screenshot'
            index += 1
            if index < len(views):
                phase, deadline = 3, now+.1
            else:
                board.set_actor_tick_enabled(True)
                load_fixture('Rain')
                sim.set_simulation_paused(False)
                phase, deadline = 10, now+3
        elif phase == 10:
            state = particle_state()
            assert sum(p['live'] for p in state.values()) > 0, state
            lifecycle['running_rain'] = state
            sim.set_simulation_paused(True)
            phase, deadline = 11, now+1
        elif phase == 11:
            frozen = {'particles': particle_state(), 'uniforms': values()}
            assert all(not p['tick'] for p in frozen['particles'].values()), frozen
            phase, deadline = 12, now+2
        elif phase == 12:
            current = {'particles': particle_state(), 'uniforms': values()}
            assert current == frozen, (frozen, current)
            lifecycle['paused_particles_and_uniforms_frozen'] = current
            ui.show_main_menu()
            phase, deadline = 13, now+2
        elif phase == 13:
            state = particle_state()
            assert all(not p['visible'] and not p['tick'] for p in state.values()), state
            lifecycle['menu_hidden_and_stopped'] = state
            ui.return_to_city()
            sim.set_simulation_paused(False)
            phase, deadline = 14, now+2
        elif phase == 14:
            state = particle_state()
            assert any(p['visible'] and p['tick'] and p['live'] > 0 for p in state.values()), state
            lifecycle['return_resumes_rain'] = state
            load_fixture('Snow')
            sim.set_simulation_paused(True)
            phase, deadline = 15, now+2
        elif phase == 15:
            state = particle_state()
            assert all(p['live'] == 0 for p in state.values()), state
            snow = values()
            assert snow['snow'] > .99 and snow['wet'] < .01, snow
            lifecycle['load_snow_resets_rain_and_uniforms'] = {'particles': state, 'uniforms': snow}
            load_fixture('Clear')
            sim.set_simulation_paused(True)
            phase, deadline = 16, now+2
        elif phase == 16:
            state, clear = particle_state(), values()
            assert all(p['live'] == 0 and not p['tick'] for p in state.values()), state
            assert clear['snow'] == 0 and clear['wet'] == 0, clear
            lifecycle['load_clear_removes_storm_and_coverage'] = {'particles': state, 'uniforms': clear}
            final_report = {'views': samples, 'native_lifecycle': lifecycle,
                            'fixture_source': str(source), 'native_geometry_and_material': True,
                            'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest()}
            finish()
    except Exception:
        error_path.write_text(traceback.format_exc(), encoding='utf-8')
        unreal.log_error(traceback.format_exc())
        if finishing:
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
        else:
            finish()


handle = unreal.register_slate_post_tick_callback(tick)
