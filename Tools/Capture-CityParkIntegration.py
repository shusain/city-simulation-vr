"""Actual D83 pooled citizens/park geometry in isolated PIE; no player/map writes."""
from pathlib import Path
import unreal,json,time,hashlib,shutil,traceback
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Artifacts/D83/Integration/Preview';OUT.mkdir(parents=True,exist_ok=True)
for name in ('Report.json','Error.txt'):(OUT/name).unlink(missing_ok=True)
source=ROOT/'Artifacts/Preview/Parks-Sports-City.json'
source_hash=hashlib.sha256(source.read_bytes()).hexdigest()
document=json.loads(source.read_text(encoding='utf-8-sig'))
for key in ('broadElevationAmplitudeCm','hillAmplitudeCm','ridgeAmplitudeCm'):document['terrain'][key]=0
fixture=OUT/'InputCity.json';fixture.write_text(json.dumps(document))
skate_id=int(next(e['entityId'] for e in document['buildings'] if e['definitionId']=='SkatePark'))
offset=(skate_id%13)*.37
views=[('Basic-Day',(45200,8400,700),(45200,5800,2100),0,'Day'),
       ('Neighborhood-Day',(18800,9200,500),(18800,4000,3900),0,'Day'),
       ('City-Day',(26800,10000,600),(26800,2000,6500),0,'Day'),
       ('Baseball-Day',(19600,22800,150),(19600,14800,6500),1,'Day'),
       ('Soccer-Day',(26800,22800,150),(26800,14800,6500),1,'Day')]
for i,t in enumerate((.9,1.1,1.3,1.5,1.7,2.1,2.6)):
    clock=t-offset
    if clock<0:clock+=15 # Common3.75/4.75 reset periods are sampled independently.
    views.append((f'Skate-{i:02}',(41100,9600,100),(40700,8950,580),clock,'Day'))
views.append(('Skate-Night',(40800,9600,200),(40800,6300,2300),1,'Night'))
map_path=ROOT/'CitySimVR/Content/CitySim/Maps/CitySandbox.umap'
map_hash=hashlib.sha256(map_path.read_bytes()).hexdigest()
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
camera=actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(),unreal.Rotator(),transient=False)
camera_name=camera.get_name();phase=0;index=0;deadline=time.monotonic()+2;started=time.monotonic()
board=sim=None;records=[];failure=None
def finish():
    level.editor_request_end_play()
    unreal.unregister_slate_post_tick_callback(handle)
    assert hashlib.sha256(source.read_bytes()).hexdigest()==source_hash
    assert hashlib.sha256(map_path.read_bytes()).hexdigest()==map_hash
    (OUT/'Report.json').write_text(json.dumps(dict(views=records,error=failure,source_city_unchanged=True,map_unchanged=True,
        source='actual game park instance/skeletal pools; isolated copied city; desktop rendering'),indent=2)+'\n')
    unreal.SystemLibrary.quit_editor()
def mesh_name(component):
    asset=component.get_editor_property('skeletal_mesh_asset')
    return asset.get_name() if asset else ''
def tick(delta):
    global phase,index,deadline,board,sim,failure
    try:
        now=time.monotonic()
        if now-started>220:raise RuntimeError('D83 preview timeout')
        if now<deadline:return
        if phase==0:
            level.editor_request_begin_play();phase=1;deadline=now+6
        elif phase==1:
            world=editor.get_game_world()
            if not world:return
            live=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
            board=next(a for a in live if a.get_class().get_name()=='CityBoardActor')
            ui=next(a for a in live if a.get_class().get_name()=='CityVRToolController')
            sim=next(o for o in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if o.get_outer()==world)
            ui.return_to_city();ui.set_actor_hidden_in_game(True)
            error=sim.load_city_from_path(str(fixture),False)
            assert error=='',error
            sim.set_simulation_paused(True)
            target=next(a for a in live if a.get_name()==camera_name)
            target.get_component_by_class(unreal.CameraComponent).set_field_of_view(45)
            unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(target,0)
            for cmd in ('DisableAllScreenMessages','t.IdleWhenNotForeground 0','r.SetNearClipPlane 0.01'):
                unreal.SystemLibrary.execute_console_command(world,cmd)
            phase=2;deadline=now+2
        elif phase==2:
            if index==len(views):finish();return
            name,aim,eye,clock,lighting=views[index]
            board.set_lighting_preset(getattr(unreal.CityLightingPreset,lighting.upper()))
            board.set_diagnostic_overlay(unreal.CityDiagnosticOverlay.NONE)
            assert unreal.CityParkAssetLibrary.set_park_capture_clock(board,clock)
            components=board.get_components_by_class(unreal.SkeletalMeshComponent)
            active=[c for c in components if c.is_visible()]
            if name.startswith('Skate-') and lighting=='Day':
                skaters=[c for c in active if 'Skate' in mesh_name(c)]
                riders=sorted(skaters,key=lambda c:int(c.get_name().removeprefix('ParkAnimatedVisitor')))[:2]
                points=[unreal.MathLibrary.inverse_transform_location(board.get_actor_transform(),c.get_world_location()) for c in riders]
                aim=tuple(sum(getattr(p,axis) for p in points)/len(points) for axis in ('x','y','z'))
                aim=(aim[0],aim[1],aim[2]+70)
                eye=(aim[0]-650,aim[1]-1000,aim[2]+700)
            target=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(editor.get_game_world(),unreal.CameraActor) if a.get_name()==camera_name)
            transform=board.get_actor_transform();position=unreal.MathLibrary.transform_location(transform,unreal.Vector(*eye))
            focus=unreal.MathLibrary.transform_location(transform,unreal.Vector(*aim))
            target.set_actor_location(position,False,True);target.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(position,focus),True)
            records.append(dict(view=name,clock=clock,animated=len(active),bones=[list((c.get_socket_location('head').x,c.get_socket_location('head').y,c.get_socket_location('head').z)) for c in active],
                rigs=[dict(name=c.get_name(),mesh=mesh_name(c),position=[c.get_world_location().x,c.get_world_location().y,c.get_world_location().z]) for c in active]))
            if lighting=='Night':assert not active,'Night did not hide rigs'
            phase=3;deadline=now+.5
        elif phase==3:
            name=views[index][0]
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(),f'HighResShot 1600x1200 filename="{(OUT/(name+".png")).as_posix()}"')
            phase=4;deadline=now+1.5
        elif phase==4:
            assert (OUT/(views[index][0]+'.png')).exists()
            index+=1;phase=2;deadline=now+.1
    except Exception:
        failure=traceback.format_exc();(OUT/'Error.txt').write_text(failure);finish()
handle=unreal.register_slate_post_tick_callback(tick)
