"""Capture real ConditionIndicators pools in temporary PIE, restoring the local city byte-for-byte."""
from pathlib import Path
import hashlib
import json
import time
import traceback
import unreal

root=Path(__file__).resolve().parents[1]
out=root/'Artifacts/Preview'
fixture=out/'ConditionIndicators-Indicator-City.json'
save=root/'CitySimVR/Saved/SaveGames/CitySaveV1.json'
report=out/'ConditionIndicators-Indicators.json'
error=report.with_suffix('.error.txt')
labels=('Day','Night','Hidden')
outputs=[out/f'ConditionIndicators-Indicators-{label}.png' for label in labels]
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


def restore_save():
    global needs_restore
    if not needs_restore:return
    if prior is None:save.unlink(missing_ok=True)
    else:save.write_bytes(prior)
    needs_restore=False


def finish():
    restore_save()
    unreal.unregister_slate_post_tick_callback(handle)
    level.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()


def sample(label):
    roots=[]
    for component in ui.get_components_by_class(unreal.SceneComponent):
        if component.get_name().startswith('ConditionRoot') and component.is_visible():
            v=component.get_world_location()
            roots.append({'name':component.get_name(),'position':[v.x,v.y,v.z]})
    texts={c.get_name():str(c.get_editor_property('text')) for c in ui.get_components_by_class(unreal.TextRenderComponent)
           if c.get_name().startswith('Condition') and c.is_visible()}
    samples.append({'label':label,'visible_badges':len(roots),'roots':roots,'texts':texts})


def tick(delta):
    global phase,deadline,needs_restore,board,ui
    try:
        now=time.monotonic()
        if now-start>120:raise RuntimeError('Condition preview timed out')
        if now<deadline:return
        if phase==0:
            document=json.loads(fixture.read_text(encoding='utf-8-sig'))
            # A synthetic halfway repair deadline demonstrates the default ten-day
            # progress display. The native load still validates the whole fixture.
            for damage in document['fire']['damage']:
                if int(damage['repairCompleteStep'])>=0:damage['repairCompleteStep']='5'
            save.parent.mkdir(parents=True,exist_ok=True)
            if prior is not None:(out/'ConditionIndicators-Indicators-LocalSaveBackup.json').write_bytes(prior)
            needs_restore=True
            save.write_text(json.dumps(document),encoding='utf-8')
            level.editor_request_begin_play()
            phase=1;deadline=now+8
        elif phase==1:
            world=editor.get_game_world()
            if not world or not level.is_in_play_in_editor():return
            sim=next(obj for obj in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if obj.get_outer()==world)
            if not sim.load_city():raise RuntimeError('Condition fixture failed native load validation')
            restore_save()
            sim.set_simulation_paused(True)
            actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
            board=next(a for a in actors if a.get_class().get_name()=='CityBoardActor')
            ui=next(a for a in actors if a.get_class().get_name()=='CityVRToolController')
            board.set_lighting_preset(unreal.CityLightingPreset.DAY)
            ui.set_condition_indicators_visible(True)
            for component in ui.get_components_by_class(unreal.PrimitiveComponent):
                if not component.get_name().startswith('Condition'):component.set_editor_property('render_in_main_pass',False)
            transform=board.get_actor_transform()
            location=unreal.MathLibrary.transform_location(transform,unreal.Vector(6400,4500,9500))
            aim=unreal.MathLibrary.transform_location(transform,unreal.Vector(6400,10400,300))
            pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            pawn.set_actor_location(location-unreal.Vector(0,0,pawn.get_editor_property('base_eye_height')),False,True)
            unreal.GameplayStatics.get_player_controller(world,0).set_control_rotation(unreal.MathLibrary.find_look_at_rotation(location,aim))
            for command in ('FOV 60','DisableAllScreenMessages','t.IdleWhenNotForeground 0','r.EyeAdaptationQuality 0'):
                unreal.SystemLibrary.execute_console_command(world,command)
            phase=2;deadline=now+3
        elif phase in (2,4,6):
            index=(phase-2)//2
            sample(labels[index])
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(),f'HighResShot 1800x1400 filename="{outputs[index].as_posix()}"')
            phase+=1;deadline=now+3
        elif phase==3:
            board.set_lighting_preset(unreal.CityLightingPreset.NIGHT)
            phase=4;deadline=now+3
        elif phase==5:
            ui.set_condition_indicators_visible(False)
            phase=6;deadline=now+3
        elif all(p.exists() and p.stat().st_size>0 for p in outputs):
            if [s['visible_badges'] for s in samples]!=[3,3,0]:raise RuntimeError('Incorrect day/night/hidden badge counts')
            after_hash=hashlib.sha256(save.read_bytes()).hexdigest() if save.exists() else None
            if after_hash!=prior_hash:raise RuntimeError('Local save restoration did not match original')
            report.write_text(json.dumps({'source':'actual pooled ConditionIndicators components in temporary PIE with validated mixed-condition fixture and synthetic halfway repair deadline','samples':samples,'local_save_preserved':True,'prior_save_sha256':prior_hash,'saved_map':False},indent=2),encoding='utf-8')
            finish()
    except Exception:
        error.write_text(traceback.format_exc()+'\n'+json.dumps(samples,indent=2),encoding='utf-8')
        finish()


handle=unreal.register_slate_post_tick_callback(tick)
