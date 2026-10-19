"""Record actual decoded D58 MetaSounds in isolated native PIE, without saving a map."""
from pathlib import Path
import json
import time
import traceback
import unreal

root = Path(__file__).resolve().parents[1]
rain_only = '-CityRainPreview' in unreal.SystemLibrary.get_command_line()
traffic_only = '-CityTrafficAudioPreview' in unreal.SystemLibrary.get_command_line()
out = root / ('Artifacts/D58/TrafficAlarmRefinement/BankPlayback' if traffic_only else
              'Artifacts/D58/RainSirenRefinement/BankPlayback' if rain_only else 'Artifacts/D58/Integration/BankPlayback')
out.mkdir(parents=True, exist_ok=True)
report = out / 'NativePlayback.json'
if report.exists():
    raise RuntimeError('Preserve the existing native playback report before repeating this probe')
manifest = json.loads((root / 'AudioSource/Soundscape/Unreal/Manifest.json').read_text())
selected = [a for a in manifest['assets'] if (not rain_only or a['id'].startswith('Rain-')) and
            (not traffic_only or a['bank']=='Traffic')]
jobs = [(a, min(a['seconds'] + .5, 3.0), False) for a in selected]
for name in ([a['id'] for a in selected] if rain_only or traffic_only else ['Police-Loop', 'Fire-Loop', 'Ambulance-Loop', 'Town-Loop', 'Rain-Light', 'Night-Crickets-Light']):
    asset = next(a for a in manifest['assets'] if a['id'] == name)
    jobs.append((asset, asset['seconds'] * 2 + 1.0, True))
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
start = time.monotonic()
deadline = start + 2
phase = 0
index = 0
records = []
audio = None


def finish():
    if audio:
        audio.stop()
    level.editor_request_end_play()
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(delta):
    global phase, deadline, index, audio, world
    try:
        now = time.monotonic()
        if now - start > 720:
            raise RuntimeError('Native bank playback timed out')
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
            unreal.SystemLibrary.execute_console_command(world, 't.IdleWhenNotForeground 0')
            unreal.SystemLibrary.execute_console_command(world, 'au.DisableAppVolume 1')
            phase = 2
            deadline = now + 1
        elif phase == 2:
            if index == len(jobs):
                report.write_text(json.dumps({'status': 'Pass', 'jobs': records,
                    'scope': 'Native editor decoded four steady traffic SoundCues and repeated loops; runtime/device listening separate' if traffic_only else 'Native editor decoded rain and four repeated loops; runtime/device listening separate' if rain_only else 'Native editor decoded bank auditions and six repeated-loop recordings; runtime routing/platform/device checks separate'}, indent=2))
                finish()
                return
            asset, seconds, repeated = jobs[index]
            graph_name = asset['assetName'].replace('SW_', 'SC_' if asset['bank']=='Traffic' else 'MS_', 1)
            graph = unreal.load_asset('/Game/CitySim/Audio/Soundscape/Graphs/' + graph_name)
            if not graph:
                raise RuntimeError('Missing native graph ' + graph_name)
            unreal.AudioMixerLibrary.start_recording_output(world, seconds + 2)
            audio = unreal.GameplayStatics.spawn_sound2d(world, graph, auto_destroy=False)
            if not audio:
                raise RuntimeError('No native audio component ' + asset['id'])
            phase = 3
            deadline = now + seconds
        elif phase == 3:
            asset, seconds, repeated = jobs[index]
            playing = audio.is_playing()
            if repeated and not playing:
                raise RuntimeError('Explicit MetaSound loop ended: ' + asset['id'])
            if not asset['loop'] and seconds > asset['seconds'] and playing:
                raise RuntimeError('One-shot did not finish: ' + asset['id'])
            name = f'{index:02d}-' + asset['id'] + ('-Repeated' if repeated else '')
            unreal.AudioMixerLibrary.stop_recording_output(world, unreal.AudioRecordingExportType.WAV_FILE, name, str(out))
            audio.stop()
            records.append({'id': asset['id'], 'file': name + '.wav', 'seconds': seconds,
                            'repeated': repeated, 'playingAtEnd': playing})
            unreal.log('D58_NATIVE_PLAYBACK_PASS ' + name)
            index += 1
            phase = 2
            deadline = now + .35
    except Exception:
        (out / 'NativePlayback.error.txt').write_text(traceback.format_exc())
        unreal.log_error(traceback.format_exc())
        finish()


handle = unreal.register_slate_post_tick_callback(tick)
