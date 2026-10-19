"""Actual production meshes/materials, unsaved native day/night review scene."""
from pathlib import Path
import unreal,json,time,traceback
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D91/Integration/Unreal';OUT.mkdir(parents=True,exist_ok=True)
(OUT/'Report.json').unlink(missing_ok=True);(OUT/'Error.txt').unlink(missing_ok=True)
assert Path(unreal.Paths.project_dir()).resolve()==(ROOT/'CitySimVR').resolve()
assert unreal.SystemLibrary.get_console_variable_bool_value('r.ForwardShading')
doc=json.loads((ROOT/'ArtSource/Manifests/city_building_adoption.json').read_text());entries={e['asset_id']:e for e in doc['assets']}
ids=['building.residential.a','building.residential.a.damaged','building.residential.high.a','building.shop.a','building.factory.medium.a','building.police.a','building.fire.high.a','building.hospital.medium.a','building.school.secondary.a','building.power.high.a','building.service.high.a','building.power.coal.a','building.park.city']
# Resolve school/coal naming from stable authoring metadata rather than guesses.
for i,id in enumerate(ids):
    if id not in entries:
        token='secondary' if 'secondary' in id else 'coal'
        choices=[e['asset_id'] for e in doc['assets'] if token in e['asset_id']];assert len(choices)==1,choices;ids[i]=choices[0]
views=[dict(asset_id=id,lighting=light,framing='Whole',lod=0) for id in ids for light in ('Day','Night')]
views += [dict(asset_id='building.residential.a',lighting='Day',framing=f,lod=0) for f in ('Entrance','Chimney')]
views += [dict(asset_id='building.park.city',lighting='Day',framing='Whole',lod=1),dict(asset_id='building.residential.a',lighting='Day',framing='SeasonalDoor',lod=0)]
world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def spawn(kind,location=(0,0,0),rotation=(0,0,0)):
    return actors.spawn_actor_from_class(kind,unreal.Vector(*location),unreal.Rotator(*rotation))
