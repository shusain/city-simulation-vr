"""Review native clustered condition markers in PIE; preserve every local city save."""
from pathlib import Path
import copy,json,time,traceback
import unreal
root=Path(__file__).resolve().parents[1]
out=root/'Artifacts/Preview'
saved=root/'CitySimVR/Saved/SaveGames'
prior={p:p.read_bytes() for p in saved.rglob('*') if p.is_file()}
fixture=out/'ConditionClusters-City.json'
dense=out/'ConditionClusters-Dense-City.json'
doc=json.loads(fixture.read_text(encoding='utf-8-sig'))
doc['buildings']=[];doc['development']['states']=[];doc['population']['citizens']=[];doc['population']['households']=[];doc['health']['patients']=[]
base=json.loads(fixture.read_text(encoding='utf-8-sig'))
for i in range(20):
    entity=str(i+1)
    b=copy.deepcopy(base['buildings'][0]);b.update(entityId=entity,x=4+3*(i%5),y=12+3*(i//5));doc['buildings'].append(b)
    d=copy.deepcopy(base['development']['states'][0]);d['entityId']=entity;doc['development']['states'].append(d)
    h=copy.deepcopy(base['population']['households'][0]);h.update(id=entity,homeEntityId=entity);doc['population']['households'].append(h)
    for j in range(4):
        citizen=str(i*4+j+1)
        c=copy.deepcopy(base['population']['citizens'][0]);c.update(id=citizen,householdId=entity,homeEntityId=entity,currentLocation=entity);doc['population']['citizens'].append(c)
        p=copy.deepcopy(base['health']['patients'][0]);p['citizen']=citizen;doc['health']['patients'].append(p)
doc['nextEntityId']='21';doc['population']['nextCitizenId']='81'
dense.write_text(json.dumps(doc),encoding='utf-8')
labels=('Day','Night','Dense','Hidden')
outputs=[out/f'ConditionClusters-Clusters-{name}.png' for name in labels]
report=out/'ConditionClusters-Clusters-PIE.json';error=report.with_suffix('.error.txt')
for p in outputs+[report,error]:p.unlink(missing_ok=True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
start=time.monotonic();deadline=start+5;phase=0;index=0;samples=[]
def finish():
    level.editor_request_end_play()
    for p in saved.rglob('*'):
        if p.is_file() and p not in prior:p.unlink()
    for p,data in prior.items():p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
    assert {p:p.read_bytes() for p in saved.rglob('*') if p.is_file()}==prior
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()
def tick(delta):
    global phase,deadline,index,board,ui,sim
    try:
        now=time.monotonic()
        if now-start>180:raise RuntimeError('Cluster preview timed out')
        if now<deadline:return
        if phase==0:
            level.editor_request_begin_play();phase=1;deadline=now+8
        elif phase==1:
            world=editor.get_game_world()
            if not world or not level.is_in_play_in_editor():return
            actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
            board=next(a for a in actors if a.get_class().get_name()=='CityBoardActor')
            ui=next(a for a in actors if a.get_class().get_name()=='CityVRToolController')
            sim=next(obj for obj in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if obj.get_outer()==world)
            ui.return_to_city()
            for c in ui.get_components_by_class(unreal.PrimitiveComponent):
                if not c.get_name().startswith('Condition'):c.set_editor_property('render_in_main_pass',False)
            transform=board.get_actor_transform()
            location=unreal.MathLibrary.transform_location(transform,unreal.Vector(8000,3500,12500))
            aim=unreal.MathLibrary.transform_location(transform,unreal.Vector(8000,11200,300))
            pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            pawn.set_actor_location(location-unreal.Vector(0,0,pawn.get_editor_property('base_eye_height')),False,True)
            unreal.GameplayStatics.get_player_controller(world,0).set_control_rotation(unreal.MathLibrary.find_look_at_rotation(location,aim))
            for command in ('FOV 60','DisableAllScreenMessages','t.IdleWhenNotForeground 0','r.EyeAdaptationQuality 0'):
                unreal.SystemLibrary.execute_console_command(world,command)
            phase=2;deadline=now+1
        elif phase==2:
            if index in (0,2):
                source=fixture if index==0 else dense
                (saved/'CitySaveV1.json').write_bytes(source.read_bytes())
                if not sim.load_city():raise RuntimeError('Native cluster fixture failed load: '+source.name)
                sim.set_simulation_paused(True)
            if index==2:
                transform=board.get_actor_transform()
                location=unreal.MathLibrary.transform_location(transform,unreal.Vector(9200,5000,19500))
                aim=unreal.MathLibrary.transform_location(transform,unreal.Vector(9200,14000,300))
                pawn=unreal.GameplayStatics.get_player_pawn(editor.get_game_world(),0)
                pawn.set_actor_location(location-unreal.Vector(0,0,pawn.get_editor_property('base_eye_height')),False,True)
                unreal.GameplayStatics.get_player_controller(editor.get_game_world(),0).set_control_rotation(unreal.MathLibrary.find_look_at_rotation(location,aim))
            board.set_tool_mode(unreal.CityToolMode.INSPECT)
            board.set_lighting_preset(unreal.CityLightingPreset.NIGHT if index==1 else unreal.CityLightingPreset.DAY)
            ui.set_condition_indicators_visible(index!=3)
            phase=3;deadline=now+4
        elif phase==3:
            roots=[c for c in ui.get_components_by_class(unreal.SceneComponent) if c.get_name().startswith('ConditionRoot') and c.is_visible()]
            texts={c.get_name():str(c.get_editor_property('text')) for c in ui.get_components_by_class(unreal.TextRenderComponent) if c.get_name().startswith('ConditionTitle') and c.is_visible()}
            pool=next(c for c in ui.get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name()=='ConditionLeaderInstances')
            sample={'view':labels[index],'popups':len(roots),'leaders':pool.get_instance_count(),'titles':texts}
            if sample['popups'] != (1,1,4,0)[index] or sample['leaders'] != (3,3,20,0)[index]:raise RuntimeError('Wrong clustered counts: '+str(sample))
            if index<2 and list(texts.values())!=['ILL 13']:raise RuntimeError('Missing combined ILL13 caption')
            if index==2 and sum(int(v.split()[1]) for v in texts.values())!=80:raise RuntimeError('Dense illness total lost sources')
            samples.append(sample)
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(),f'HighResShot 1800x1400 filename="{outputs[index].as_posix()}"')
            phase=4;deadline=now+3
        elif phase==4:
            if not outputs[index].exists():raise RuntimeError('Missing cluster screenshot')
            index+=1
            if index<len(labels):phase=2;deadline=now+1
            else:
                report.write_text(json.dumps({'source':'Real native condition pools in PIE; paused isolated fixtures','samples':samples,'localSaveCatalogPreserved':True,'savedMap':False},indent=2),encoding='utf-8');finish()
    except Exception:
        error.write_text(traceback.format_exc(),encoding='utf-8');finish()
handle=unreal.register_slate_post_tick_callback(tick)
