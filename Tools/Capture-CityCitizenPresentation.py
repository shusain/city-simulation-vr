"""Review real citizen pools in an isolated copied city, without source/map writes."""
from pathlib import Path
import hashlib,json,time,traceback,shutil
import unreal

root=Path(__file__).resolve().parents[1]
legacy='-CityLegacyCitizens' in unreal.SystemLibrary.get_command_line()
out=root/'Artifacts/D79-MetalD71'/('CitizenLegacyPreview' if legacy else 'CitizenPreview')
out.mkdir(parents=True,exist_ok=True)
source=root/'Artifacts/DesktopUser/Saved/SaveGames/Cities/31d560b14dac8be5625330a919728387.json'
source_hash=hashlib.sha256(source.read_bytes()).hexdigest()
fixture=out/'InputCity.json';shutil.copy2(source,fixture)
map_path=root/'CitySimVR/Content/CitySim/Maps/CitySandbox.umap';map_hash=hashlib.sha256(map_path.read_bytes()).hexdigest()
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actor_subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor_camera=actor_subsystem.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(),unreal.Rotator(),transient=False)
editor_camera.set_actor_label('D71CaptureCamera')
camera_name=editor_camera.get_name()
names=('CitizenInstances','OfficeCitizenInstances','WorkerCitizenInstances')
views=[(style+'-'+angle,style,angle,'Day') for style in ('Casual','Office','Worker') for angle in ('Front','Side')]
views+=[('Ordinary-Day','Casual','Ordinary','Day'),('Casual-Night','Casual','Front','Night')]
if legacy:
    views=[v for v in views if v[1]=='Casual']
phase=0;index=0;deadline=time.monotonic()+3;start=time.monotonic();samples=[];failure=None;motion_before=None;motion_after=None
(out/'Report.json').unlink(missing_ok=True)
(out/'Error.txt').unlink(missing_ok=True)

def pools():
    return {c.get_name():c for c in board.get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name() in names}

def snapshot():
    return {name:dict(count=p.get_instance_count(),mesh=p.get_editor_property('static_mesh').get_path_name(),
        positions=[[round(v,6) for v in (t.translation.x,t.translation.y,t.translation.z)]
                   for t in [p.get_instance_transform(i,False) for i in range(p.get_instance_count())]]) for name,p in pools().items()}

def finish(error=None):
    global phase,deadline,failure
    failure=error;level.editor_request_end_play();phase=90;deadline=time.monotonic()+3

def tick(delta):
    global phase,deadline,index,board,sim,sample,motion_before,motion_after,camera
    try:
        now=time.monotonic()
        if now<deadline:return
        if phase!=90 and now-start>180:raise RuntimeError('Citizen review timed out')
        if phase==90:
            if level.is_in_play_in_editor():level.editor_request_end_play();deadline=now+1;return
            actor_subsystem.destroy_actor(editor_camera)
            assert hashlib.sha256(source.read_bytes()).hexdigest()==source_hash
            assert hashlib.sha256(map_path.read_bytes()).hexdigest()==map_hash
            (out/'Report.json').write_text(json.dumps(dict(legacy=legacy,samples=samples,error=failure,
                source_city_sha256=source_hash,source_city_unchanged=True,map_unchanged=True,
                motion_before=motion_before,motion_after=motion_after),indent=2)+'\n')
            unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
        elif phase==0:
            level.editor_request_begin_play();phase=1;deadline=now+8
        elif phase==1:
            world=editor.get_game_world()
            if not world:return
            actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
            board=next(a for a in actors if a.get_class().get_name()=='CityBoardActor')
            ui=next(a for a in actors if a.get_class().get_name()=='CityVRToolController')
            sim=next(o for o in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if o.get_outer()==world)
            ui.return_to_city();ui.set_actor_hidden_in_game(True)
            error=sim.load_city_from_path(str(fixture),False)
            if error is None or error:raise RuntimeError('Copied city failed to load: '+str(error))
            sim.set_simulation_paused(True)
            camera=next(a for a in actors if a.get_name()==camera_name)
            camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(45)
            unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(camera,0)
            board.set_tool_mode(unreal.CityToolMode.INSPECT);board.set_diagnostic_overlay(unreal.CityDiagnosticOverlay.NONE)
            for cmd in ('DisableAllScreenMessages','t.IdleWhenNotForeground 0','r.SetNearClipPlane 0.01'):
                unreal.SystemLibrary.execute_console_command(world,cmd)
            phase=2;deadline=now+3
        elif phase==2:
            name,style,angle,lighting=views[index]
            board.set_lighting_preset(unreal.CityLightingPreset.NIGHT if lighting=='Night' else unreal.CityLightingPreset.DAY)
            phase=3;deadline=now+2
        elif phase==3:
            name,style,angle,lighting=views[index]
            data=snapshot();active=sum(d['count'] for d in data.values())
            assert active>0 and active<=64
            if legacy:assert data['CitizenInstances']['count']>0 and 'SM_Citizen_A' in data['CitizenInstances']['mesh']
            else:assert all(d['count']>0 and '/Art/PC/' in d['mesh'] for d in data.values())
            pool=pools()[names[0 if legacy else ('Casual','Office','Worker').index(style)]]
            pose=pool.get_instance_transform(0,True)
            mesh=pool.get_editor_property('static_mesh');box=mesh.get_bounding_box()
            center=unreal.MathLibrary.transform_location(pose,(box.min+box.max)*.5)
            height=(box.max.z-box.min.z)*pose.scale3d.z
            if angle=='Ordinary':location=center+unreal.Vector(12,-90,65)
            else:
                offset=unreal.Vector(height*3.5,0,height*.50) if angle=='Front' else unreal.Vector(0,-height*3.5,height*.50)
                location=center+unreal.MathLibrary.transform_direction(pose,offset)
            if legacy:
                refined=json.loads((root/'Artifacts/D79-MetalD71/CitizenPreview/Report.json').read_text())
                reference=next(s for s in refined['samples'] if s['view']==name)
                location=unreal.Vector(*reference['camera']);center=unreal.Vector(*reference['target'])
            camera.set_actor_location(location,False,True)
            camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(location,center),True)
            sample=dict(view=name,pools=data,world_height_cm=height,camera=[location.x,location.y,location.z],target=[center.x,center.y,center.z])
            phase=4;deadline=now+2
        elif phase==4:
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(),f'HighResShot 1800x1200 filename="{(out/(views[index][0]+".png")).as_posix()}"')
            phase=5;deadline=now+2
        elif phase==5:
            assert (out/(views[index][0]+'.png')).is_file()
            samples.append(sample);index+=1
            if index<len(views):phase=2;deadline=now+.5
            else:
                motion_before=snapshot();sim.set_simulation_paused(False);phase=6;deadline=now+8
        elif phase==6:
            sim.set_simulation_paused(True);motion_after=snapshot()
            assert motion_before!=motion_after,'Real traffic pools did not advance'
            finish()
    except Exception:
        if phase==90:
            (out/'Error.txt').write_text(traceback.format_exc());unreal.SystemLibrary.quit_editor()
        else:finish(traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick)
