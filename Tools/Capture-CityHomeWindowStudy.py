"""D91 matched daytime and nighttime captures in a disposable isolated UE scene."""
from pathlib import Path
import unreal, json, time, traceback
ROOT=Path(__file__).resolve().parents[1]
scale='-D91HomeScale' in unreal.SystemLibrary.get_command_line()
details=scale or '-D91HomeDetails' in unreal.SystemLibrary.get_command_line()
OUT=ROOT/('Artifacts/D91/Scale/Unreal' if scale else 'Artifacts/D91/Details/Unreal' if details else 'Artifacts/D91/Unreal');OUT.mkdir(parents=True,exist_ok=True)
assert Path(unreal.Paths.project_dir()).resolve()==(ROOT/'Artifacts/D91/UnrealProject').resolve()
(OUT/'Error.txt').unlink(missing_ok=True)
assert unreal.SystemLibrary.get_console_variable_bool_value('r.ForwardShading'), 'Use the game forward-rendering path'
doc=json.loads((ROOT/('ArtSource/Manifests/city_home_scale_study.json' if scale else 'ArtSource/Manifests/city_home_detail_study.json' if details else 'ArtSource/Manifests/city_home_window_study.json')).read_text())
world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def spawn(kind,location=(0,0,0),rotation=(0,0,0)):
    return actors.spawn_actor_from_class(kind,unreal.Vector(*location),unreal.Rotator(*rotation))
house=spawn(unreal.StaticMeshActor)
component=house.static_mesh_component
citizens=[]
if scale:
    for reference in doc['citizens']:
        actor=spawn(unreal.StaticMeshActor)
        mesh=unreal.load_asset(reference['native_mesh']);assert mesh
        actor.static_mesh_component.set_static_mesh(mesh)
        actor.set_actor_rotation(unreal.Rotator(pitch=0,yaw=-90,roll=0),False)
        citizens.append((actor,mesh,reference))
