"""D88 actual pooled sports poses in isolated PIE; no player/map writes."""
from pathlib import Path
import unreal,json,time,hashlib,traceback
ROOT=Path(__file__).resolve().parents[1];COMMAND_LINE=unreal.SystemLibrary.get_command_line()
SCALE_REVIEW='CityD88ScalePreview' in COMMAND_LINE
assert SCALE_REVIEW==('D88/Scale/NativePreview' in COMMAND_LINE.replace('\\','/')),'Capture flag/UserDir disagree; refuse writing previews'
OUT=ROOT/('Artifacts/D88/Scale/NativePreview' if SCALE_REVIEW else 'Artifacts/D88/NativePreview');OUT.mkdir(parents=True,exist_ok=True)
source=ROOT/'Artifacts/Preview/Parks-Sports-City.json';source_hash=hashlib.sha256(source.read_bytes()).hexdigest()
document=json.loads(source.read_text(encoding='utf-8-sig'))
for key in ('broadElevationAmplitudeCm','hillAmplitudeCm','ridgeAmplitudeCm'):document['terrain'][key]=0
fixture=OUT/'InputCity.json';fixture.write_text(json.dumps(document))
entities={e['definitionId']:int(e['entityId']) for e in document['buildings'] if e['definitionId'] in ('BaseballPark','SoccerPark')}
jobs=[]
for clip,entity,times in [('BaseballPitch','BaseballPark',(0,.7,35/24,2.1)),('BaseballSwing','BaseballPark',(.5,1.6,2,2.4)),
                          ('SoccerKick','SoccerPark',(.5,1,1.3)),('SoccerRun','SoccerPark',(.2,.65)),('GoalkeeperSave','SoccerPark',(.4,1,1.45))]:
    period=3 if clip.startswith('Baseball') else 2
    offset=(entities[entity]%13)*.37
    for index,t in enumerate(times):jobs.append(dict(clip=clip,time=t,clock=(t-offset)%(period*2),name=f'{clip}-{index:02}'))
if SCALE_REVIEW:
    for clip,entity,period in [('GoalkeeperSave','SoccerPark',2),('BaseballSwing','BaseballPark',3)]:
        t=1 if clip=='GoalkeeperSave' else 2;offset=(entities[entity]%13)*.37
        jobs.append(dict(clip=clip,time=t,clock=(t-offset)%(period*2),name=clip+'-ScaleOverview',overview=True))
map_path=ROOT/'CitySimVR/Content/CitySim/Maps/CitySandbox.umap';map_hash=hashlib.sha256(map_path.read_bytes()).hexdigest()
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
camera=actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(),unreal.Rotator(),transient=False)
camera_name=camera.get_name();phase=0;index=0;started=time.monotonic();deadline=started+2;records=[];failure=None;board=sim=None
def finish():
    level.editor_request_end_play();unreal.unregister_slate_post_tick_callback(handle)
    assert hashlib.sha256(source.read_bytes()).hexdigest()==source_hash and hashlib.sha256(map_path.read_bytes()).hexdigest()==map_hash
    (OUT/'Report.json').write_text(json.dumps(dict(task='D88',views=records,error=failure,map_unchanged=True,source_city_unchanged=True,
        source='actual native pooled actors/animation/props; desktop rendering'),indent=2)+'\n')
    unreal.SystemLibrary.quit_editor()
def clip_name(component):
    instance=component.get_anim_instance();asset=instance.get_animation_asset() if instance else None
    return asset.get_name() if asset else ''
def tick(delta):
    global phase,index,deadline,board,sim,failure
    try:
        now=time.monotonic()
        if now-started>220:raise RuntimeError('D88 native sports capture timed out')
        if now<deadline:return
        if phase==0:level.editor_request_begin_play();phase=1;deadline=now+5
        elif phase==1:
            world=editor.get_game_world()
            if not world:return
            live=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
            board=next(a for a in live if a.get_class().get_name()=='CityBoardActor')
            ui=next(a for a in live if a.get_class().get_name()=='CityVRToolController');ui.return_to_city();ui.set_actor_hidden_in_game(True)
            sim=next(o for o in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if o.get_outer()==world)
            error=sim.load_city_from_path(str(fixture),False);assert error=='',error;sim.set_simulation_paused(True)
            target=next(a for a in live if a.get_name()==camera_name);target.get_component_by_class(unreal.CameraComponent).set_field_of_view(45)
            unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(target,0)
            for command in ('DisableAllScreenMessages','t.IdleWhenNotForeground 0','r.SetNearClipPlane 0.01'):unreal.SystemLibrary.execute_console_command(world,command)
            board.set_lighting_preset(unreal.CityLightingPreset.DAY);board.set_diagnostic_overlay(unreal.CityDiagnosticOverlay.NONE)
            phase=2;deadline=now+2
        elif phase==2:
            if index==len(jobs):finish();return
            job=jobs[index];assert unreal.CityParkAssetLibrary.set_park_capture_clock(board,job['clock'])
            active=[c for c in board.get_components_by_class(unreal.SkeletalMeshComponent) if c.is_visible()]
            matches=[c for c in active if clip_name(c)=='AS_Park_'+job['clip']];assert matches,('Missing native clip',job,[clip_name(c) for c in active])
            hero=matches[0];scale=board.get_actor_transform().scale3d.x;up=board.get_actor_up_vector()
            aim=hero.get_world_location()+up*(80*scale)
            eye=aim+hero.get_forward_vector()*(300*scale)-hero.get_right_vector()*(420*scale)+up*(180*scale)
            if job.get('overview'):eye=aim+hero.get_forward_vector()*(950*scale)-hero.get_right_vector()*(750*scale)+up*(450*scale)
            world=editor.get_game_world();target=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor) if a.get_name()==camera_name)
            target.set_actor_location(eye,False,True);target.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye,aim),True)
            records.append(dict(**job,component=hero.get_name(),animation=clip_name(hero),mesh=hero.get_editor_property('skeletal_mesh_asset').get_name(),
                                visible_pool=len(active),head=list((hero.get_socket_location('head').x,hero.get_socket_location('head').y,hero.get_socket_location('head').z))))
            phase=3;deadline=now+.4
        elif phase==3:
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(),f'HighResShot 1600x1000 filename="{(OUT/(jobs[index]["name"]+".png")).as_posix()}"')
            phase=4;deadline=now+.7
        else:
            assert (OUT/(jobs[index]['name']+'.png')).exists();index+=1;phase=2;deadline=now+.1
    except Exception:
        failure=traceback.format_exc();(OUT/'Error.txt').write_text(failure);finish()
handle=unreal.register_slate_post_tick_callback(tick)
