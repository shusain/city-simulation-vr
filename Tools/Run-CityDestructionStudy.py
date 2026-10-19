"""Import closed fragments, simulate with Unreal Chaos, record bake transforms."""
from pathlib import Path
import json,time,traceback,random
import unreal
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D104/DestructionStudy'
assert Path(unreal.Paths.project_dir()).resolve()==(ROOT/'CitySimVR').resolve()
def main():
    doc=json.loads((OUT/'Fragments.json').read_text());assets=unreal.AssetToolsHelpers.get_asset_tools();meshes={};rows=[]
    subsystem=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for e in doc['fragments']:
        task=unreal.AssetImportTask();name=e['name'];path='/Game/CityStudies/D104Destruction/Fragments/'+name
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            # These generated study packages are owned by this script. Recreate
            # to avoid reimport preserving an older user-edited collision hull.
            assert unreal.EditorAssetLibrary.delete_asset(path)
        for k,v in dict(filename=str(ROOT/e['export']),destination_path=path.rsplit('/',1)[0],destination_name=name,automated=True,replace_existing=True,replace_existing_settings=True,save=False).items():task.set_editor_property(k,v)
        task.factory=unreal.FbxFactory();ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_materials=False;ui.import_textures=False
        for k,v in dict(combine_meshes=True,vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,convert_scene_unit=True,convert_scene=True,auto_generate_collision=True,generate_lightmap_u_vs=False,normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():ui.static_mesh_import_data.set_editor_property(k,v)
        task.options=ui;assets.import_asset_tasks([task]);mesh=unreal.load_asset(path);assert mesh
        slots=[str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')]
        for i,slot in enumerate(slots):mesh.set_material(i,unreal.load_asset(doc['materials'][slot]))
        collisions=subsystem.get_simple_collision_count(mesh)+subsystem.get_convex_collision_count(mesh)
        if not collisions:
            assert subsystem.set_convex_decomposition_collisions(mesh,1,16,100000)
            collisions=subsystem.get_simple_collision_count(mesh)+subsystem.get_convex_collision_count(mesh)
        assert collisions>0,(name,'Simple collision required for Chaos')
        assert unreal.EditorAssetLibrary.save_loaded_asset(mesh);meshes[name]=mesh;rows.append(dict(name=name,asset=path,triangles=mesh.get_num_triangles(0),simple_collisions=collisions))
    (OUT/'Import.json').write_text(json.dumps(dict(meshes=rows,scope='/Game/CityStudies/D104Destruction',production_catalog_unchanged=True),indent=2)+'\n')
    world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    settings=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.WorldSettings)
    assert settings;settings[0].set_editor_property('default_game_mode',unreal.GameModeBase)
    def spawn(kind,location=(0,0,0),rotation=None):return actors.spawn_actor_from_class(kind,unreal.Vector(*location),rotation or unreal.Rotator())
    floor=spawn(unreal.StaticMeshActor,(0,0,-10));floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));floor.set_actor_scale3d(unreal.Vector(200,200,.2));floor.static_mesh_component.set_collision_profile_name('BlockAll')
    sun=spawn(unreal.DirectionalLight,(0,0,2000),unreal.Rotator(pitch=-48,yaw=-120,roll=0));sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE);sun.light_component.set_intensity(3)
    sky=spawn(unreal.SkyLight,(0,0,2000));sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE);sky.light_component.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP);sky.light_component.set_cubemap(unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'));sky.light_component.recapture_sky()
    for e in doc['fragments']:
        a=spawn(unreal.StaticMeshActor,(e['center_m'][0]*100,-e['center_m'][1]*100,e['center_m'][2]*100));a.set_actor_label('D104_'+e['name']);c=a.static_mesh_component;c.set_static_mesh(meshes[e['name']]);c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_collision_profile_name('BlockAll');c.set_linear_damping(.7);c.set_angular_damping(.7)
    camera=spawn(unreal.SceneCapture2D,(1550,-1900,1350));camera.set_actor_label('D104_StudyCamera');camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),unreal.Vector(0,0,180)),False);cap=camera.get_component_by_class(unreal.SceneCaptureComponent2D)
    rt=unreal.RenderingLibrary.create_render_target2d(world,1000,1000,unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    for k,v in dict(texture_target=rt,capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,capture_every_frame=False,capture_on_movement=False,fov_angle=42).items():cap.set_editor_property(k,v)
    pp=cap.get_editor_property('post_process_settings')
    for k,v in dict(override_auto_exposure_method=True,auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,override_auto_exposure_apply_physical_camera_exposure=True,auto_exposure_apply_physical_camera_exposure=False).items():pp.set_editor_property(k,v)
    cap.set_editor_property('post_process_settings',pp)
    state=dict(phase=0,deadline=time.monotonic()+2,frames=[],snapshots=[],pieces={},next_sample=0);specs={e['name']:e for e in doc['fragments']};rng=random.Random(104106)
    def record(world):
        rows=[]
        for name,a in state['pieces'].items():
            t=a.get_actor_transform();rows.append(dict(name=name,translation_cm=list(t.translation.to_tuple()),rotation_xyzw=[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],scale=list(t.scale3d.to_tuple()),awake=a.static_mesh_component.is_any_rigid_body_awake()))
        return rows
    def tick(dt):
        try:
            now=time.monotonic()
            if now<state['deadline']:return
            game=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if state['phase']==0:
                state['phase']=1;state['deadline']=now+2;unreal.EditorLevelLibrary.editor_play_simulate();return
            if state['phase']==1:
                if not game:state['deadline']=now+.1;return
                all_actors=unreal.GameplayStatics.get_all_actors_of_class(game,unreal.StaticMeshActor)
                state['pieces']={a.get_actor_label().removeprefix('D104_'):a for a in all_actors if a.get_actor_label().startswith('D104_Fragment_')}
                assert len(state['pieces'])==len(specs)
                state['capture']=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(game,unreal.SceneCapture2D) if a.get_actor_label()=='D104_StudyCamera').get_component_by_class(unreal.SceneCaptureComponent2D)
                state['capture'].capture_scene();state['start']=unreal.GameplayStatics.get_time_seconds(game);state['snapshots'].append(dict(seconds=0,transforms=record(game)));state['phase']=2;state['deadline']=now+.5;return
            if state['phase']==2:
                unreal.RenderingLibrary.export_render_target(game,state['capture'].texture_target,str(OUT),'Assembly.png')
                for name,a in state['pieces'].items():
                    e=specs[name]
                    if e['static']:continue
                    c=a.static_mesh_component;c.set_simulate_physics(True);c.set_mass_override_in_kg('',max(2,e['volume_m3']*(1200 if e['wall'] else 450)),True)
                    x,y,_=e['center_m'];length=max(.1,(x*x+y*y)**.5);speed=-35 if e['wall'] else 0
                    c.set_physics_linear_velocity(unreal.Vector(x/length*speed,-y/length*speed,0),False)
                    c.set_physics_angular_velocity_in_degrees(unreal.Vector(rng.uniform(-12,12),rng.uniform(-12,12),rng.uniform(-8,8)),False)
                state['start']=unreal.GameplayStatics.get_time_seconds(game);state['phase']=3;state['deadline']=now+.05;return
            elapsed=unreal.GameplayStatics.get_time_seconds(game)-state['start']
            if elapsed>=state['next_sample']:
                state['frames'].append(dict(seconds=elapsed,transforms=record(game)));state['next_sample']=elapsed+.1
            targets=[1.5,4,20]
            if state['phase']==3 and elapsed>=targets[len(state['snapshots'])-1]:
                state['snapshots'].append(dict(seconds=elapsed,transforms=record(game)));state['capture'].capture_scene();state['phase']=4;state['deadline']=now+.4;return
            if state['phase']==4:
                name=['','EarlyCollapse.png','Collapsed.png','Settled.png'][len(state['snapshots'])-1]
                unreal.RenderingLibrary.export_render_target(game,state['capture'].texture_target,str(OUT),name)
                if len(state['snapshots'])==4:
                    result=dict(physics_engine='Unreal Chaos rigid bodies',fracture='Pre-authored closed masonry/roof pieces with source details',runtime_geometry_collection=False,snapshots=state['snapshots'],frames=state['frames'],seed=104106,production_damage_replaced=False)
                    (OUT/'Simulation.json').write_text(json.dumps(result,indent=2)+'\n');unreal.EditorLevelLibrary.editor_end_play();unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor();return
                state['phase']=3
            state['deadline']=now+.05
        except Exception:
            (OUT/'Error.txt').write_text(traceback.format_exc());unreal.log_error(traceback.format_exc())
            if game:unreal.EditorLevelLibrary.editor_end_play()
            unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
    handle=unreal.register_slate_post_tick_callback(tick)

try:main()
except Exception:
    (OUT/'BootstrapError.txt').write_text(traceback.format_exc());unreal.log_error(traceback.format_exc());unreal.SystemLibrary.quit_editor()
