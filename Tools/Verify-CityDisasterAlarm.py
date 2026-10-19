"""Render the actual MetaSound in temporary native PIE; preserve local city files."""
from pathlib import Path
import json
import time
import traceback
import unreal

root = Path(__file__).resolve().parents[1]
output = root / 'Artifacts/Preview'
report = output / 'D57-AlarmPIE.json'
error = output / 'D57-AlarmPIE.error.txt'
wav = output / 'D57-CityAlarm.wav'
for path in (report, error, wav):
    path.unlink(missing_ok=True)
saved = root / 'CitySimVR/Saved/SaveGames'
paths = [saved / 'CitySaveV1.json', saved / 'Cities/LibraryIndex.json',
         saved / 'Cities/LibraryIndex.json.bak', saved / 'Cities/LibraryIndex.json.tmp']
prior = {p: p.read_bytes() if p.exists() else None for p in paths}
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
start = time.monotonic()
deadline = start + 3
phase = 0
checks = {}


def finish():
    level.editor_request_end_play()
    for p, data in prior.items():
        if data is None:
            p.unlink(missing_ok=True)
        else:
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(data)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(delta):
    global phase, deadline, board, ui, world, audio, count
    try:
        now = time.monotonic()
        if now - start > 100:
            raise RuntimeError('Alarm verification timed out')
        if now < deadline:
            return
        if phase == 0:
            level.editor_request_begin_play()
            phase = 1
            deadline = now + 7
        elif phase == 1:
            world = editor.get_game_world()
            if not world:
                return
            actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            ui = next(a for a in actors if a.get_class().get_name() == 'CityVRToolController')
            board = next(a for a in actors if a.get_class().get_name() == 'CityBoardActor')
            paths[0].write_bytes((output / 'D51-NaturalBaseline.json').read_bytes())
            ui.show_main_menu()
            ui.review_city_load()
            ui.confirm_reviewed_city_load()
            board.set_city_paused(True)
            audio = next(c for c in board.get_components_by_class(unreal.AudioComponent)
                         if c.get_name() == 'DisasterAlarmAudio')
            unreal.SystemLibrary.execute_console_command(world, 't.IdleWhenNotForeground 0')
            # Hidden editor windows otherwise mute the mixer through application focus.
            unreal.SystemLibrary.execute_console_command(world, 'au.DisableAppVolume 1')
            phase = 2
            deadline = now + 1
        elif phase == 2:
            unreal.AudioMixerLibrary.start_recording_output(world, 7.0)
            if not board.preview_disaster_alarm():
                raise RuntimeError('Paused city preview rejected')
            phase = 3
            deadline = now + 6
        elif phase == 3:
            unreal.AudioMixerLibrary.stop_recording_output(
                world, unreal.AudioRecordingExportType.WAV_FILE, 'D57-CityAlarm', str(output))
            if audio.is_playing():
                raise RuntimeError('One-shot alarm did not finish')
            checks['previewFinishes'] = True
            count = board.get_disaster_alarm_play_count()
            if not board.start_natural_disaster_test(False):
                raise RuntimeError('Quake test rejected')
            board.set_city_paused(False)
            phase = 4
            deadline = now + 3
        elif phase == 4:
            if board.get_disaster_alarm_play_count() != count + 1 or not audio.is_playing():
                raise RuntimeError('Quake onset did not play exactly once')
            checks['quakeOnset'] = True
            board.set_city_paused(True)
            phase = 5
            deadline = now + .3
        elif phase == 5:
            if audio.is_playing():
                raise RuntimeError('Pause did not stop cue')
            board.set_city_paused(False)
            phase = 6
            deadline = now + .4
        elif phase == 6:
            if audio.is_playing() or board.get_disaster_alarm_play_count() != count + 1:
                raise RuntimeError('Resume replayed cue')
            checks['pauseResumeNoReplay'] = True
            board.preview_disaster_alarm()
            ui.show_main_menu()
            phase = 7
            deadline = now + .3
        elif phase == 7:
            if audio.is_playing():
                raise RuntimeError('Main menu did not stop cue')
            checks['mainMenuStops'] = True
            paths[0].write_bytes((output / 'D51-NaturalBaseline.json').read_bytes())
            ui.review_city_load()
            ui.confirm_reviewed_city_load()
            phase = 8
            deadline = now + .5
        elif phase == 8:
            count = board.get_disaster_alarm_play_count()
            if not board.start_natural_disaster_test(True):
                raise RuntimeError('Tornado test rejected')
            board.set_city_paused(False)
            phase = 9
            deadline = now + 3
        elif phase == 9:
            if board.get_disaster_alarm_play_count() != count + 1 or not audio.is_playing():
                raise RuntimeError('Tornado onset did not play exactly once')
            checks['tornadoOnset'] = True
            report.write_text(json.dumps({'checks': checks, 'recording': wav.name,
                                          'source': 'native PIE actual MetaSound master-submix capture'}, indent=2))
            finish()
    except Exception:
        error.write_text(traceback.format_exc())
        finish()


handle = unreal.register_slate_post_tick_callback(tick)
