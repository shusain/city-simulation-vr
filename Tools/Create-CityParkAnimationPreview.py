"""D83 playable Blender sports scenes with bone-driven citizens and props.

Reads accepted sources; writes only CityParkAnimationPreview.blend and D83 QA.
Run with Blender background; -- --validate-only checks the saved scene instead.
"""
from pathlib import Path
import bpy, bmesh, json, math, hashlib, sys
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Artifacts/ArtPreview/D83'
SOURCE=ROOT/'ArtSource/Blender/CityParkAnimationPreview.blend'
REPORT=ROOT/'Artifacts/D83/sports-preview.json'
sys.path.insert(0,str(ROOT/'Tools'))
import CitySkateKeyframes as skate_keys
manifest=json.loads((ROOT/'ArtSource/Manifests/city_park_refinement.json').read_text())
BOARD_SCALE=manifest['skateboard_preview']['scale']
DECK=manifest['skateboard_preview']['deck_top_m']
CLEARANCE=manifest['skateboard_preview']['wheel_clearance_m']
def ease(value):
    value=max(0,min(1,value));return value*value*(3-2*value)
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def evaluated_points(mesh):
    obj=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
    data=obj.to_mesh();points=[obj.matrix_world@v.co for v in data.vertices];obj.to_mesh_clear()
    return points
