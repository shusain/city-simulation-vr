"""D63 native Inspect/overlay review using the pressure regression fixture.
Restores all local save bytes after PIE settles; never saves the sandbox map.
"""
from pathlib import Path
import json
import time
import traceback
import unreal

root = Path(__file__).resolve().parents[1]
out = root/'Artifacts/Preview'
saved = root/'CitySimVR/Saved/SaveGames'
prior = {p: p.read_bytes() for p in saved.rglob('*') if p.is_file()}
source = out/'Water-Pressure.json'
doc = json.loads(source.read_text(encoding='utf-8-sig'))
views = [('Source', next(b for b in doc['buildings'] if b['definitionId'] == 'Service'))]
for name, x, y in [('Full', 4, 11), ('Partial', 30, 11), ('LowPressure', 46, 11), ('Disconnected', 34, 31)]:
    candidates = [b for b in doc['buildings'] if b['definitionId'] == 'Home' and b['x'] == x]
    if name == 'Disconnected':
        candidates = [b for b in candidates if b['y'] == y]
    views.append((name, candidates[0]))
views.append(('Overlay', views[0][1]))
report = out/'D63-Water-PIE.json'
error = report.with_suffix('.error.txt')
for p in [report, error]+[out/f'D63-Water-{n}-PIE.png' for n, _ in views]:
    p.unlink(missing_ok=True)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
start = time.monotonic()
phase, index, deadline = 0, 0, start+5
samples = []


def finish():
    global phase, deadline
    level.editor_request_end_play()
    phase, deadline = 99, time.monotonic()+3


def tick(delta):
    global phase, deadline, index, ui, board, sim
    try:
        now = time.monotonic()
        if now < deadline:
            return
        if phase == 99:
            if level.is_in_play_in_editor():
                return
            for p in saved.rglob('*'):
                if p.is_file() and p not in prior:
                    p.unlink()
            for p, data in prior.items():
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_bytes(data)
            assert {p: p.read_bytes() for p in saved.rglob('*') if p.is_file()} == prior
            if not error.exists():
                report.write_text(json.dumps({'source': 'Native pressure fixture Inspect/overlay', 'samples': samples, 'localSaveBytesRestored': True, 'noMapSaved': True}, indent=2), encoding='utf-8')
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
            return
        if now-start > 160:
            raise RuntimeError('Water preview timed out')
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
            ui.return_to_city()
            load_error = sim.load_city_from_path(str(source), False)
            if load_error:
                raise RuntimeError(str(load_error))
            sim.set_simulation_paused(True)
            board.set_lighting_preset(unreal.CityLightingPreset.DAY)
            board.set_tool_mode(unreal.CityToolMode.INSPECT)
            for command in ['FOV 90', 'DisableAllScreenMessages', 't.IdleWhenNotForeground 0', 'r.EyeAdaptationQuality 0']:
                unreal.SystemLibrary.execute_console_command(world, command)
            phase, deadline = 2, now+3
        elif phase == 2:
            name, entity = views[index]
            board.set_diagnostic_overlay(unreal.CityDiagnosticOverlay.SERVICE if name == 'Overlay' else unreal.CityDiagnosticOverlay.NONE)
            ui.inspect_building(int(entity['entityId']))
            phase, deadline = 3, now+3
        elif phase == 3:
            name, _ = views[index]
            texts = {c.get_name(): str(c.get_editor_property('text')) for c in ui.get_components_by_class(unreal.TextRenderComponent) if c.is_visible() and not c.get_editor_property('hidden_in_game')}
            inspector = texts.get('InspectorText', '')
            expected = {'Source': 'Groundwater yield', 'Full': 'Water supplied', 'Partial': 'Low water pressure', 'LowPressure': 'Low water pressure', 'Disconnected': 'No connected source'}
            if name in expected and expected[name] not in inspector:
                raise RuntimeError(name+' missing diagnostic: '+inspector)
            samples.append({'view': name, 'texts': texts})
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(), f'HighResShot 1800x1400 filename="{(out/f"D63-Water-{name}-PIE.png").as_posix()}"')
            phase, deadline = 4, now+3
        elif phase == 4:
            name, _ = views[index]
            if not (out/f'D63-Water-{name}-PIE.png').exists():
                raise RuntimeError('Missing water screenshot '+name)
            index += 1
            if index < len(views):
                phase, deadline = 2, now+1
            else:
                finish()
    except Exception:
        error.write_text(traceback.format_exc(), encoding='utf-8')
        finish()


handle = unreal.register_slate_post_tick_callback(tick)
