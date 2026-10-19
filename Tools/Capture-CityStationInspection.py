"""Capture actual station inspection UI in PIE; preserve the local save/map."""
from pathlib import Path
import hashlib
import json
import time
import traceback
import unreal

root=Path(__file__).resolve().parents[1]
out=root/'Artifacts/Preview'
fixture=root/'Artifacts/Diagnostics/D48-stations-device-before.json'
save=root/'CitySimVR/Saved/SaveGames/CitySaveV1.json'
report=out/'D48-StationInspection.json'
error=report.with_suffix('.error.txt')
outputs=[out/f'D48-StationInspection-{name}.png' for name in ('PoliceDay','FireDay','FireNight')]
for path in outputs+[report,error]:path.unlink(missing_ok=True)
prior=save.read_bytes() if save.exists() else None
prior_hash=hashlib.sha256(prior).hexdigest() if prior is not None else None
needs_restore=False
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
start=time.monotonic()
deadline=start+5
phase=0
samples=[]
board=None
ui=None
document=json.loads(fixture.read_text(encoding='utf-8-sig'))
stations=[next(e for e in document['buildings'] if e['definitionId']==kind) for kind in ('Police','Fire')]


def restore():
    global needs_restore
    if not needs_restore:return
    if prior is None:save.unlink(missing_ok=True)
    else:save.write_bytes(prior)
    needs_restore=False


def finish():
    restore()
    unreal.unregister_slate_post_tick_callback(handle)
    level.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()


def show_station(world,entry):
    pose=board.get_actor_transform()
    centre=unreal.Vector((entry['x']+entry['width']*.5)*800,(entry['y']+entry['height']*.5)*800,1200)
    aim=unreal.MathLibrary.transform_location(pose,centre+unreal.Vector(0,0,4000))
    location=unreal.MathLibrary.transform_location(pose,centre+unreal.Vector(0,-12000,16000))
    pawn=unreal.GameplayStatics.get_player_pawn(world,0)
    pawn.set_actor_location(location-unreal.Vector(0,0,pawn.get_editor_property('base_eye_height')),False,True)
    unreal.GameplayStatics.get_player_controller(world,0).set_control_rotation(unreal.MathLibrary.find_look_at_rotation(location,aim))
    ui.inspect_building(int(entry['entityId']))


def sample(name):
    texts={c.get_name():str(c.get_editor_property('text')) for c in ui.get_components_by_class(unreal.TextRenderComponent)
           if c.get_name().startswith('Inspector') and c.is_visible()}
    label=texts.get('InspectorUpgradeLabel','')
    if 'UPGRADE' not in label or '$' not in label:raise RuntimeError('Missing priced upgrade action: '+str(texts))
    samples.append({'name':name,'texts':texts})


def tick(delta):
    global phase,deadline,needs_restore,board,ui
    try:
        now=time.monotonic()
        if now-start>120:raise RuntimeError('Station inspection preview timed out')
        if now<deadline:return
        if phase==0:
            save.parent.mkdir(parents=True,exist_ok=True)
            if prior is not None:(out/'D48-StationInspection-LocalSaveBackup.json').write_bytes(prior)
            needs_restore=True
            save.write_bytes(fixture.read_bytes())
            level.editor_request_begin_play()
            phase=1;deadline=now+8
        elif phase==1:
            world=editor.get_game_world()
            if not world or not level.is_in_play_in_editor():return
            sim=next(obj for obj in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if obj.get_outer()==world)
            if not sim.load_city():raise RuntimeError('Existing city fixture failed native load')
            restore()
            sim.set_simulation_paused(True)
            actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
            board=next(a for a in actors if a.get_class().get_name()=='CityBoardActor')
            ui=next(a for a in actors if a.get_class().get_name()=='CityVRToolController')
            board.set_lighting_preset(unreal.CityLightingPreset.DAY)
            for component in ui.get_components_by_class(unreal.PrimitiveComponent):
                if not component.get_name().startswith('Inspector'):component.set_editor_property('render_in_main_pass',False)
            for command in ('FOV 60','DisableAllScreenMessages','t.IdleWhenNotForeground 0','r.EyeAdaptationQuality 0'):
                unreal.SystemLibrary.execute_console_command(world,command)
            # Let the board rebuild the newly loaded placement model before selection.
            phase=15;deadline=now+2
        elif phase==15:
            show_station(editor.get_game_world(),stations[0])
            phase=2;deadline=now+4
        elif phase in (2,4,6):
            index=(phase-2)//2
            sample(('PoliceDay','FireDay','FireNight')[index])
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(),f'HighResShot 1800x1400 filename="{outputs[index].as_posix()}"')
            phase+=1;deadline=now+3
        elif phase==3:
            show_station(editor.get_game_world(),stations[1])
            phase=4;deadline=now+4
        elif phase==5:
            board.set_lighting_preset(unreal.CityLightingPreset.NIGHT)
            phase=6;deadline=now+3
        elif all(p.exists() and p.stat().st_size>0 for p in outputs):
            after=hashlib.sha256(save.read_bytes()).hexdigest() if save.exists() else None
            if after!=prior_hash:raise RuntimeError('Local save restoration mismatch')
            report.write_text(json.dumps({'source':'actual station inspection controls in temporary PIE with native validated existing city',
                'samples':samples,'local_save_preserved':True,'prior_save_sha256':prior_hash,'saved_map':False},indent=2),encoding='utf-8')
            finish()
    except Exception:
        error.write_text(traceback.format_exc(),encoding='utf-8')
        finish()


handle=unreal.register_slate_post_tick_callback(tick)
