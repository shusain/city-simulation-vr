"""D88 five editable control-driven sports scenes; preserves accepted originals."""
from pathlib import Path
import bpy,sys,json,hashlib,math
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityCitizenRigControls import update
from CityCitizenSportsMotion import PERIODS,pose,palm,bat_direction,PITCH_RELEASE,BAT_IMPACT,KICK_IMPACT,KEEPER_CATCH
SOURCE=ROOT/'ArtSource/Blender/CityCitizenSportsRefinement.blend'
MANIFEST=ROOT/'ArtSource/Manifests/city_citizen_sports_refinement.json'
OUT=ROOT/'Artifacts/D88';OUT.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
if SOURCE.exists():
    assert '--replace-generated' in sys.argv,'Existing artist source preserved; use --replace-generated only for the unchanged generated baseline'
    assert sha(SOURCE)==json.loads(MANIFEST.read_text())['source_sha256'],'Preserve edited artist work separately'
inputs=[ROOT/'ArtSource/Blender'/n for n in ('CityCitizenControlRig.blend','CityCitizenRigPrototype.blend','CityParkAnimationPreview.blend')]
inputs+=list((ROOT/'ArtSource/Exports/ParkRefinement').glob('SK_Citizen*.fbx'))
protected={p.relative_to(ROOT).as_posix():sha(p) for p in inputs}
bpy.ops.wm.open_mainfile(filepath=str(inputs[0]))
if bpy.context.object.mode!='OBJECT':bpy.ops.object.mode_set(mode='OBJECT')
original=bpy.context.scene;template=bpy.data.objects['SK_Citizen_Rig_Casual'];template_rig=template.parent
records=[];samples={};rigs={}
starter=json.loads((ROOT/'ArtSource/Manifests/city_starter_kit.json').read_text())
def scene(name,period,aim,size,heading=0):
    s=bpy.data.scenes.new(name);bpy.context.window.scene=s;s.unit_settings.system='METRIC';s.unit_settings.scale_length=1
    s.frame_start=1;s.frame_end=period;s.render.fps=24
    s.render.engine='BLENDER_WORKBENCH';s.render.resolution_x=720;s.render.resolution_y=600;s.render.resolution_percentage=100
    s.display.shading.color_type='VERTEX';s.display.shading.light='STUDIO';s.display.shading.show_shadows=False;s.display.shading.show_cavity=True
    s.world=bpy.data.worlds.new(name+'_World');s.world.color=(.065,.085,.115);s.display.shading.background_type='WORLD'
    s.view_settings.view_transform='Standard'
    data=bpy.data.cameras.new(name+'_Camera');cam=bpy.data.objects.new(name+'_Camera',data);s.collection.objects.link(cam);s.camera=cam
    vector=Matrix.Rotation(heading,4,'Z')@Vector((5,-7,2.8));cam.location=Vector(aim)+vector
    cam.rotation_euler=(Vector(aim)-cam.location).to_track_quat('-Z','Y').to_euler();data.type='ORTHO';data.ortho_scale=size
    return s
def sphere(s,name,radius):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=16,ring_count=8,radius=radius)
    obj=bpy.context.object;obj.name=name;attr=obj.data.color_attributes.new(name='BallPalette',type='BYTE_COLOR',domain='CORNER')
    obj.data.color_attributes.active_color=attr
    for c in attr.data:c.color=(.95,.95,.9,1)
    for poly in obj.data.polygons:poly.use_smooth=True
    return obj
def key(obj,frame):
    for field in ('location','rotation_euler','scale'):obj.keyframe_insert(field,frame=frame)
def linear(action):
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for c in bag.fcurves:
                    for p in c.keyframe_points:p.interpolation='LINEAR'
def park(s,kind):
    path=ROOT/next(e['export'] for e in starter['assets'] if e['asset_id']=='building.park.'+kind.lower())
    protected[path.relative_to(ROOT).as_posix()]=sha(path)
    before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(path))
    obj=next(o for o in set(bpy.data.objects)-before if o.type=='MESH');obj.name=s.name+'_Park'
    bpy.context.view_layer.objects.active=obj;bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    attr=obj.data.color_attributes.active_color or obj.data.color_attributes.new(name='D88ParkPalette',type='BYTE_COLOR',domain='CORNER')
    obj.data.color_attributes.active_color=attr
    palette={'Park':(.18,.62,.22,1),'Trim':(.82,.73,.48,1),'Traffic':(.88,.88,.86,1),'Roof':(.10,.14,.19,1),'Bark':(.22,.12,.055,1),'Canopy':(.12,.32,.07,1)}
    for poly in obj.data.polygons:
        color=palette.get(obj.data.materials[poly.material_index].name.split('.')[0],(.3,.3,.3,1))
        for i in poly.loop_indices:attr.data[i].color=color
    return obj