def validate(report):
    assert digest(SOURCE)==report['source_sha256']
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
    checks=[]
    for record in report['scenes']:
        scene=bpy.data.scenes[record['scene']];bpy.context.window.scene=scene
        hero=bpy.data.objects[record['citizen']]
        rig=hero.parent
        first=None;max_contact_error=0;previous_x=None;subframe_samples=0
        for frame in range(1,record['period']+2):
            scene.frame_set(frame)
            points=evaluated_points(hero)
            assert all(math.isfinite(c) for v in points for c in v)
            for obj in scene.objects:
                assert all(math.isfinite(c) for row in obj.matrix_world for c in row)
            if first is None:first=points
            if frame==record['period']+1 and record.get('playback','loop')=='loop':
                assert max((a-b).length for a,b in zip(first,points))<1e-4,(record['scene'],'loop seam')
            if record['sport']=='Skate':
                carrier=bpy.data.objects[record['carrier']]
                board=bpy.data.objects[record['props'][0]]
                assert all(abs(s-BOARD_SCALE)<1e-6 for s in board.scale)
                assert abs(record['board_length_m']/manifest['skateboard_preview']['citizen_height_m']-.5)<.025
                local=[carrier.matrix_world.inverted()@v for v in points]
                t=(frame-1)/record['period']
                if record['clip']=='SkateKickflip':
                    u,in_air,_,_=skate_keys.flight(record['clip'],t)
                    foot=min(v.z for v in local)
                    contact=abs(foot-DECK) if not in_air else 0
                    if in_air and .15<u<.85:
                        board_points=[carrier.matrix_world.inverted()@(board.matrix_world@v.co) for v in board.data.vertices]
                        assert foot>max(v.z for v in board_points)+.01,'Flipping board intersects feet'
                    assert abs(board.rotation_euler.x+math.tau*ease(u))<1e-5
                else:contact=abs(min(v.z for v in local)-DECK)
                assert contact<1e-4,(frame,'feet penetrate/floating above deck',contact)
                max_contact_error=max(max_contact_error,contact)
                x=carrier.location.x
                if record.get('playback')=='one_shot':
                    if previous_x is not None:
                        assert abs((x-previous_x)*24-record['speed_m_s'])<1e-4,'Travel reverses/slows during rotation'
                    previous_x=x
                if record['clip'] in ('SkateKickflip','Skate180'):
                    _,in_air,start,end=skate_keys.flight(record['clip'],t)
                    duration=(end-start)*record['period']/24
                    elapsed=max(0,min(duration,(t-start)*record['period']/24))
                    expected=.36+CLEARANCE+.5*record['gravity_m_s2']*elapsed*(duration-elapsed)
                    assert abs(carrier.location.z-expected)<1e-5,'Jump arc does not match its flight time'
                # Both soles must fit the deck at approach/landing, not merely one
                # lowest vertex. Temporary IK is gone from the saved keyframe rig.
                assert not any(p.constraints for p in rig.pose.bones),'Unbaked skate constraints'
                landed=record['clip'] not in ('SkateKickflip','Skate180') or not skate_keys.flight(record['clip'],t)[1]
                if landed:
                    for side,indices in record['sole_indices'].items():
                        sole=[local[i] for i in indices]
                        assert max(abs(v.z-DECK) for v in sole)<1e-4,(record['scene'],side,'sole not planted')
                        assert all(abs(v.x)<=.55*BOARD_SCALE+1e-4 and abs(v.y)<=.175*BOARD_SCALE+1e-4 for v in sole), (record['scene'],frame,side,'Foot outside deck',[(min(v[j] for v in sole),max(v[j] for v in sole)) for j in range(3)])
                if record['clip'] in ('SkateCarve','SkatePush'):
                    assert abs(carrier.location.z-(.36+.12*max(0,x-8)**2+CLEARANCE))<1e-5
                if record['clip']=='Skate180':
                    assert carrier.location.z>=.36+CLEARANCE-1e-5
                    if t>=record['flight_end']:assert abs(carrier.rotation_euler.z-math.pi)<1e-5
                if record['clip']=='SkateLipStall' and .4<=t<=.65:
                    wheel=board.matrix_world@Vector((-.35,0,-.13))
                    assert abs(wheel.x-13)<1e-5 and abs(wheel.z-3.36)<1e-5,'Rear truck misses ramp lip'
            elif record['clip']=='BaseballPitch' and frame<=record['release_frame']:
                hand=rig.matrix_world@rig.pose.bones['hand.R'].tail
                assert (bpy.data.objects[record['ball']].location-hand).length<1e-5
            elif record['clip']=='BaseballSwing':
                bat=bpy.data.objects[record['bat']]
                hand=rig.matrix_world@rig.pose.bones['hand.R'].tail
                grip=bat.matrix_world@Vector((0,0,-.425))
                assert (hand-grip).length<1e-4,'Bat detached from grip'
                if frame==record['impact_frame']:
                    barrel=bat.matrix_world@Vector((0,0,.275))
                    assert (barrel-bpy.data.objects[record['ball']].location).length<1e-4,'Bat misses the contact sample'
            elif record['clip']=='SoccerKick' and frame==record['impact_frame']:
                foot=rig.matrix_world@rig.pose.bones['foot.R'].tail
                assert (foot-bpy.data.objects[record['ball']].location).length<.14,'Soccer foot misses ball'
        # Props also close their loop: hidden-ball recycling stays deterministic.
        scene.frame_set(1)
        props={name:bpy.data.objects[name].matrix_world.copy() for name in record['props']}
        scene.frame_set(record['period']+1)
        if record.get('playback','loop')=='loop':
            assert all(max(abs(a-b) for ra,rb in zip(matrix,bpy.data.objects[name].matrix_world) for a,b in zip(ra,rb))<1e-4 for name,matrix in props.items())
        if record.get('playback')=='one_shot':
            carrier=bpy.data.objects[record['carrier']]
            for frame in range(1,record['period']):
                scene.frame_set(frame);a=carrier.location.x
                scene.frame_set(frame,subframe=.5);b=carrier.location.x
                assert abs((b-a)*48-record['speed_m_s'])<2e-4,'Between-key momentum overshoot'
                subframe_samples+=1
                t=(frame-.5)/record['period']
                if record['clip'] not in ('SkateKickflip','Skate180') or not skate_keys.flight(record['clip'],t)[1]:
                    local=[carrier.matrix_world.inverted()@v for v in evaluated_points(hero)]
                    for indices in record['sole_indices'].values():
                        sole=[local[i] for i in indices]
                        assert max(abs(v.z-DECK) for v in sole)<.002,'Between-key sole contact drift'
                        assert all(abs(v.x)<=.55*BOARD_SCALE+.001 and abs(v.y)<=.175*BOARD_SCALE+.001 for v in sole),'Between-key foot outside deck'
        checks.append(dict(scene=record['scene'],samples=record['period']+1,
                           subframe_samples=subframe_samples,
                           playback=record.get('playback','loop'),forward_motion_pass=record.get('playback')=='one_shot',
                           maximum_deck_contact_error_m=max_contact_error,pass_checks=True))
    for path,expected in report['protected_inputs'].items():assert digest(ROOT/path)==expected,path
    (ROOT/'Artifacts/D83/sports-validation.json').write_text(json.dumps(dict(scenes=checks,source_hash_pass=True,unreal_launched=False),indent=2)+'\n')
    print('CITYSIM_D83_SPORTS_VALIDATION_PASS',len(checks),'scenes')

