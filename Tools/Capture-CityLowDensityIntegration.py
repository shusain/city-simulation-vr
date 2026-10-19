"""Perspective review of actual integrated assets, emissive windows and steel."""
from pathlib import Path
import json,hashlib,time,traceback
import unreal
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/LowDensity/Integration/Unreal';OUT.mkdir(parents=True,exist_ok=True)
doc=json.loads((ROOT/'ArtSource/Manifests/city_low_density_buildings.json').read_text());entries={e['name']:e for e in doc['assets']}
jobs=[dict(name=e['name'],night=False,detail=False) for e in doc['assets'] if not e['damaged']]
jobs += [dict(name='SM_LowDensity_Home_'+letter,night=True,detail=False) for letter in 'ABCD']
jobs += [dict(name='SM_LowDensity_Factory_D',night=True,detail=False)]
jobs += [dict(name='SM_LowDensity_'+family+'_D_Damaged',night=False,detail=False) for family in ('Home','Shop','Factory')]
jobs += [dict(name='SM_LowDensity_Shop_'+letter,night=False,detail=False,business=business) for letter,business in [('B','bakery'),('C','cafe')]]
jobs += [dict(name='SM_LowDensity_Factory_D',night=False,detail=True)]
jobs += [dict(name='HighRoofReference',night=False,detail=False,path='/Game/CitySim/Art/Meshes/SM_Building_Residential_High_A')]
assert len(jobs)==24
SIGN_ONLY='-D104BottomSigns' in unreal.SystemLibrary.get_command_line()
METAL_ONLY=next((label for label in ('Before','After') if '-D104IndustrialMetal'+label in unreal.SystemLibrary.get_command_line()),None)
if METAL_ONLY:
    OUT=ROOT/'Artifacts/D104/IndustrialMetal'/METAL_ONLY;OUT.mkdir(parents=True,exist_ok=True)
    jobs=[dict(name='SM_LowDensity_Factory_'+letter,night=False,detail=detail) for letter in 'ABCD' for detail in (False,True)]
    jobs += [dict(name='SM_LowDensity_Factory_'+letter+'_Damaged',night=False,detail=False) for letter in ('A','D')]
