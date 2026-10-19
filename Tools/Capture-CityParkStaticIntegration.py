"""Native D83 imported static variants/paired LODs; temporary unsaved PIE gallery."""
from pathlib import Path
import unreal,json,time,hashlib,traceback
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Artifacts/D83/Integration/StaticPreview';OUT.mkdir(parents=True,exist_ok=True)
for name in ('Report.json','Error.txt'):(OUT/name).unlink(missing_ok=True)
document=json.loads((ROOT/'ArtSource/Manifests/city_park_integration.json').read_text())
views=[(e,lod) for e in document['statics'] for lod in (0,1)]
map_path=ROOT/'CitySimVR/Content/CitySim/Maps/CitySandbox.umap'
map_hash=hashlib.sha256(map_path.read_bytes()).hexdigest()
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
camera=actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(),unreal.Rotator(),transient=False)
camera_name=camera.get_name();phase=0;index=0;deadline=time.monotonic()+2;started=time.monotonic()
gallery_source=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(),unreal.Rotator(),transient=False)
gallery_source.get_component_by_class(unreal.StaticMeshComponent).set_mobility(unreal.ComponentMobility.MOVABLE)
gallery_name=gallery_source.get_name()
board=world=target=gallery=None;records=[];failure=None
def finish():
    level.editor_request_end_play();unreal.unregister_slate_post_tick_callback(handle)
    assert hashlib.sha256(map_path.read_bytes()).hexdigest()==map_hash
    (OUT/'Report.json').write_text(json.dumps(dict(views=records,error=failure,map_unchanged=True,
        source='native imported static assets; isolated unsaved PIE gallery; forced LOD0/LOD1'),indent=2)+'\n')
    unreal.SystemLibrary.quit_editor()
def tick(delta):
    global phase,index,deadline,board,world,target,gallery,failure
    try:
        now=time.monotonic()
        if now-started>250:raise RuntimeError('D83 static capture timeout')
        if now<deadline:return
        if phase==0:
            level.editor_request_begin_play();phase=1;deadline=now+6
        elif phase==1:
            world=editor.get_game_world()
            if not world:return
            live=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
            board=next(a for a in live if a.get_class().get_name()=='CityBoardActor')
            sim=next(o for o in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if o.get_outer()==world)
            sim.set_simulation_paused(True)
            ui=next(a for a in live if a.get_class().get_name()=='CityVRToolController')
            ui.return_to_city();ui.set_actor_hidden_in_game(True)
            board.set_lighting_preset(unreal.CityLightingPreset.DAY)
            target=next(a for a in live if a.get_name()==camera_name)
            target.get_component_by_class(unreal.CameraComponent).set_field_of_view(45)
            unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(target,0)
            for cmd in ('DisableAllScreenMessages','t.IdleWhenNotForeground 0','r.SetNearClipPlane 0.01'):
                unreal.SystemLibrary.execute_console_command(world,cmd)
            # Elevated above terrain/scenery; preserve source geometry and material lighting.
            transform=unreal.Transform(
                location=unreal.MathLibrary.transform_location(board.get_actor_transform(),unreal.Vector(32000,32000,6000)),
                rotation=board.get_actor_rotation(),scale=board.get_actor_scale3d())
            gallery=next(a for a in live if a.get_name()==gallery_name)
            gallery.set_actor_transform(transform,False,True)
            phase=2;deadline=now+2
        elif phase==2:
            if index==len(views):finish();return
            e,lod=views[index];mesh=unreal.load_asset(e['mesh_destination']);assert mesh
            component=gallery.get_component_by_class(unreal.StaticMeshComponent)
            component.set_static_mesh(mesh);component.set_forced_lod_model(lod+1)
            low,high=e['lods'][0]['bounds_m'];width=max(high[0]-low[0],high[1]-low[1])*100
            height=high[2]*100;distance=max(width*1.5,height*2.8)
            aim=(32000,32000,6000+height*.4);eye=(aim[0]-distance*.7,aim[1]-distance,aim[2]+distance*.65)
            transform=board.get_actor_transform();position=unreal.MathLibrary.transform_location(transform,unreal.Vector(*eye))
            focus=unreal.MathLibrary.transform_location(transform,unreal.Vector(*aim))
            target.set_actor_location(position,False,True)
            target.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(position,focus),True)
            name=e['mesh_destination'].split('/')[-1]+f'-LOD{lod}'
            records.append(dict(view=name,asset=e['mesh_destination'],lod=lod,triangles=e['lods'][lod]['triangles']))
            # Let the render thread receive the new mesh/material/LOD before readback.
            phase=3;deadline=now+1
        elif phase==3:
            name=records[-1]['view']
            unreal.SystemLibrary.execute_console_command(world,f'HighResShot 1200x1000 filename="{(OUT/(name+".png")).as_posix()}"')
            phase=4;deadline=now+1.1
        elif phase==4:
            assert (OUT/(records[-1]['view']+'.png')).exists()
            index+=1;phase=2;deadline=now+.1
    except Exception:
        failure=traceback.format_exc();(OUT/'Error.txt').write_text(failure);finish()
handle=unreal.register_slate_post_tick_callback(tick)