if '--validate-only' in sys.argv:
    validate(json.loads(REPORT.read_text()));sys.exit(0)

starter=json.loads((ROOT/'ArtSource/Manifests/city_starter_kit.json').read_text())
def park_export(kind):
    if kind=='Skate':return ROOT/'ArtSource/Exports/ParkRefinement/SM_Building_SkatePark_A.fbx'
    return ROOT/next(e['export'] for e in starter['assets'] if e['asset_id']=='building.park.'+kind.lower())
paths=[park_export(kind) for kind in ('Skate','Baseball','Soccer','Basketball')]
paths += [ROOT/'ArtSource/Blender/CityCitizenRigPrototype.blend',ROOT/'ArtSource/Exports/SM_Park_Skateboard_A.fbx',
          ROOT/'ArtSource/Exports/SM_Scenery_Broadleaf_A.fbx']
protected={p.relative_to(ROOT).as_posix():digest(p) for p in paths}
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'ArtSource/Blender/CityCitizenRigPrototype.blend'))
template=bpy.data.objects['SK_Citizen_Rig_Casual'];template.hide_set(False);template.parent.hide_set(False)
original=bpy.context.scene

def import_mesh(path):
    before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(path))
    obj=next(o for o in set(bpy.data.objects)-before if o.type=='MESH')
    bpy.context.view_layer.objects.active=obj
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    obj.data=obj.data.copy()
    palette={'Park':(.18,.62,.22,1),'Trim':(.82,.73,.48,1),'Traffic':(.88,.88,.86,1),
             'Roof':(.10,.14,.19,1),'Bark':(.22,.12,.055,1),'Canopy':(.12,.32,.07,1)}
    color=obj.data.color_attributes.active_color or obj.data.color_attributes.new(name='D83PreviewPalette',type='BYTE_COLOR',domain='CORNER')
    obj.data.color_attributes.active_color=color
    for p in obj.data.polygons:
        for i in p.loop_indices:color.data[i].color=palette[obj.data.materials[p.material_index].name.split('.')[0]]
    return obj

def setup_scene(name,period,aim,scale,direction=(6,-5,2.2)):
    scene=bpy.data.scenes.new(name);bpy.context.window.scene=scene
    scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
    scene.frame_start=1;scene.frame_end=period;scene.render.fps=24
    scene.render.engine='BLENDER_WORKBENCH';scene.render.resolution_x=scene.render.resolution_y=480
    scene.render.resolution_percentage=100
    scene.world=bpy.data.worlds.new(name+'World');scene.world.color=(.11,.13,.16)
    scene.display.shading.light='STUDIO';scene.display.shading.color_type='VERTEX'
    scene.display.shading.show_shadows=True;scene.display.shading.show_cavity=True
    scene.display.shading.background_type='WORLD';scene.view_settings.view_transform='Standard'
    data=bpy.data.cameras.new(name+'Camera');camera=bpy.data.objects.new(name+'Camera',data)
    scene.collection.objects.link(camera);scene.camera=camera
    camera.location=Vector(aim)+Vector(direction)
    camera.rotation_euler=(Vector(aim)-camera.location).to_track_quat('-Z','Y').to_euler()
    data.type='ORTHO';data.ortho_scale=scale
    return scene

