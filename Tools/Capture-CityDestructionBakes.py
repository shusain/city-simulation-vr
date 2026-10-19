"""Import static Chaos bakes into study scope and capture a fixed-scale comparison."""
from pathlib import Path
import json,time,hashlib,traceback
import unreal
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D104/DestructionStudy';doc=json.loads((OUT/'Baked.json').read_text());assets=unreal.AssetToolsHelpers.get_asset_tools();jobs=[]
assert Path(unreal.Paths.project_dir()).resolve()==(ROOT/'CitySimVR').resolve()
jobs=[dict(name='Intact',path='/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Home_A'),dict(name='CurrentStandIn',path='/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Home_A_Damaged')]
for e in doc['meshes']:
    path='/Game/CityStudies/D104Destruction/Bakes/'+e['name'];task=unreal.AssetImportTask()
    for k,v in dict(filename=str(ROOT/e['export']),destination_path=path.rsplit('/',1)[0],destination_name=e['name'],automated=True,replace_existing=True,replace_existing_settings=True,save=False).items():task.set_editor_property(k,v)
    task.factory=unreal.FbxFactory();ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_materials=False;ui.import_textures=False
    for k,v in dict(combine_meshes=True,vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,convert_scene_unit=True,convert_scene=True,auto_generate_collision=False,generate_lightmap_u_vs=False,normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():ui.static_mesh_import_data.set_editor_property(k,v)
    task.options=ui;assets.import_asset_tasks([task]);mesh=unreal.load_asset(path);assert mesh
    slots=[str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')]
    for i,slot in enumerate(slots):mesh.set_material(i,unreal.load_asset(doc['material_bindings'][slot]))
    assert mesh.get_num_triangles(0)==e['triangles'];assert unreal.EditorAssetLibrary.save_loaded_asset(mesh);jobs.append(dict(name=e['name'],path=path))
world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def spawn(kind,loc=(0,0,0),rot=None):return actors.spawn_actor_from_class(kind,unreal.Vector(*loc),rot or unreal.Rotator())
building=spawn(unreal.StaticMeshActor)
floor=spawn(unreal.StaticMeshActor,(0,0,-25));floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'));floor.static_mesh_component.set_material(0,unreal.load_asset('/Game/CitySim/Art/Materials/M_CityRoomFloor'));floor.set_actor_scale3d(unreal.Vector(800,800,1))
sun=spawn(unreal.DirectionalLight,(0,0,2000),unreal.Rotator(pitch=-38,yaw=-132,roll=0));sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE);sun.light_component.set_intensity(3)
sky=spawn(unreal.SkyLight);sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE);sky.light_component.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP);sky.light_component.set_cubemap(unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'));sky.light_component.set_intensity(1);sky.light_component.recapture_sky()
a=spawn(unreal.SceneCapture2D,(1600,-2200,1500));a.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(a.get_actor_location(),unreal.Vector(0,0,200)),False);cap=a.get_component_by_class(unreal.SceneCaptureComponent2D)
rt=unreal.RenderingLibrary.create_render_target2d(world,1000,1000,unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
for k,v in dict(texture_target=rt,capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,capture_every_frame=False,capture_on_movement=False,fov_angle=40).items():cap.set_editor_property(k,v)
pp=cap.get_editor_property('post_process_settings')
for k,v in dict(override_auto_exposure_method=True,auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,override_auto_exposure_apply_physical_camera_exposure=True,auto_exposure_apply_physical_camera_exposure=False).items():pp.set_editor_property(k,v)
cap.set_editor_property('post_process_settings',pp);collection=unreal.load_asset('/Game/CitySim/Art/Materials/MPC_CityLighting');unreal.MaterialLibrary.set_scalar_parameter_value(world,collection,'NightGlow',0)
state=dict(i=0,phase=0,next=time.monotonic()+5,rows=[])
def tick(dt):
    try:
        now=time.monotonic()
        if now<state['next']:return
        job=jobs[state['i']]
        if state['phase']==0:
            mesh=unreal.load_asset(job['path']);building.static_mesh_component.set_static_mesh(mesh);state['phase']=1;state['next']=now+1.5;return
        if state['phase']==1:cap.capture_scene();state['phase']=2;state['next']=now+.4;return
        filename='BakeReview_'+job['name']+'.png';unreal.RenderingLibrary.export_render_target(world,rt,str(OUT),filename);state['rows'].append(dict(job,image=filename,sha256=hashlib.sha256((OUT/filename).read_bytes()).hexdigest(),triangles=building.static_mesh_component.static_mesh.get_num_triangles(0)))
        state['i']+=1
        if state['i']==len(jobs):
            (OUT/'ReviewRenders.json').write_text(json.dumps(dict(views=state['rows'],fixed_scale_camera=True,static_bakes=True,production_damage_replaced=False),indent=2)+'\n');unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor();return
        state['phase']=0;state['next']=now+.1
    except Exception:
        (OUT/'BakeCaptureError.txt').write_text(traceback.format_exc());unreal.log_error(traceback.format_exc());unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