building=spawn(unreal.StaticMeshActor);component=building.static_mesh_component
citizen=spawn(unreal.StaticMeshActor);citizen_mesh=unreal.load_asset('/Game/CitySim/Art/PC/Meshes/SM_Citizen_PC_Casual');assert citizen_mesh
citizen.static_mesh_component.set_static_mesh(citizen_mesh);citizen.set_actor_scale3d(unreal.Vector(1.15,1.15,1.15));citizen.set_actor_rotation(unreal.Rotator(pitch=0,yaw=-90,roll=0),False)
wreath=spawn(unreal.StaticMeshActor);wreath.static_mesh_component.set_static_mesh(unreal.load_asset('/Game/CitySim/Art/Meshes/SM_Seasonal_WreathAutumn_A'));wreath.set_actor_scale3d(unreal.Vector(.5,.5,.5));wreath.set_actor_rotation(unreal.Rotator(pitch=0,yaw=180,roll=0),False);wreath.set_actor_location(unreal.Vector(0,478,85),False,True)
ground=spawn(unreal.StaticMeshActor,(0,0,-21));ground.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'));ground.set_actor_scale3d(unreal.Vector(200,200,1))
ground_material=unreal.MaterialLibrary.create_dynamic_material_instance(world,unreal.load_asset('/Game/CitySim/Art/Materials/M_CityColor'));ground_material.set_vector_parameter_value('CityColor',unreal.LinearColor(.16,.18,.22,1));ground.static_mesh_component.set_material(0,ground_material)
sun=spawn(unreal.DirectionalLight,(0,0,2000),(-38,-132,0));sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky=spawn(unreal.SkyLight,(0,0,2000));sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE);sky.light_component.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP);sky.light_component.set_cubemap(unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'));sky.light_component.recapture_sky()
capture=spawn(unreal.SceneCapture2D);cap=capture.get_component_by_class(unreal.SceneCaptureComponent2D)
target=unreal.RenderingLibrary.create_render_target2d(world,1200,900,unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB);target.set_editor_property('target_gamma',2.2)
for k,v in dict(texture_target=target,capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,capture_every_frame=False,capture_on_movement=False,projection_type=unreal.CameraProjectionMode.ORTHOGRAPHIC,always_persist_rendering_state=True).items():cap.set_editor_property(k,v)
pp=cap.get_editor_property('post_process_settings')
for k,v in dict(override_auto_exposure_method=True,auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,override_auto_exposure_bias=True,auto_exposure_bias=0,override_auto_exposure_apply_physical_camera_exposure=True,auto_exposure_apply_physical_camera_exposure=False,override_bloom_intensity=True,bloom_intensity=.15).items():pp.set_editor_property(k,v)
cap.set_editor_property('post_process_settings',pp)
collection=unreal.load_asset('/Game/CitySim/Art/Materials/MPC_CityLighting');assert collection
original_glow=unreal.MaterialLibrary.get_scalar_parameter_value(world,collection,'NightGlow')
start=time.monotonic();deadline=start+8;phase=0;index=0;records=[]
def finish(error=None):
    unreal.MaterialLibrary.set_scalar_parameter_value(world,collection,'NightGlow',original_glow)
    if error:(OUT/'Error.txt').write_text(error)
    else:(OUT/'Report.json').write_text(json.dumps(dict(task='D91',views=records,size=[1200,900],renderer='Windows D3D11 forward',production_assets=True,scene='unsaved blank editor scene; fixed sun/cubemap/manual exposure',physical_device_verified=False),indent=2)+'\n')
    unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def tick(delta):
    global phase,index,deadline,current
    try:
        now=time.monotonic()
        if now<deadline:return
        assert now-start<420,'Native capture timeout'
        if phase==0:
            v=views[index];e=entries[v['asset_id']];mesh=unreal.load_asset(e['mesh_destination']);assert mesh
            component.set_static_mesh(mesh);component.set_forced_lod_model(v['lod']+1)
            night=v['lighting']=='Night';sun.light_component.set_intensity(.08 if night else 3);sky.light_component.set_intensity(.15 if night else 1)
            unreal.MaterialLibrary.set_scalar_parameter_value(world,collection,'NightGlow',1 if night else 0)
            b=mesh.get_bounding_box();size=b.max-b.min;wide=max(size.x,size.y,size.z*1.4)
            aim=unreal.Vector(0,0,size.z*.4);offset=unreal.Vector(wide*1.1,wide*1.6,wide*.95);width=wide*1.65
            if v['framing'] in ('Entrance','SeasonalDoor'):aim,offset,width=unreal.Vector(0,472,110),unreal.Vector(200,1000,180),720
            elif v['framing']=='Chimney':aim,offset,width=unreal.Vector(277.2,-198,480),unreal.Vector(500,800,400),520
            wreath.set_actor_hidden_in_game(v['framing']!='SeasonalDoor');wreath.set_is_temporarily_hidden_in_editor(v['framing']!='SeasonalDoor')
            citizen.set_actor_location(unreal.Vector(-200 if e['asset_id'].startswith('building.residential.a') else -size.x*.25,b.max.y+90,-20-citizen_mesh.get_bounding_box().min.z*1.15),False,True)
            origin,extent=citizen.get_actor_bounds(False);assert abs(extent.z*2-143.175)<.2 and abs(origin.z-extent.z+20)<.2
            location=aim+offset;capture.set_actor_location(location,False,True);capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(location,aim),False);cap.set_editor_property('ortho_width',width)
            current=dict(v,name=e['mesh_destination'].rsplit('/',1)[1]+'-'+v['lighting']+'-'+v['framing']+'-LOD'+str(v['lod']),destination=e['mesh_destination'],triangles=mesh.get_num_triangles(v['lod']),sections=mesh.get_num_sections(v['lod']),materials=[mesh.get_material(i).get_path_name() for i in range(component.get_num_materials())],camera=[location.x,location.y,location.z],ortho_width=width,night_glow=int(night),citizen_height_cm=extent.z*2,citizen_feet_z_cm=origin.z-extent.z)
            phase=1;deadline=now+3
        elif phase==1:cap.capture_scene();phase=2;deadline=now+1
        else:
            filename=current['name']+'.png';unreal.RenderingLibrary.export_render_target(world,target,str(OUT),filename);assert (OUT/filename).is_file()
            records.append(current);index+=1
            if index==len(views):finish()
            else:phase=0;deadline=now+.25
    except Exception:unreal.log_error(traceback.format_exc());finish(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick)