def citizen(scene,clip,period,location,yaw=0):
    mesh=template.copy();mesh.data=template.data.copy();mesh.name=scene.name+'_Citizen'
    rig=template.parent.copy();rig.data=template.parent.data.copy();rig.name=scene.name+'_Rig'
    rig.animation_data_clear();rig.animation_data_create()
    scene.collection.objects.link(rig);scene.collection.objects.link(mesh)
    mesh.parent=rig;mesh.matrix_parent_inverse.identity();mesh.location=(0,0,0)
    mesh.modifiers['D83HumanoidSkin'].object=rig
    for pose in rig.pose.bones:
        for constraint in pose.constraints:
            if constraint.type=='IK':constraint.target=rig
    rig.location=location;rig.rotation_euler=(0,0,yaw);rig.scale=(.811,.811,.811)
    mesh.hide_set(False);rig.hide_set(False);mesh.hide_render=False
    track=rig.animation_data.nla_tracks.new()
    action=bpy.data.actions[template.parent.name+'_'+clip]
    strip=track.strips.new(clip,1,action);strip.action_frame_start=1
    length=manifest['citizens'][0]['clips'][clip]
    strip.action_frame_end=length+1;strip.repeat=period/length
    return mesh,rig

def ball(name,radius,color):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=16,ring_count=8,radius=radius)
    obj=bpy.context.object;obj.name=name
    attr=obj.data.color_attributes.new(name='D83Ball',type='BYTE_COLOR',domain='CORNER')
    obj.data.color_attributes.active_color=attr
    for c in attr.data:c.color=(*color,1)
    for p in obj.data.polygons:p.use_smooth=True
    return obj

def key(obj,frame):
    for field in ('location','rotation_euler','scale'):obj.keyframe_insert(field,frame=frame)

scenes=[]
specs=[('Skate','SkateCarve',96),('Skate','SkatePush',48),
       ('Skate','SkateKickflip',72),('Skate','Skate180',96),('Skate','SkateLipStall',120),
       ('Baseball','BaseballPitch',72),('Baseball','BaseballSwing',48),
       ('Soccer','SoccerKick',48),('Soccer','SoccerRun',48),('Soccer','GoalkeeperSave',48),
       ('Basketball','BasketballDribble',48),('Basketball','BasketballShot',72)]