for clip,period in PERIODS.items():
    base=Vector((-9,-9,.46)) if clip=='BaseballPitch' else Vector((-17,-19,.2)) if clip=='BaseballSwing' else Vector((0,0,.2))
    heading=math.atan2(-10,-8) if clip=='BaseballPitch' else math.atan2(10,8) if clip=='BaseballSwing' else 0
    s=scene('D88_'+clip,period,base+Vector((0,0,.8)),3.3,heading)
    field=park(s,'Baseball' if clip.startswith('Baseball') else 'Soccer')
    mesh=template.copy();mesh.data=template.data.copy();mesh.name=s.name+'_Citizen'
    rig=template_rig.copy();rig.data=template_rig.data.copy();rig.name=s.name+'_Rig';rig.animation_data_clear()
    s.collection.objects.link(mesh);s.collection.objects.link(rig)
    mesh.parent=rig;mesh.matrix_parent_inverse=Matrix.Identity(4);mesh.location=(0,0,0);mesh.rotation_euler=(0,0,0);mesh.scale=(1,1,1)
    mesh.modifiers['D83HumanoidSkin'].object=rig
    rig.location=base;rig.rotation_euler=(0,0,heading);rig.scale=(.811,)*3
    for side in ('L','R'):
        group=mesh.vertex_groups['hand.'+side].index
        vertices=[v.co for v in mesh.data.vertices if any(g.group==group and g.weight>.999 for g in v.groups)]
        assert vertices
        rig['D88_palm_'+side]=list(sum(vertices,Vector())/len(vertices))
    mesh.hide_set(False);rig.hide_set(False);mesh.hide_render=False
    for p in rig.pose.bones:
        for c in p.constraints:
            if hasattr(c,'target') and c.target==template_rig:c.target=rig
            if c.type=='IK' and c.pole_target==template_rig:c.pole_target=rig
    rig.animation_data_create();action=bpy.data.actions.new('D88_'+clip+'_Controls');action.use_fake_user=True;rig.animation_data.action=action
    for frame in range(1,period+2):
        s.frame_set(frame);pose(rig,clip,(frame-1)/period)
        for p in rig.pose.bones:
            if p.name in ('root','pelvis','spine','chest','neck','head','clavicle.L','clavicle.R') or p.name.startswith('CTRL_'):
                for channel in ('location','rotation_euler'):p.keyframe_insert(channel,frame=frame,group=p.name)
    linear(action)
    values=[]
    for frame in range(1,period+2):
        s.frame_set(frame);update(rig)
        values.append(dict(right_hand=list(rig.matrix_world@palm(rig,'R')),
                           left_hand=list(rig.matrix_world@palm(rig,'L')),
                           right_foot=list(rig.matrix_world@rig.pose.bones['foot.R'].tail)))
    samples[clip]=values;rigs[clip]=rig
    ball=sphere(s,s.name+'_Ball',.055 if clip.startswith('Baseball') else .13)
    props=[ball.name];record=dict(scene=s.name,clip=clip,sport='Baseball' if clip.startswith('Baseball') else 'Soccer',period=period,
                                  citizen=mesh.name,rig=rig.name,control_action=action.name,props=props,ball=ball.name,playback='loop')
    if clip=='BaseballSwing':
        bpy.ops.mesh.primitive_cone_add(vertices=16,radius1=.024,radius2=.048,depth=.85)
        bat=bpy.context.object;bat.name=s.name+'_Bat';attr=bat.data.color_attributes.new(name='BatWood',type='BYTE_COLOR',domain='CORNER')
        bat.data.color_attributes.active_color=attr
        for c in attr.data:c.color=(.58,.34,.15,1)
        record['bat']=bat.name;props.append(bat.name);record['impact_frame']=BAT_IMPACT
        for frame in range(1,period+2):
            s.frame_set(frame);direction=rig.matrix_world.to_quaternion()@bat_direction((frame-1)/period)
            bat.location=Vector(values[frame-1]['right_hand'])+direction*.425
            bat.rotation_euler=direction.to_track_quat('Z','Y').to_euler();key(bat,frame)
        linear(bat.animation_data.action)
    elif clip=='BaseballPitch':record['release_frame']=PITCH_RELEASE
    elif clip=='SoccerKick':record['impact_frame']=KICK_IMPACT
    elif clip=='GoalkeeperSave':record['catch_frame']=KEEPER_CATCH;record['hold_end_frame']=40
    records.append(record)

