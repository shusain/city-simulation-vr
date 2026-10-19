"""Blender-only stance posing and visual keyframe bake for D83 skate scenes."""
import bpy, math
from mathutils import Matrix, Vector, Quaternion

def flight(clip,t):
    start,end=(.34,.54) if clip=='SkateKickflip' else (.36,.50)
    u=max(0,min(1,(t-start)/(end-start)))
    return u,start<t<end,start,end

def smooth(u):
    u=max(0,min(1,u));return u*u*(3-2*u)

def pulse(t,start,end):
    return math.sin(math.pi*(t-start)/(end-start))**2 if start<t<end else 0

def prepare(scene,mesh,rig):
    rig.animation_data_clear();rig.animation_data_create()
    action=bpy.data.actions.new(scene.name+'_BakedSkatePose');action.use_fake_user=True
    rig.animation_data.action=action
    # Skate derivatives use rigid shoes. The accepted walking/field meshes and
    # their original skin weights remain read-only in CityCitizenRigPrototype.
    neighbors={v.index:set() for v in mesh.data.vertices}
    for edge in mesh.data.edges:
        a,b=edge.vertices;neighbors[a].add(b);neighbors[b].add(a)
    remaining=set(neighbors);soles={}
    while remaining:
        stack=[min(remaining)];comp=set(stack)
        while stack:
            for other in neighbors[stack.pop()]-comp:comp.add(other);stack.append(other)
        remaining-=comp
        points=[mesh.data.vertices[i].co for i in comp]
        if min(v.z for v in points)<1e-5 and max(v.z for v in points)<.11:
            side='L' if sum(v.y for v in points)>0 else 'R'
            for i in comp:
                for group in list(mesh.data.vertices[i].groups):mesh.vertex_groups[group.group].remove([i])
            mesh.vertex_groups['foot.'+side].add(list(comp),1,'REPLACE')
            soles[side]=[i for i in comp if mesh.data.vertices[i].co.z<1e-5]
    assert set(soles)=={'L','R'}
    for pose in rig.pose.bones:
        for constraint in list(pose.constraints):pose.constraints.remove(constraint)
    constraints=[];controls=[]
    for side,sign in (('L',1),('R',-1)):
        angle=math.pi/2 if side=='L' else math.radians(65)
        target=bpy.data.objects.new(scene.name+'_Ankle'+side,None)
        scene.collection.objects.link(target);target.parent=rig
        target.location=(-sign*.26-.055*math.cos(angle),-.055*math.sin(angle),.09)
        target.rotation_euler=(Matrix.Rotation(angle,4,'Z')@rig.data.bones['foot.'+side].matrix_local).to_quaternion().to_euler()
        pole=bpy.data.objects.new(scene.name+'_KneePole'+side,None)
        scene.collection.objects.link(pole);pole.parent=rig
        pole.location=(-sign*.25,.8,.5)
        leg=rig.pose.bones['shin.'+side].constraints.new('IK')
        leg.name='D83TemporarySkateIK';leg.target=target;leg.pole_target=pole
        leg.chain_count=2;leg.use_stretch=False
        foot=rig.pose.bones['foot.'+side].constraints.new('COPY_ROTATION')
        foot.target=target;foot.owner_space='WORLD';foot.target_space='WORLD'
        constraints.extend((leg,foot));controls.extend((target,pole))
    return dict(constraints=constraints,controls=controls,soles=soles,action=action)

def rotate(rig,name,axis,degrees):
    pose=rig.pose.bones[name]
    local=rig.data.bones[name].matrix_local.to_quaternion().inverted()@Vector(axis)
    pose.rotation_quaternion=Quaternion(local,math.radians(degrees))@pose.rotation_quaternion

def bake_frame(rig,state,clip,t,frame,bank_pitch=0):
    for constraint in state['constraints']:constraint.influence=1
    for pose in rig.pose.bones:
        pose.rotation_mode='QUATERNION';pose.rotation_quaternion=Quaternion()
        pose.location=(0,0,0);pose.scale=(1,1,1)
    u,in_air,start,end=flight(clip,t)
    prep=pulse(t,.06,start)
    tuck=pulse(t,start-.025,end+.025)
    landing=pulse(t,end-.02,min(1,end+.22))
    if clip not in ('SkateKickflip','Skate180'):
        prep=0;tuck=.4*pulse(t,.05,.95);landing=0
    rig.pose.bones['root'].location.y=-.12-.10*prep-.15*tuck-.07*landing
    rotate(rig,'root',(0,0,1),90)
    rotate(rig,'pelvis',(0,1,0),3)
    look=smooth(u) if clip=='Skate180' else 0
    rotate(rig,'chest',(0,0,1),-25+50*look)
    rotate(rig,'head',(0,0,1),-35+70*look)
    rotate(rig,'chest',(0,1,0),4)
    rotate(rig,'chest',(1,0,0),-math.degrees(bank_pitch)*.7)
    for side,sign in (('L',1),('R',-1)):
        rotate(rig,'upper_arm.'+side,(1,0,0),sign*(12+26*tuck))
        rotate(rig,'upper_arm.'+side,(0,1,0),-8 if side=='L' else 8)
        rotate(rig,'forearm.'+side,(0,1,0),-25)
    bpy.context.view_layer.update()
    matrices={pose.name:pose.matrix.copy() for pose in rig.pose.bones}
    for constraint in state['constraints']:constraint.influence=0
    # Assign parents before children; the saved result contains only keyframes.
    for pose in rig.pose.bones:
        pose.matrix=matrices[pose.name]
        for field in ('location','rotation_quaternion','scale'):pose.keyframe_insert(field,frame=frame,group=pose.name)

def finish(rig,state):
    for pose in rig.pose.bones:
        for constraint in list(pose.constraints):pose.constraints.remove(constraint)
    for obj in state['controls']:bpy.data.objects.remove(obj,do_unlink=True)
    for layer in state['action'].layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for point in curve.keyframe_points:point.interpolation='LINEAR'
