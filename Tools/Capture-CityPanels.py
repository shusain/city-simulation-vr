"""Render the real D59 menu/HUD and trend arrows in temporary PIE, preserving local save/catalog bytes."""
from pathlib import Path
import json
import time
import traceback
import unreal

root = Path(__file__).resolve().parents[1]
out = root / 'Artifacts/Preview'
out.mkdir(parents=True, exist_ok=True)
report = out / 'D59-Panels.json'
error = out / 'D59-Panels.error.txt'
names = ('SettingsDebugOff', 'DebugTools', 'CityDay', 'CityNight', 'DemolishNight')
outputs = [out / f'D59-{name}.png' for name in names]
for path in outputs + [report, error]:
    path.unlink(missing_ok=True)
saved = root / 'CitySimVR/Saved/SaveGames'
prior = {p: p.read_bytes() for p in saved.rglob('*') if p.is_file()}
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
start = time.monotonic()
deadline = start + 5
phase = 0
samples = []
ui = None
board = None


def finish():
    level.editor_request_end_play()
    for path in saved.rglob('*'):
        if path.is_file() and path not in prior:
            path.unlink()
    for path, data in prior.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    assert {p: p.read_bytes() for p in saved.rglob('*') if p.is_file()} == prior
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(delta):
    global phase, deadline, ui, board
    try:
        now = time.monotonic()
        if now - start > 120:
            raise RuntimeError('Panel preview timed out')
        if now < deadline:
            return
        if phase == 0:
            level.editor_request_begin_play()
            phase = 1
            deadline = now + 8
        elif phase == 1:
            world = editor.get_game_world()
            if not world or not level.is_in_play_in_editor():
                return
            actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            ui = next(a for a in actors if a.get_class().get_name() == 'CityVRToolController')
            board = next(a for a in actors if a.get_class().get_name() == 'CityBoardActor')
            ui.return_to_city()
            next(obj for obj in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if obj.get_outer() == world).set_simulation_paused(True)
            for command in ('FOV 90', 'DisableAllScreenMessages', 't.IdleWhenNotForeground 0', 'r.EyeAdaptationQuality 0'):
                unreal.SystemLibrary.execute_console_command(world, command)
            ui.set_debug_mode_enabled(False)
            ui.show_game_settings()
            phase = 2
            deadline = now + 3
        elif phase in (2, 4, 6, 8, 10):
            index = (phase - 2) // 2
            texts = {c.get_name(): str(c.get_editor_property('text')) for c in ui.get_components_by_class(unreal.TextRenderComponent) if c.is_visible() and not c.get_editor_property('hidden_in_game')}
            if index == 0 and 'DEBUG TOOLS' in texts.values():
                raise RuntimeError('Debug tools visible with debug off')
            if index == 1 and not all(label in texts.values() for label in ('TEST ALARM', 'START FIRE', 'START CRIME', 'START ARSON', 'TEST QUAKE', 'TEST TORNADO')):
                raise RuntimeError('Missing grouped debug tools')
            if index >= 2 and ('TREASURY' not in texts.get('CityTotalsText', '') or 'POPULATION' not in texts.get('CityTotalsText', '')):
                raise RuntimeError('Missing top totals')
            components = {c.get_name(): c for c in ui.get_components_by_class(unreal.SceneComponent)}
            if index >= 2:
                totals_z = components['CityTotalsText'].get_editor_property('relative_location').z
                trend_z = components['CityTrendText0'].get_editor_property('relative_location').z
                time_z = components['TimeControlLabel0'].get_editor_property('relative_location').z
                calendar_z = components['CalendarText'].get_editor_property('relative_location').z
                if not calendar_z > totals_z > trend_z > time_z:
                    raise RuntimeError('HUD rows are out of order')
                if 'Month net now' not in texts.get('CityTrendText0', ''):
                    raise RuntimeError('Missing labeled monthly money direction')
                if 'CityTrendText1' not in texts:
                    raise RuntimeError('Missing population trend')
            samples.append({'name': names[index], 'texts': texts})
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(), f'HighResShot 1800x1400 filename="{outputs[index].as_posix()}"')
            phase += 1
            deadline = now + 3
        elif phase == 3:
            ui.set_debug_mode_enabled(True)
            ui.show_debug_tools()
            phase = 4
            deadline = now + 3
        elif phase == 5:
            ui.set_debug_mode_enabled(False)
            ui.return_to_city()
            ui.set_city_summary_visible(True)
            fixture = max((root / 'CitySimVR/Saved/Automation/CivicBudget-Civic').rglob('learning.json'), key=lambda p: p.stat().st_mtime)
            (saved / 'CitySaveV1.json').write_bytes(fixture.read_bytes())
            sim = next(obj for obj in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if obj.get_outer() == editor.get_game_world())
            if not sim.load_city(): raise RuntimeError('Native progression fixture did not load')
            sim.set_simulation_paused(True)
            board.set_lighting_preset(unreal.CityLightingPreset.DAY)
            phase = 6
            deadline = now + 3
        elif phase == 7:
            fixture = max((root / 'CitySimVR/Saved/Automation/CivicBudget-Civic').rglob('payroll.json'), key=lambda p: p.stat().st_mtime)
            (saved / 'CitySaveV1.json').write_bytes(fixture.read_bytes())
            sim = next(obj for obj in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if obj.get_outer() == editor.get_game_world())
            if not sim.load_city(): raise RuntimeError('Native payroll fixture did not load')
            sim.set_simulation_paused(True)
            board.set_lighting_preset(unreal.CityLightingPreset.NIGHT)
            phase = 8
            deadline = now + 3
        elif phase == 9:
            board.set_tool_mode(unreal.CityToolMode.DEMOLISH)
            ui.show_game_settings()
            phase = 10
            deadline = now + 3
        elif phase == 11:
            if not all(p.exists() and p.stat().st_size for p in outputs):
                raise RuntimeError('A native capture is missing')
            report.write_text(json.dumps({'source': 'Native PIE menu commands and real HUD, day/night', 'samples': samples, 'localSaveCatalogPreserved': True, 'savedMap': False}, indent=2), encoding='utf-8')
            finish()
    except Exception:
        error.write_text(traceback.format_exc(), encoding='utf-8')
        finish()

handle = unreal.register_slate_post_tick_callback(tick)
