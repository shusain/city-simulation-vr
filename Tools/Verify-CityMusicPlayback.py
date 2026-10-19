"""Record native decoded D82 stereo waves at start/seek; isolated unsaved PIE only."""
from pathlib import Path
import json
import time
import traceback
import unreal

root = Path(__file__).resolve().parents[1]
command = unreal.SystemLibrary.get_command_line().replace("\\", "/")
if "-UserDir=" not in command or "Artifacts/D82" not in command:
    raise RuntimeError("Music capture requires an isolated D82 UserDir")
approved = "-D82ApprovedMusic" in command
stage = root/("AudioSource/Music/CitySoundtrack/UnrealApproved" if approved else "AudioSource/Music/CitySoundtrack/Unreal")
out = root/("Artifacts/D82/ApprovedIntegration/NativePlayback" if approved else "Artifacts/D82/NativePlayback")
out.mkdir(parents=True,exist_ok=True)
report = out/"Report.json"
if report.exists():
    raise RuntimeError("Preserve the previous native recording; use a deliberate new output")
manifest = json.loads((stage/"Manifest.json").read_text())
jobs = [(entry,0) for entry in manifest["assets"]] + [(entry,entry["seconds"]-9) for entry in manifest["assets"]]
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
start = time.monotonic()
phase,index = 0,0
deadline = start+2
audio = None
records = []

def finish():
    if audio:
        audio.stop()
    level.editor_request_end_play()
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()

def tick(delta):
    global phase,index,deadline,audio,world
    try:
        now = time.monotonic()
        if now-start > 180:
            raise RuntimeError("Music native recording timeout")
        if now < deadline:
            return
        if phase == 0:
            level.editor_request_begin_play()
            phase,deadline = 1,now+7
        elif phase == 1:
            world = editor.get_game_world()
            if not world:
                return
            unreal.SystemLibrary.execute_console_command(world,"t.IdleWhenNotForeground 0")
            unreal.SystemLibrary.execute_console_command(world,"au.DisableAppVolume 1")
            # Stop normal board presentation during source decoding capture. No map is saved.
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CityBoardActor):
                actor.set_actor_tick_enabled(False)
                for component in actor.get_components_by_class(unreal.AudioComponent):
                    component.stop()
            phase,deadline = 2,now+1
        elif phase == 2:
            if index == len(jobs):
                report.write_text(json.dumps({"status":"Pass","jobs":records,"scope":"Native decoded starts and late-track Bink seek; runtime/package/lifecycle tests separate; human listening pending"},indent=2))
                finish()
                return
            entry,offset = jobs[index]
            source = unreal.load_asset(entry["package"])
            if not source:
                raise RuntimeError("Native music missing")
            unreal.AudioMixerLibrary.start_recording_output(world,5)
            audio = unreal.GameplayStatics.spawn_sound2d(world,source,start_time=offset,auto_destroy=False)
            if not audio:
                raise RuntimeError("Native player missing")
            phase,deadline = 3,now+4
        elif phase == 3:
            entry,offset = jobs[index]
            if not audio.is_playing():
                raise RuntimeError("Music stopped unexpectedly")
            name = f'{index:02d}-{entry["id"]}'
            unreal.AudioMixerLibrary.stop_recording_output(world,unreal.AudioRecordingExportType.WAV_FILE,name,str(out))
            audio.stop()
            records.append({"id":entry["id"],"file":name+".wav","seekSeconds":offset,"seconds":4,"playingAtEnd":True})
            index += 1
            phase,deadline = 2,now+0.5
    except Exception:
        (out/"Error.txt").write_text(traceback.format_exc())
        unreal.log_error(traceback.format_exc())
        finish()

handle = unreal.register_slate_post_tick_callback(tick)