for sport,clip,period in specs:
    name='D83_'+clip
    base=Vector((10.5,0,0)) if sport=='Skate' else Vector((-9,-9,.46)) if clip=='BaseballPitch' else Vector((-17,-19,.2)) if sport=='Baseball' else Vector((0,0,.3 if sport=='Basketball' else .2))
    scene=setup_scene(name,period,base+Vector((0,0,1.2)),9 if sport=='Skate' else 4.8)
    if sport=='Skate':
        aim=base+Vector((0,0,1.8))
        scene.camera.location=aim+Vector((-8,-12,8))
        scene.camera.rotation_euler=(aim-scene.camera.location).to_track_quat('-Z','Y').to_euler()
    park=import_mesh(park_export(sport));park.name=name+'_Park'
    mesh,rig=citizen(scene,clip,period,base)
    props=[];details={}
    if sport=='Skate':
        carrier=bpy.data.objects.new(name+'_BoardCarrier',None);scene.collection.objects.link(carrier)
        board=import_mesh(ROOT/'ArtSource/Exports/SM_Park_Skateboard_A.fbx');board.name=name+'_Skateboard'
        board.parent=carrier;board.location=(0,0,0);board.scale=(BOARD_SCALE,)*3
        # Dark grip tape also makes the underside visible during a full flip.
        color=board.data.color_attributes.active_color
        for p in board.data.polygons:
            if p.normal.z>.9 and p.center.z>.075:
                for i in p.loop_indices:color.data[i].color=(.055,.06,.065,1)
        rig.parent=carrier;rig.location=(0,0,DECK)
        props.append(board.name);details['carrier']=carrier.name
        details['board_length_m']=1.1*BOARD_SCALE
        details['board_scale']=BOARD_SCALE
        stance=skate_keys.prepare(scene,mesh,rig)
        details['sole_indices']=stance['soles']
        details['playback']='one_shot' if clip in ('SkateKickflip','Skate180','SkatePush') else 'loop'
        details['speed_m_s']=1.6
        if clip in ('SkateKickflip','Skate180'):
            _,_,start,end=skate_keys.flight(clip,0)
            details['flight_start']=start;details['flight_end']=end;details['gravity_m_s2']=9.81
    elif sport=='Baseball':
        projectile=ball(name+'_Ball',.055,(.95,.95,.9));props.append(projectile.name);details['ball']=projectile.name
        if clip=='BaseballSwing':
            bpy.ops.mesh.primitive_cone_add(vertices=16,radius1=.024,radius2=.048,depth=.85)
            bat=bpy.context.object;bat.name=name+'_Bat'
            attr=bat.data.color_attributes.new(name='BatWood',type='BYTE_COLOR',domain='CORNER')
            bat.data.color_attributes.active_color=attr
            for c in attr.data:c.color=(.58,.34,.15,1)
            props.append(bat.name);details['bat']=bat.name
        else:details['release_frame']=round(.48*period)+1
    else:
        projectile=ball(name+'_Ball',.13 if sport=='Soccer' else .15,
                        (.9,.9,.88) if sport=='Soccer' else (.72,.26,.055))
        props.append(projectile.name);details['ball']=projectile.name
    release=details.get('release_frame',round(.45*period)+1)
    release_hand=None
    contact=None
    if clip in ('BaseballSwing','SoccerKick'):
        impact=round(.43*period)+1;details['impact_frame']=impact
        scene.frame_set(impact)
        if clip=='BaseballSwing':
            chest=rig.pose.bones['chest'].matrix.to_quaternion()
            direction=(rig.matrix_world.to_quaternion()@chest)@Vector((.25,0,.95));direction.normalize()
            contact=rig.matrix_world@rig.pose.bones['hand.R'].tail+direction*.70
        else:contact=rig.matrix_world@rig.pose.bones['foot.R'].tail+Vector((.08,0,0))
    for frame in range(1,period+2):
        t=(frame-1)/period
        scene.frame_set(frame)
        if sport=='Skate':
            rig.location=(0,0,DECK);board.rotation_euler=(0,0,0)
            if clip=='SkateLipStall':
                start_x=12.7;lip_x=13+.35*BOARD_SCALE
                start_z=.36+.12*(start_x-8)**2+CLEARANCE;lip_z=3.36+CLEARANCE
                start_pitch=-math.atan(.24*(start_x-8))
                if t<.30 or t>.75:
                    u=ease(t/.30) if t<.30 else 1-ease((t-.75)/.25)
                    x=8+(start_x-8)*u
                    z=.36+.12*(x-8)**2+CLEARANCE;pitch=-math.atan(.24*(x-8));shift=0
                elif t<.40 or t>.65:
                    u=(t-.30)/.10 if t<.40 else 1-(t-.65)/.10
                    shift=ease(u);x=start_x+(lip_x-start_x)*shift
                    z=start_z+(lip_z-start_z)*shift+.22*math.sin(math.pi*u)
                    pitch=start_pitch*(1-shift)
                else:x=lip_x;z=lip_z;pitch=0;shift=1
                carrier.location=(x,0,z);carrier.rotation_euler=(0,pitch,0)
            elif clip in ('SkateKickflip','Skate180'):
                u,in_air,start,end=skate_keys.flight(clip,t)
                duration=(end-start)*period/24
                elapsed=u*duration
                lift=.5*9.81*elapsed*(duration-elapsed)
                x=(1.5 if clip=='SkateKickflip' else .5)+1.6*t*period/24
                carrier.location=(x,0,.36+CLEARANCE+lift)
                yaw=ease(u)*math.pi if clip=='Skate180' else 0
                carrier.rotation_euler=(0,0,yaw)
                if clip=='SkateKickflip':
                    board.rotation_euler.x=-math.tau*ease(u)
                    rig.location.z+=.18*math.sin(math.pi*u)
            else:
                x=8+4.7*(1-math.cos(t*math.tau))/2 if clip=='SkateCarve' else 2+1.6*t*period/24
                ramp=max(0,x-8)
                carrier.location=(x,0,.36+.12*ramp*ramp+CLEARANCE)
                carrier.rotation_euler=(0,-math.atan(.24*ramp),0)
            key(carrier,frame);key(board,frame);key(rig,frame)
            skate_keys.bake_frame(rig,stance,clip,t,frame,carrier.rotation_euler.y)
            aim=carrier.location+Vector((0,0,.7))
            scene.camera.location=aim+Vector((-6,-8,5));scene.camera.data.ortho_scale=4.8
            scene.camera.rotation_euler=(aim-scene.camera.location).to_track_quat('-Z','Y').to_euler()
            key(scene.camera,frame)
        elif clip=='BaseballPitch':
            hand=rig.matrix_world@rig.pose.bones['hand.R'].tail
            if frame<=release:
                projectile.location=hand
                release_hand=hand.copy()
            else:
                u=min(1,(frame-release)/(period*.27))
                target=base+Vector((5,0,.1))
                projectile.location=release_hand.lerp(target,u)+Vector((0,0,.6*math.sin(math.pi*u)))
            projectile.scale=(1,1,1) if t<.78 or t==1 else (0,0,0)
            if frame==period+1:projectile.location=rig.matrix_world@rig.pose.bones['hand.R'].tail
            key(projectile,frame)
        elif clip=='BaseballSwing':
            hand=rig.matrix_world@rig.pose.bones['hand.R'].tail
            chest=rig.pose.bones['chest'].matrix.to_quaternion()
            vector=(rig.matrix_world.to_quaternion()@chest)@Vector((.25,0,.95))
            vector.normalize()
            bat.location=hand+vector*.425
            bat.rotation_euler=vector.to_track_quat('Z','Y').to_euler();key(bat,frame)
            if frame<=impact:
                u=(frame-1)/(impact-1)
                projectile.location=contact+Vector((2*(1-u),0,0))
            else:
                u=min(1,(frame-impact)/(period*.33))
                projectile.location=contact+Vector((4*u,0,1.8*math.sin(math.pi*u)))
            projectile.scale=(1,1,1) if t<.8 or t==1 else (0,0,0);key(projectile,frame)
            if t==1:
                projectile.location=contact+Vector((2,0,0));key(projectile,frame)
        elif clip=='SoccerKick':
            initial=base+Vector((.5,-.075,.13))
            if frame<=impact:
                projectile.location=initial.lerp(contact,(frame-1)/(impact-1))
            else:
                u=min(1,(frame-impact)/(period*.35))
                projectile.location=contact+Vector((4*u,0,.75*math.sin(math.pi*u)))
            projectile.rotation_euler=(0,t*math.tau*3,0)
            projectile.scale=(1,1,1) if t<.85 or t==1 else (0,0,0)
            if t==1:projectile.location=base+Vector((.5,-.075,.13))
            key(projectile,frame)
        elif clip=='SoccerRun':
            # In-place dribble test: small ball rolls/taps in front of the runner.
            projectile.location=base+Vector((.55+.1*math.sin(t*math.tau*2),0,.13))
            projectile.rotation_euler=(0,t*math.tau*4,0);key(projectile,frame)
        elif clip=='GoalkeeperSave':
            reach=math.sin(math.pi*t)**2
            projectile.location=base+Vector((2.8-2.25*reach,.5*reach,.8+.15*reach))
            key(projectile,frame)
        elif clip=='BasketballDribble':
            projectile.location=base+Vector((.35,-.22,.15+.65*(1-math.cos(t*math.tau*2))/2))
            key(projectile,frame)
        else:
            hand=rig.matrix_world@rig.pose.bones['hand.R'].tail
            if frame<=release:projectile.location=hand+Vector((.05,.1,.05));release_hand=projectile.location.copy()
            else:
                u=min(1,(frame-release)/(period*.30))
                projectile.location=release_hand+Vector((4*u,0,1.8*math.sin(math.pi*u)))
            projectile.scale=(1,1,1) if t<.84 or t==1 else (0,0,0)
            if t==1:projectile.location=hand+Vector((.05,.1,.05))
            key(projectile,frame)
        if frame<=period and frame%2==1:
            scene.render.filepath=str(OUT/f'{clip}-{frame:03}.png');bpy.ops.render.render(write_still=True)
    if sport=='Skate':
        skate_keys.finish(rig,stance)
        for animated in (carrier,board,rig,scene.camera):
            for layer in animated.animation_data.action.layers:
                for strip in layer.strips:
                    for bag in strip.channelbags:
                        for curve in bag.fcurves:
                            for point in curve.keyframe_points:point.interpolation='LINEAR'
    scene.frame_set(1)
    scenes.append(dict(scene=name,sport=sport,clip=clip,period=period,citizen=mesh.name,props=props,**details))