ground=spawn(unreal.StaticMeshActor,(0,0,-21))
ground.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
ground.set_actor_scale3d(unreal.Vector(80,80,1))
ground_material=unreal.MaterialLibrary.create_dynamic_material_instance(world,unreal.load_asset('/Game/CitySim/Art/Materials/M_CityColor'))
ground_material.set_vector_parameter_value('CityColor',unreal.LinearColor(.16,.18,.22,1))
ground.static_mesh_component.set_material(0,ground_material)
sun=spawn(unreal.DirectionalLight,(0,0,2000),(-38,-132,0))
sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sun.light_component.set_intensity(3)
sky=spawn(unreal.SkyLight,(0,0,2000))
sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
sky.light_component.set_cubemap(unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
sky.light_component.set_intensity(1)
sky.light_component.recapture_sky()
capture=spawn(unreal.SceneCapture2D)
cap=capture.get_component_by_class(unreal.SceneCaptureComponent2D)
target=unreal.RenderingLibrary.create_render_target2d(world,1200,900,unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
target.set_editor_property('target_gamma',2.2)
cap.set_editor_property('texture_target',target)
cap.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
cap.set_editor_property('capture_every_frame',False);cap.set_editor_property('capture_on_movement',False)
cap.set_editor_property('projection_type',unreal.CameraProjectionMode.ORTHOGRAPHIC)
cap.set_editor_property('always_persist_rendering_state',True)
pp=cap.get_editor_property('post_process_settings')
pp.set_editor_property('override_auto_exposure_method',True);pp.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
pp.set_editor_property('override_auto_exposure_bias',True);pp.set_editor_property('auto_exposure_bias',0)
pp.set_editor_property('override_auto_exposure_apply_physical_camera_exposure',True);pp.set_editor_property('auto_exposure_apply_physical_camera_exposure',False)
pp.set_editor_property('override_bloom_intensity',True);pp.set_editor_property('bloom_intensity',.15)
cap.set_editor_property('post_process_settings',pp)
collection=unreal.load_asset('/Game/CitySim/Art/Materials/MPC_CityLighting');assert collection
framings=('Whole','Entrance','Chimney') if scale else ('Whole','Door','Chimney') if details else ('Whole','Window')
views=[(e,light,framing) for e in doc['assets'] for light in ('Day','Night') for framing in framings]
start=time.monotonic();deadline=start+8;phase=0;index=0;records=[]
def tick(delta):
    global phase,index,deadline,current
    try:
        now=time.monotonic()
        if now<deadline:return
        if now-start>240:raise RuntimeError('D91 capture timed out')
        if phase==0:
            e,light,framing=views[index]
            mesh=unreal.load_asset('/Game/CitySim/Art/Prototypes/D91/Meshes/'+e['name']);assert mesh
            component.set_static_mesh(mesh)
            night=light=='Night'
            sun.light_component.set_intensity(.08 if night else 3)
            sky.light_component.set_intensity(.15 if night else 1)
            unreal.MaterialLibrary.set_scalar_parameter_value(world,collection,'NightGlow',1 if night else 0)
            # FBX handedness conversion maps Blender's -Y front to Unreal +Y.
            aim=unreal.Vector(0,50,380) if framing=='Whole' else unreal.Vector(365,619,340)
            offset=unreal.Vector(2300,3200,1900) if framing=='Whole' else unreal.Vector(200,700,160)
            width=2200 if framing=='Whole' else 450
            if framing=='Door':aim,offset,width=unreal.Vector(0,650,210),unreal.Vector(200,800,100),560
            elif framing=='Chimney':aim,offset,width=unreal.Vector(369.6,-264,800),unreal.Vector(500,800,400),470
            citizen_records=[]
            if scale:
                refined=e['name'].endswith('_Refined');front=-4.62 if refined else -6.16
                if framing=='Whole':aim,offset,width=unreal.Vector(0,50,300),unreal.Vector(2300,3200,1900),2200
                elif framing=='Entrance':aim,offset,width=unreal.Vector(0,-front*100,165),unreal.Vector(200,1000,200),870
                else:aim,offset,width=unreal.Vector(277.2,-198,480) if refined else unreal.Vector(369.6,-264,800),unreal.Vector(500,800,400),520
                for actor,citizen_mesh,reference in citizens:
                    factor=reference['scale_after' if refined else 'scale_before'];b=citizen_mesh.get_bounding_box()
                    height=(b.max.z-b.min.z)*factor
                    assert abs(height-reference['height_after_m' if refined else 'height_before_m']*100)<.2
                    actor.set_actor_scale3d(unreal.Vector(factor,factor,factor))
                    actor.set_actor_location(unreal.Vector(reference['x']*100,-(front-.9)*100,-20-b.min.z*factor),False,True)
                    origin,extent=actor.get_actor_bounds(False)
                    assert abs(extent.z*2-height)<.2 and abs(origin.z-extent.z+20)<.2, 'Reference must stand upright with grounded feet'
                    citizen_records.append(dict(kind=reference['kind'],mesh=citizen_mesh.get_path_name(),scale=factor,height_cm=height,world_height_cm=extent.z*2,feet_z_cm=origin.z-extent.z))
            location=aim+offset
            capture.set_actor_location(location,False,True)
            capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(location,aim),False)
            cap.set_editor_property('ortho_width',width)
            current=dict(view=e['name']+'-'+light+'-'+framing,mesh=mesh.get_path_name(),lighting=light,framing=framing,
                triangles=mesh.get_num_triangles(0),sections=mesh.get_num_sections(0),camera=[location.x,location.y,location.z],
                night_glow=unreal.MaterialLibrary.get_scalar_parameter_value(world,collection,'NightGlow'),
                materials=[component.get_material(i).get_path_name() for i in range(component.get_num_materials())],citizens=citizen_records)
            phase=1;deadline=now+4
        elif phase==1:
            cap.capture_scene();phase=2;deadline=now+1
        elif phase==2:
            filename=current['view']+'.png'
            unreal.RenderingLibrary.export_render_target(world,target,str(OUT),filename)
            assert (OUT/filename).is_file(), 'Missing rendered capture'
            records.append(current);index+=1
            if index==len(views):
                unreal.MaterialLibrary.set_scalar_parameter_value(world,collection,'NightGlow',0)
                (OUT/'Report.json').write_text(json.dumps(dict(task='D91',views=records,renderer='Windows D3D11 forward',size=[1200,900],scene='isolated unsaved blank editor map; copied game surfaces; fixed cubemap/sun, neutral floor and exposure',production_changed=False,physical_device_verified=False),indent=2)+'\n')
                unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
            else:phase=0;deadline=now+.25
    except Exception:
        (OUT/'Error.txt').write_text(traceback.format_exc())
        unreal.log_error(traceback.format_exc());unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