if SIGN_ONLY:jobs=[dict(name='SM_LowDensity_Shop_C',night=False,detail=False,business=business) for business in ('pharmacy','outfitters')]
CYLINDER_ONLY=next((label for label in ('Before','After') if '-D104Cylinder'+label in unreal.SystemLibrary.get_command_line()),None)
if CYLINDER_ONLY:
    OUT=ROOT/'Artifacts/D104/CylinderShading'/(CYLINDER_ONLY+'ChimneysFinal');OUT.mkdir(parents=True,exist_ok=True)
    jobs=[dict(name=e['name'],night=False,detail=True) for e in doc['assets'] if e['category']=='Factory']
    if CYLINDER_ONLY=='Before':
        # Unsaved comparison imports from the frozen previous FBXs. Never
        # replace current production packages to obtain the before images.
        for job in jobs:
            e=entries[job['name']];task=unreal.AssetImportTask()
            for k,v in dict(filename=str(ROOT/'Artifacts/D104/CylinderShading/Before'/e['export']),destination_path='/Game/D104UnsavedNormalsBefore',destination_name=e['name'],automated=True,replace_existing=False,save=False).items():task.set_editor_property(k,v)
            task.factory=unreal.FbxFactory();ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_materials=False;ui.import_textures=False
            for k,v in dict(combine_meshes=True,vertex_color_import_option=unreal.VertexColorImportOption.REPLACE,convert_scene_unit=True,convert_scene=True,auto_generate_collision=False,generate_lightmap_u_vs=False,normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS).items():ui.static_mesh_import_data.set_editor_property(k,v)
            task.options=ui;unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            job['path']='/Game/D104UnsavedNormalsBefore/'+e['name'];mesh=unreal.load_asset(job['path']);assert mesh
            slots=[str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')]
            for i,slot in enumerate(slots):mesh.set_material(i,unreal.load_asset(e['material_bindings'][slot]))
assert Path(unreal.Paths.project_dir()).resolve()==(ROOT/'CitySimVR').resolve()
for option in doc['materials']:
    m=unreal.load_asset(option['native_material']);assert m
    assert unreal.MaterialEditingLibrary.get_statistics(m).get_editor_property('num_pixel_shader_instructions')>0
world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
unreal.SystemLibrary.execute_console_command(world,'r.ReflectionEnvironmentLightmapMixing 0')
def spawn(kind,location=(0,0,0),rotation=None):return actors.spawn_actor_from_class(kind,unreal.Vector(*location),rotation or unreal.Rotator())
building=spawn(unreal.StaticMeshActor);component=building.static_mesh_component
citizen=spawn(unreal.StaticMeshActor);cm=unreal.load_asset('/Game/CitySim/Art/PC/Meshes/SM_Citizen_PC_Casual');assert cm
citizen.static_mesh_component.set_static_mesh(cm);citizen.set_actor_scale3d(unreal.Vector(1.15,1.15,1.15));citizen.set_actor_rotation(unreal.Rotator(pitch=0,yaw=-90,roll=0),False)
ground=spawn(unreal.StaticMeshActor);ground.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'));ground.set_actor_scale3d(unreal.Vector(800,800,1))
mat=unreal.MaterialLibrary.create_dynamic_material_instance(world,unreal.load_asset('/Game/CitySim/Art/Materials/M_CityColor'));mat.set_vector_parameter_value('CityColor',unreal.LinearColor(.16,.19,.23,1));ground.static_mesh_component.set_material(0,mat)
sun=spawn(unreal.DirectionalLight,(0,0,2000),unreal.Rotator(pitch=-38,yaw=-132,roll=0));sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky=spawn(unreal.SkyLight,(0,0,2000));sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP);sky.light_component.set_cubemap(unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'));sky.light_component.recapture_sky()
probe=spawn(unreal.SphereReflectionCapture,(0,0,400)).get_component_by_class(unreal.SphereReflectionCaptureComponent)
probe.set_editor_property('reflection_source_type',unreal.ReflectionSourceType.SPECIFIED_CUBEMAP);probe.set_editor_property('cubemap',unreal.load_asset('/Game/CitySim/Art/Textures/T_CityRoomReflection'));probe.set_editor_property('influence_radius',100000)
capture=spawn(unreal.SceneCapture2D);cap=capture.get_component_by_class(unreal.SceneCaptureComponent2D)
rt=unreal.RenderingLibrary.create_render_target2d(world,1000,1000,unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB);rt.set_editor_property('target_gamma',2.2)
for k,v in dict(texture_target=rt,capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,capture_every_frame=False,capture_on_movement=False,always_persist_rendering_state=True,projection_type=unreal.CameraProjectionMode.PERSPECTIVE,fov_angle=45).items():cap.set_editor_property(k,v)
pp=cap.get_editor_property('post_process_settings')
for k,v in dict(override_auto_exposure_method=True,auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL,override_auto_exposure_bias=True,auto_exposure_bias=0,override_auto_exposure_apply_physical_camera_exposure=True,auto_exposure_apply_physical_camera_exposure=False,override_bloom_intensity=True,bloom_intensity=.15).items():pp.set_editor_property(k,v)
cap.set_editor_property('post_process_settings',pp)
collection=unreal.load_asset('/Game/CitySim/Art/Materials/MPC_CityLighting');original=unreal.MaterialLibrary.get_scalar_parameter_value(world,collection,'NightGlow')
state=dict(index=0,phase=0,deadline=time.monotonic()+8,start=time.monotonic(),rows=[])
def tick(delta):
    try:
        now=time.monotonic()
        if now<state['deadline']:return
        assert now-state['start']<240
        job=jobs[state['index']]
        if state['phase']==0:
            path=job.get('path',entries.get(job['name'],{}).get('mesh_destination'))
            if job.get('business'):path+='_'+job['business']
            mesh=unreal.load_asset(path);assert mesh;component.set_static_mesh(mesh)
            b=mesh.get_bounding_box();size=b.max-b.min;wide=max(size.x,size.y,size.z*1.25)
            target=unreal.Vector(0,0,size.z*.42);offset=unreal.Vector(wide*1.05,wide*1.45,wide*.9)
            if job['detail']:target=unreal.Vector(0,b.max.y,350);offset=unreal.Vector(700,1400,500)
            if METAL_ONLY and job['detail']:target=unreal.Vector(0,730,230);offset=unreal.Vector(650,1600,500)
            if CYLINDER_ONLY:
                centers=entries[job['name']]['cylinder_shading']['cylinders']
                target=unreal.Vector(sum(r['center_m'][0] for r in centers)/len(centers)*100,-sum(r['center_m'][1] for r in centers)/len(centers)*100,size.z*.72)
                offset=unreal.Vector(800,1100,1100)
            capture.set_actor_location(target+offset,False,False);capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(target+offset,target),False)
            ground.set_actor_location(unreal.Vector(0,0,b.min.z-1.5),False,False)
            citizen.set_actor_hidden_in_game(job['detail']);citizen.set_is_temporarily_hidden_in_editor(job['detail'])
            citizen.set_actor_location(unreal.Vector(-size.x*.3,b.max.y+90,b.min.z-cm.get_bounding_box().min.z*1.15),False,False)
            origin,extent=citizen.get_actor_bounds(False);assert abs(extent.z*2-143.175)<.2
            night=job['night'];sun.light_component.set_intensity(.08 if night else 3);sky.light_component.set_intensity(.15 if night else 1)
            unreal.MaterialLibrary.set_scalar_parameter_value(world,collection,'NightGlow',1 if night else 0)
            state['current']=dict(job,destination=path,triangles=mesh.get_num_triangles(0),sections=mesh.get_num_sections(0),materials=[mesh.get_material(i).get_path_name() for i in range(component.get_num_materials())],camera_cm=list((target+offset).to_tuple()),fov=45,citizen_height_cm=extent.z*2,reflection_cube=probe.get_editor_property('cubemap').get_path_name(),mixing=0)
            state['phase']=1;state['deadline']=now+1.5
        elif state['phase']==1:cap.capture_scene();state['phase']=2;state['deadline']=now+.5
        else:
            filename=job['name']+('_'+job['business'] if job.get('business') else '')+('_Night' if job['night'] else '_Day')+('_Detail' if job['detail'] else '')+'.png'
            unreal.RenderingLibrary.export_render_target(world,rt,str(OUT),filename);assert (OUT/filename).is_file()
            state['rows'].append(dict(state['current'],image=filename,sha256=hashlib.sha256((OUT/filename).read_bytes()).hexdigest()));state['index']+=1
            unreal.log('CITYSIM_D104_INTEGRATED_FRAME '+str(state['index'])+'/'+str(len(jobs))+' '+filename)
            if state['index']==len(jobs):
                if SIGN_ONLY:
                    report=json.loads((OUT/'Report.json').read_text());report['views']=[r for r in report['views'] if r.get('business') not in ('pharmacy','outfitters')]+state['rows'];report['focused_sign_captures']=True
                else:report=dict(task='D104',views=state['rows'],production_assets=True,projection='perspective',source_sha256=doc['source_sha256'],physical_device_verified=False)
                (OUT/'Report.json').write_text(json.dumps(report,indent=2)+'\n')
                unreal.MaterialLibrary.set_scalar_parameter_value(world,collection,'NightGlow',original);unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
            else:state['phase']=0;state['deadline']=now+.1
    except Exception:
        (OUT/'Error.txt').write_text(traceback.format_exc());unreal.log_error(traceback.format_exc());unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