# Actual background source beside the pruned simple-park tree at matched height.
scene=setup_scene('D83_TreeDesign',48,(2.5,1.5625,3.4),15,(10,-16,7))
park=import_mesh(ROOT/'ArtSource/Exports/ParkRefinement/SM_Building_Park_A.fbx')
bm=bmesh.new();bm.from_mesh(park.data)
remove=[v for v in bm.verts if not (v.co.x<-4 and v.co.y<-4 and v.co.z>.35)]
bmesh.ops.delete(bm,geom=remove,context='VERTS')
for v in bm.verts:v.co+=Vector((7,7,-.4))
bm.to_mesh(park.data);bm.free();park.name='D83_PrunedParkTree';park.location=(5,3.125,0)
background=import_mesh(ROOT/'ArtSource/Exports/SM_Scenery_Broadleaf_A.fbx')
background.name='D83_BackgroundBroadleaf'
height=max(v.co.z for v in background.data.vertices)-min(v.co.z for v in background.data.vertices)
background.scale*=6.95/height
scene.render.resolution_x=1000;scene.render.resolution_y=650
scene.render.filepath=str(OUT/'Tree-Design-Comparison.png');bpy.ops.render.render(write_still=True)
bpy.data.scenes.remove(original)
bpy.context.window.scene=bpy.data.scenes['D83_SkateCarve']
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':area.spaces.active.region_3d.view_perspective='CAMERA'
bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE))
report=dict(task='D83',source=SOURCE.relative_to(ROOT).as_posix(),source_sha256=digest(SOURCE),
            scenes=scenes,protected_inputs=protected,
            limitations=['Blender-only keyframe/contact prototypes; no gameplay callbacks or native animation policy',
                         'Balls recycle invisibly between repetitions; sports choreography/contact polish remains pending',
                         'Run/dribble propulsion remains provisional; carrier translation is separate from bone clips',
                         'Revised skate stance is scene-only; original seventeen-clip rig blend/FBXs remain unchanged',
                         'Kickflip/180 are one-way passes; timeline replay cuts back to the approach'])
REPORT.write_text(json.dumps(report,indent=2)+'\n')
print('CITYSIM_D83_SPORTS_AUTHORING_PASS',len(scenes),'scenes')
validate(report)
