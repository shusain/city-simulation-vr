"""Capture actual pooled park activity in temporary PIE, preserving the local save."""
from pathlib import Path
import hashlib
import json
import time
import traceback
import unreal

root=Path(__file__).resolve().parents[1]
out=root/'Artifacts/Preview'
terrain_preview='-CityParkPreviewTerrain' in unreal.SystemLibrary.get_command_line()
prefix='Parks-ParkTerrainActivity' if terrain_preview else 'Parks-ParkActivity'
fixture=out/'Parks-Sports-City.json'
save=root/'CitySimVR/Saved/SaveGames/CitySaveV1.json'
report=out/f'{prefix}.json'
error=report.with_suffix('.error.txt')
outputs=[out/f'{prefix}-{name}.png' for name in ('DayA','DayB','FieldsDay','Night')]
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
sim=None
document_terrain=None


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
    data={}
    for component in board.get_components_by_class(unreal.InstancedStaticMeshComponent):
        if not component.get_name().startswith('Park'):continue
        count=component.get_instance_count()
        positions=[]
        for i in range(min(count,4)):
            pose=component.get_instance_transform(i,False)
            v=pose.translation
            positions.append([v.x,v.y,v.z])
        data[component.get_name()]={'count':count,'positions':positions,'visible':component.is_visible()}
    samples.append({'label':label,'pools':data})


def set_camera(fields):
    world=editor.get_game_world()
    transform=board.get_actor_transform()
    center=unreal.Vector(23400,22800,100) if fields else unreal.Vector(39500,10300,100)
    eye=unreal.Vector(23400,14000,12000) if fields else unreal.Vector(39500,1000,12500)
    location=unreal.MathLibrary.transform_location(transform,eye)
    aim=unreal.MathLibrary.transform_location(transform,center)
    pawn=unreal.GameplayStatics.get_player_pawn(world,0)
    controller=unreal.GameplayStatics.get_player_controller(world,0)
    pawn.set_actor_location(location-unreal.Vector(0,0,pawn.get_editor_property('base_eye_height')),False,True)
    controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(location,aim))


def tick(delta):
    global phase,deadline,needs_restore,board,sim,document_terrain
    try:
        now=time.monotonic()
        if now-start>120:raise RuntimeError('Park activity preview timed out')
        if now<deadline:return
        if phase==0:
            if not fixture.exists():raise RuntimeError('Run the park automation fixture before capturing activity')
            save.parent.mkdir(parents=True,exist_ok=True)
            if prior is not None:(out/f'{prefix}-LocalSaveBackup.json').write_bytes(prior)
            needs_restore=True
            document=json.loads(fixture.read_text(encoding='utf-8-sig'))
            if not terrain_preview:
                for key in ('broadElevationAmplitudeCm','hillAmplitudeCm','ridgeAmplitudeCm'):document['terrain'][key]=0
            document_terrain=document['terrain'].copy()
            save.write_text(json.dumps(document),encoding='utf-8')
            level.editor_request_begin_play()
            phase=1;deadline=now+8
        elif phase==1:
            world=editor.get_game_world()
            if not world or not level.is_in_play_in_editor():return
            sim=next(obj for obj in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if obj.get_outer()==world)
            if not sim.load_city():raise RuntimeError('Temporary park fixture failed to load')
            restore_save()
            actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
            board=next(a for a in actors if a.get_class().get_name()=='CityBoardActor')
            board.set_lighting_preset(unreal.CityLightingPreset.DAY)
            for actor in actors:
                if actor.get_class().get_name()=='CityVRToolController':
                    for component in actor.get_components_by_class(unreal.PrimitiveComponent):component.set_visibility(False,False)
            set_camera(False)
            for command in ('FOV 55','DisableAllScreenMessages','t.IdleWhenNotForeground 0','r.EyeAdaptationQuality 0'):
                unreal.SystemLibrary.execute_console_command(world,command)
            sim.set_simulation_paused(False)
            phase=2;deadline=now+3
        elif phase in (2,3,5,7):
            index={2:0,3:1,5:2,7:3}[phase]
            sample(('DayA','DayB','FieldsDay','Night')[index])
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(),f'HighResShot 1800x1400 filename="{outputs[index].as_posix()}"')
            phase+=1;deadline=now+3
        elif phase==4:
            set_camera(True)
            phase=5;deadline=now+3
        elif phase==6:
            set_camera(False)
            board.set_lighting_preset(unreal.CityLightingPreset.NIGHT)
            phase=7;deadline=now+3
        elif all(p.exists() and p.stat().st_size>0 for p in outputs):
            if samples[0]['pools']['ParkVisitorInstances']['positions']==samples[1]['pools']['ParkVisitorInstances']['positions']:
                raise RuntimeError('Actual visitor instances did not animate between captures')
            if any(p['count'] for p in samples[3]['pools'].values()):raise RuntimeError('Activity remained at night')
            after_hash=hashlib.sha256(save.read_bytes()).hexdigest() if save.exists() else None
            if after_hash!=prior_hash:raise RuntimeError('Local save restoration did not match original')
            terrain_kind='original generated terrain' if terrain_preview else 'flat preview terrain'
            report.write_text(json.dumps({'source':f'actual seven park styles and pooled activity in temporary desktop PIE on {terrain_kind}','terrain_parameters':document_terrain,'samples':samples,'local_save_preserved':True,'prior_save_sha256':prior_hash,'saved_map':False},indent=2),encoding='utf-8')
            finish()
    except Exception:
        error.write_text(traceback.format_exc(),encoding='utf-8')
        finish()


handle=unreal.register_slate_post_tick_callback(tick)