batter=next(e for e in records if e['clip']=='BaseballSwing');s=bpy.data.scenes[batter['scene']];bpy.context.window.scene=s;s.frame_set(BAT_IMPACT)
bat=bpy.data.objects[batter['bat']];contact=bat.matrix_world@Vector((0,0,.275))
release=Vector(samples['BaseballPitch'][PITCH_RELEASE-1]['right_hand'])
for record in records:
    clip=record['clip'];period=record['period'];s=bpy.data.scenes[record['scene']];bpy.context.window.scene=s
    ball=bpy.data.objects[record['ball']];rig=rigs[clip];forward=rig.matrix_world.to_quaternion()@Vector((1,0,0))
    if clip=='SoccerKick':
        toe=Vector(samples[clip][KICK_IMPACT-1]['right_foot'])
        dz=rig.location.z+.13-toe.z
        kick_contact=toe+Vector((math.sqrt(max(0,.13**2-dz**2)),0,dz))
        record['contact_point']=list(kick_contact)
    if clip=='GoalkeeperSave':
        catch=samples[clip][KEEPER_CATCH-1]
        catch_point=(Vector(catch['right_hand'])+Vector(catch['left_hand']))/2+forward*.10
    for frame in range(1,period+2):
        s.frame_set(frame);t=(frame-1)/period;scale=1
        if clip.startswith('Baseball'):
            if frame<=PITCH_RELEASE:flight=Vector(samples['BaseballPitch'][frame-1]['right_hand'])
            elif frame<=BAT_IMPACT:
                u=(frame-PITCH_RELEASE)/(BAT_IMPACT-PITCH_RELEASE);flight=release.lerp(contact,u)+Vector((0,0,.035*math.sin(math.pi*u)))
            else:flight=contact.copy()
            if clip=='BaseballPitch':
                ball.location=flight if frame<BAT_IMPACT else Vector(samples[clip][frame-1]['right_hand'])
                scale=1 if frame<BAT_IMPACT or frame>=67 else 0
            else:
                if frame<BAT_IMPACT:ball.location=flight;scale=0
                elif frame<=67:
                    u=(frame-BAT_IMPACT)/18;ball.location=contact+forward*6*u+Vector((0,0,1.1*math.sin(math.pi*u)))
                else:ball.location=Vector(samples['BaseballPitch'][0]['right_hand']);scale=0
        elif clip=='SoccerKick':
            if frame<=KICK_IMPACT:ball.location=kick_contact
            elif frame<=44:
                u=(frame-KICK_IMPACT)/19;ball.location=kick_contact+Vector((4*u,0,.7*math.sin(math.pi*u)))
            else:ball.location=kick_contact;scale=0 if frame<period+1 else 1
            ball.rotation_euler=(0,t*math.tau*3,0)
        elif clip=='SoccerRun':
            ball.location=rig.location+Vector((.39+.045*math.sin(t*math.tau*4),.065*math.sin(t*math.tau*2),.13))
            ball.rotation_euler=(0,t*math.tau*4,0)
        else:
            if frame<KEEPER_CATCH:
                u=(frame-1)/(KEEPER_CATCH-1);ball.location=(rig.location+Vector((2.8,0,.95))).lerp(catch_point,u)
            elif frame<=record['hold_end_frame']:
                hands=samples[clip][frame-1];ball.location=(Vector(hands['right_hand'])+Vector(hands['left_hand']))/2+forward*.10
            else:ball.location=rig.location+Vector((2.8,0,.95));scale=0 if frame<period+1 else 1
        ball.scale=(scale,)*3;key(ball,frame)
    linear(ball.animation_data.action);s.frame_set(1)

# One scene shows the actual synchronized pitcher/batter and single-ball handoff.
pair=scene('D88_BaseballPair',72,(-13,-14,1.2),17,math.atan2(10,8))
for record in records[:2]:
    source=bpy.data.scenes[record['scene']]
    for name in [record['citizen'],record['rig']]+record['props']:
        pair.collection.objects.link(bpy.data.objects[name])
    if record['clip']=='BaseballPitch':pair.collection.objects.link(next(o for o in source.objects if o.name.endswith('_Park')))
pair.frame_set(1)
bpy.context.window.scene=bpy.data.scenes['D88_BaseballPitch']
bpy.data.scenes.remove(original)
for o in list(bpy.data.objects):
    if not o.users_scene and o.type in ('ARMATURE','MESH') and not o.name.startswith('D87_Shape_'):bpy.data.objects.remove(o,do_unlink=True)
text=bpy.data.texts.new('D88_START_HERE');text.write('D88 control-driven sports clips\nChoose a D88_ scene and play its24fps timeline.\nBaseballPitch/BaseballSwing share72-frame timing; release36, bat contact49.\nSoccer kick impact25; keeper catch25.\nActions animate D87 controls; export20-bone bakes with Prepare-CityCitizenSportsRefinement.py.\nOriginal D83 files and D87 generic authoring file are unchanged.\n')
bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE))
assert all(sha(ROOT/p)==v for p,v in protected.items())
doc=dict(task='D88',source=SOURCE.relative_to(ROOT).as_posix(),source_sha256=sha(SOURCE),protected_inputs=protected,
         scenes=records,baseball_pair_scene=pair.name,baseball_release_frame=PITCH_RELEASE,baseball_impact_frame=BAT_IMPACT,
         native_selected=False,notes=['Five newly authored control actions; original D83 actions remain recovery sources',
                                      'Baseball swing period changes48 to72 to synchronize the paired pitch',
                                      'Body/prop presentation loops remain cosmetic; no sports gameplay state'])
MANIFEST.write_text(json.dumps(doc,indent=2)+'\n')
print('CITYSIM_D88_AUTHORING_PASS 5 control clips;paired72-frame baseball;original inputs preserved')
