"""Actual-game audio lifecycle probe; launch with the dedicated RuntimeUser profile."""
from pathlib import Path
import json
import re
import time
import traceback
import unreal

root = Path(__file__).resolve().parents[1]
out = root / 'Artifacts/D58/Integration/RuntimePlayback'
out.mkdir(parents=True, exist_ok=True)
report = out / 'Runtime.json'
if report.exists():
    raise RuntimeError('Preserve the prior runtime report before repeating this probe')
text = (root / 'CitySimVR/Source/CitySimVR/Public/Interaction/CityPaletteModel.h').read_text()
body = re.search(r'enum class ECityPaletteAction\s*:\s*\w+\s*\{(.*?)\};', text, re.S).group(1)
actions = {}
value = 0
for name, explicit in re.findall(r'^\s*(\w+)\s*(?:=\s*(\d+))?\s*,?\s*$', body, re.M):
    if explicit:
        value = int(explicit)
    actions[name] = value
    value += 1
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
phase = 0
start = time.monotonic()
deadline = start + 2
checks = {}
samples = []


def playing():
    return [p.get_editor_property('sound').get_name() for p in board.get_components_by_class(unreal.AudioComponent)
            if p.is_playing() and p.get_editor_property('sound')]


def audio_choice(name):
    # Hidden native windows correctly reject mouse dispatch. Use the same saved
    # presentation API here; actual button dispatch is checked by packaged smoke.
    settings = unreal.get_default_object(unreal.CityAudioSettings)
    volume = {'Mute': 0.0, '25': .25, '50': .5, '75': .75, '100': 1.0}
    if name.startswith('Alarm'):
        settings.set_alarm_volume(volume[name.removeprefix('Alarm')])
    else:
        settings.set_ambience_volume(volume[name.removeprefix('Ambience')])


def finish():
    level.editor_request_end_play()
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(delta):
    global phase, deadline, board, ui, world
    try:
        now = time.monotonic()
        if now - start > 100:
            raise RuntimeError('Runtime audio probe timed out')
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
            board = next(a for a in actors if a.get_class().get_name() == 'CityBoardActor')
            ui = next(a for a in actors if a.get_class().get_name() == 'CityVRToolController')
            unreal.SystemLibrary.execute_console_command(world, 't.IdleWhenNotForeground 0')
            unreal.SystemLibrary.execute_console_command(world, 'au.DisableAppVolume 1')
            if playing():
                raise RuntimeError('Entry menu has stale city audio')
            checks['entryQuiet'] = True
            ui.review_city_load()
            ui.confirm_reviewed_city_load()
            board.set_city_paused(False)
            ui.set_debug_mode_enabled(True)
            phase = 2
            deadline = now + 1
        elif phase == 2:
            if not playing():
                raise RuntimeError('Running city has no native ambient bed')
            checks['ambienceRuns'] = True
            if not board.start_natural_disaster_test(True):
                raise RuntimeError('Controlled tornado warning rejected')
            phase = 3
            deadline = now + .3
        elif phase == 3:
            names = playing()
            if 'MS_Town_Intro' not in names:
                raise RuntimeError('New warning did not start Town G intro: ' + str(names))
            checks['newWarningIntro'] = True
            phase = 4
            deadline = now + 4.5
        elif phase == 4:
            names = playing()
            if 'MS_Town_Loop' not in names or 'MS_Town_Intro' in names:
                raise RuntimeError('Warning failed its intro-to-loop transition: ' + str(names))
            checks['introToLoop'] = True
            board.set_city_paused(True)
            phase = 5
            deadline = now + .15
        elif phase == 5:
            if playing():
                raise RuntimeError('Pause did not cancel all players: ' + str(playing()))
            checks['pauseCancels'] = True
            board.set_city_paused(False)
            phase = 6
            deadline = now + .3
        elif phase == 6:
            names = playing()
            if 'MS_Town_Loop' not in names or 'MS_Town_Intro' in names or 'MS_Incident_Notice' in names:
                raise RuntimeError('Resume replayed onset instead of restoring Loop: ' + str(names))
            checks['resumeDirectLoop'] = True
            simulation = board.get_city_simulation()
            if not simulation.save_city():
                raise RuntimeError('Isolated active-warning save failed')
            ui.show_main_menu()
            phase = 7
            deadline = now + .15
        elif phase == 7:
            if playing():
                raise RuntimeError('Menu retained old city audio')
            checks['menuCancels'] = True
            ui.review_city_load()
            ui.confirm_reviewed_city_load()
            board.set_city_paused(False)
            phase = 8
            deadline = now + .4
        elif phase == 8:
            names = playing()
            if 'MS_Town_Loop' not in names or 'MS_Town_Intro' in names:
                raise RuntimeError('Loaded warning replayed or disappeared: ' + str(names))
            checks['loadDirectLoop'] = True
            audio_choice('AlarmMute')
            phase = 9
            deadline = now + .15
        elif phase == 9:
            names = playing()
            if any(n.startswith('MS_Town') for n in names) or not names:
                raise RuntimeError('Effects mute did not leave independent ambience: ' + str(names))
            checks['independentEffectsMute'] = True
            audio_choice('AmbienceMute')
            phase = 10
            deadline = now + .15
        elif phase == 10:
            if playing():
                raise RuntimeError('Both mutes left audio playing')
            checks['bothMutesQuiet'] = True
            audio_choice('Alarm25')
            audio_choice('Ambience50')
            phase = 11
            deadline = now + .3
        elif phase == 11:
            names = playing()
            if 'MS_Town_Loop' not in names or 'MS_Town_Intro' in names:
                raise RuntimeError('Unmute failed direct loop restoration: ' + str(names))
            checks['unmuteDirectLoop'] = True
            samples.append({'phase': phase, 'playing': names, 'count': len(names)})
            board.set_city_paused(True)
            if not board.preview_disaster_alarm():
                raise RuntimeError('Paused Town G preview rejected')
            phase = 12
            deadline = now + .25
        elif phase == 12:
            if playing() != ['MS_Town_Loop']:
                raise RuntimeError('Preview does not use the selected Town G source: ' + str(playing()))
            checks['previewUsesTownG'] = True
            phase = 13
            deadline = now + 5.2
        elif phase == 13:
            if playing():
                raise RuntimeError('Town G preview exceeded its five-second bound')
            checks['previewEnds'] = True
            ui.show_new_city_setup()
            ui.review_city_creation()
            ui.confirm_reviewed_city_creation()
            board.set_city_paused(False)
            phase = 14
            deadline = now + .3
        elif phase == 14:
            names = playing()
            if any(any(part in n for part in ['Town', 'Police', 'Fire', 'Ambulance', 'Park', 'Industry', 'Market', 'Incident']) for n in names):
                raise RuntimeError('Fresh city retained old-city audio: ' + str(names))
            checks['newCityClearsOldLayers'] = True
            report.write_text(json.dumps({'status': 'Pass', 'checks': checks, 'samples': samples,
                'scope': 'Actual native Desktop PIE lifecycle and independent controls; device/rendering/spatial mix and dense cost remain separate'}, indent=2))
            finish()
            return
        samples.append({'phase': phase, 'playing': playing() if phase > 1 else [], 'time': now-start})
    except Exception:
        (out / 'Runtime.error.txt').write_text(traceback.format_exc())
        unreal.log_error(traceback.format_exc())
        finish()


handle = unreal.register_slate_post_tick_callback(tick)
