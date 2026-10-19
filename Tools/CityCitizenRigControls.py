"""D87 Blender authoring controls; never writes accepted sources/native assets."""
import bpy, math, json
from mathutils import Vector, Matrix

BASE_BONES = ('root','pelvis','spine','chest','neck','head',
              'clavicle.L','upper_arm.L','forearm.L','hand.L','thigh.L','shin.L','foot.L',
              'clavicle.R','upper_arm.R','forearm.R','hand.R','thigh.R','shin.R','foot.R')
BODY_LIMITS = {
    'pelvis': ((-35,35),(-50,50),(-35,35)),
    'spine': ((-25,25),(-35,35),(-35,35)),
    'chest': ((-35,35),(-60,60),(-45,45)),
    'neck': ((-25,25),(-45,45),(-35,35)),
    'head': ((-35,35),(-65,65),(-40,40)),
    'clavicle': ((-30,30),(-30,30),(-30,30)),
    'hand': ((-40,40),(-60,60),(-70,70)),
    'foot': ((-45,45),(-45,45),(-75,75)),
}
HINGE_LIMITS = {'forearm.L': (-145,5), 'forearm.R': (-145,5),
                'shin.L': (-5,150), 'shin.R': (-5,150)}

def seed_hinges(rig):
    for side in ('L','R'):
        rig.pose.bones['forearm.'+side].rotation_euler.z=math.radians(-30)
        rig.pose.bones['shin.'+side].rotation_euler.z=math.radians(30)

def update(rig):
    rig.update_tag(refresh={'OBJECT'})
    bpy.context.view_layer.update()

def reset(rig):
    rig.animation_data_clear()
    for p in rig.pose.bones:
        p.matrix_basis=Matrix.Identity(4)
    seed_hinges(rig)
    update(rig)

def shape(name,kind):
    found=bpy.data.objects.get(name)
    if found:return found
    if kind=='ring':
        vertices=[(math.cos(i*math.tau/32),0,math.sin(i*math.tau/32)) for i in range(32)]
        edges=[(i,(i+1)%32) for i in range(32)]
    elif kind=='diamond':
        vertices=[(0,0,1),(1,0,0),(0,0,-1),(-1,0,0)]
        edges=[(i,(i+1)%4) for i in range(4)]
    else:
        vertices=[(-1,-.5,-.5),(-1,-.5,.5),(-1,.5,-.5),(-1,.5,.5),
                  (1,-.5,-.5),(1,-.5,.5),(1,.5,-.5),(1,.5,.5)]
        edges=[(i,j) for i in range(8) for j in range(i+1,8) if sum(a!=b for a,b in zip(vertices[i],vertices[j]))==1]
    data=bpy.data.meshes.new(name);data.from_pydata(vertices,edges,[])
    obj=bpy.data.objects.new(name,data)
    collection=bpy.data.collections.get('D87_ControlShapes')
    if not collection:
        collection=bpy.data.collections.new('D87_ControlShapes');bpy.context.scene.collection.children.link(collection)
    collection.objects.link(obj);obj.hide_render=True;obj.hide_set(True)
    return obj

def set_shape(p,kind,size,color):
    p.custom_shape=shape('D87_Shape_'+kind,kind)
    p.use_custom_shape_bone_size=False;p.custom_shape_scale_xyz=(size,)*3
    p.color.palette='CUSTOM'
    p.color.custom.normal=color;p.color.custom.select=(.95,.85,.3);p.color.custom.active=(1,1,1)

def limit(p,ranges):
    c=p.constraints.new('LIMIT_ROTATION');c.name='D87_LocalJointLimit'
    c.owner_space='LOCAL';c.use_transform_limit=True
    for axis,(lo,hi) in zip('xyz',ranges):
        setattr(c,'use_limit_'+axis,True)
        setattr(c,'min_'+axis,math.radians(lo));setattr(c,'max_'+axis,math.radians(hi))
    return c

def configure(rig):
    """Normalize authoring limbs; retain the original bind for the clean export."""
    rig['D87_original_bind']=json.dumps({b.name:dict(matrix=[list(row) for row in b.matrix_local],length=b.length)
                                       for b in rig.data.bones})
    rig.animation_data_clear()
    for p in rig.pose.bones:
        p.matrix_basis=Matrix.Identity(4)
        for c in list(p.constraints):p.constraints.remove(c)
    for collection in list(rig.data.collections):rig.data.collections.remove(collection)
    body=rig.data.collections.new('Body - FK controls')
    targets=rig.data.collections.new('Hands and feet - IK controls')
    poles=rig.data.collections.new('Elbows and knees - pole controls')
    deform=rig.data.collections.new('Limb bones - inspect only');deform.is_visible=False
    bpy.ops.object.select_all(action='DESELECT');rig.select_set(True)
    bpy.context.view_layer.objects.active=rig;bpy.ops.object.mode_set(mode='EDIT')
    # The old free-rotation rig has unrelated upper/lower rolls. Align each
    # chain's hinge axes, retaining all original joint positions and lengths.
    # The clean bake remaps inverse bind matrices to the original rotations.
    for side,sign in (('L',1),('R',-1)):
        for upper,lower in (('upper_arm','forearm'),('thigh','shin')):
            a=rig.data.edit_bones[upper+'.'+side];b=rig.data.edit_bones[lower+'.'+side]
            axis=(a.tail-a.head).cross(b.tail-b.head).normalized()
            if axis.y<0:axis=-axis
            a.align_roll(axis);b.align_roll(axis)
        for family in ('hand','foot'):rig.data.edit_bones[family+'.'+side].align_roll(Vector((0,1,0)))
    for side in ('L','R'):
        for kind,source in (('hand','hand'),('foot','foot')):
            b=rig.data.edit_bones.new('CTRL_'+kind+'.'+side)
            b.length=.14;b.matrix=rig.data.edit_bones[source+'.'+side].matrix.copy()
            b.use_deform=False;b.parent=rig.data.edit_bones['root']
        for kind,joint,direction in (('elbow','forearm',-.5),('knee','shin',.6)):
            b=rig.data.edit_bones.new('CTRL_'+kind+'_pole.'+side)
            b.head=rig.data.edit_bones[joint+'.'+side].head+Vector((direction,0,0))
            b.tail=b.head+Vector((0,0,.10));b.use_deform=False;b.parent=rig.data.edit_bones['root']
    bpy.ops.object.mode_set(mode='OBJECT')
    for p in rig.pose.bones:p.rotation_mode='XYZ'
    seed_hinges(rig)
    for p in rig.pose.bones:
        p.rotation_mode='XYZ';p.lock_scale=(True,True,True)
        if p.name.startswith('CTRL_'):
            p.lock_rotation=(True,)*3 if '_pole.' in p.name else (False,)*3
            (poles if '_pole.' in p.name else targets).assign(p.bone)
            color=(.1,.85,.5) if p.name.endswith('.L') else (.95,.55,.12)
            set_shape(p,'diamond' if '_pole.' in p.name else 'box' if 'foot' in p.name else 'ring',
                      .06 if '_pole.' in p.name else .11,color)
        else:
            p.lock_location=(False,)*3 if p.name in ('root','pelvis') else (True,)*3
            family=p.name.split('.')[0]
            if family in ('upper_arm','forearm','thigh','shin','hand','foot'):
                deform.assign(p.bone)
            else:
                body.assign(p.bone);set_shape(p,'ring',.22 if p.name=='root' else .12,(.2,.65,1))
            if family in BODY_LIMITS:limit(p,BODY_LIMITS[family])
    setup=[]
    for side in ('L','R'):
        for kind,upper,lower,pole in (('hand','upper_arm','forearm','elbow'),('foot','thigh','shin','knee')):
            p=rig.pose.bones[lower+'.'+side]
            c=p.constraints.new('IK');c.name='D87_'+kind.title()+'IK'
            c.target=rig;c.subtarget='CTRL_'+kind+'.'+side;c.chain_count=2
            c.pole_target=rig;c.pole_subtarget='CTRL_'+pole+'_pole.'+side
            c.use_stretch=False;c.iterations=128
            p.ik_stretch=0;rig.pose.bones[upper+'.'+side].ik_stretch=0
            # Calibrate the pole orientation against the normalized neutral roll.
            control=rig.pose.bones[c.subtarget]
            control.location=rig.data.bones[c.subtarget].matrix_local.to_3x3().inverted()@Vector(
                (.20,0,.10) if kind=='hand' else (.10,0,.20))
            def calibration_error():
                a=rig.pose.bones[upper+'.'+side].head
                tip=control.head;axis=(tip-a).normalized();distance=(tip-a).length
                upper_length=rig.data.bones[upper+'.'+side].length;lower_length=p.bone.length
                along=(upper_length**2-lower_length**2+distance**2)/(2*distance)
                offset=math.sqrt(max(0,upper_length**2-along**2))
                pole_position=rig.pose.bones[c.pole_subtarget].head
                direction=pole_position-a;direction=(direction-axis*direction.dot(axis)).normalized()
                wanted=a+axis*along+direction*offset
                return (p.head-wanted).length_squared
            best=None
            for step in range(72):
                c.pole_angle=-math.pi+step*math.tau/72;update(rig)
                error=calibration_error()
                if best is None or error<best[0]:best=(error,c.pole_angle)
            center=best[1]
            for step in range(-10,11):
                angle=center+step*math.radians(.25)
                c.pole_angle=math.atan2(math.sin(angle),math.cos(angle));update(rig)
                error=calibration_error()
                if error<best[0]:best=(error,c.pole_angle)
            c.pole_angle=best[1]
            control.location=(0,0,0)
            p.lock_ik_x=True;p.lock_ik_y=True;p.use_ik_limit_z=True
            lo,hi=HINGE_LIMITS[p.name];p.ik_min_z=math.radians(lo);p.ik_max_z=math.radians(hi)
            p.lock_rotation=(True,True,False)
            limit(p,((0,0),(0,0),(lo,hi)))
            terminal=rig.pose.bones[kind+'.'+side]
            copy=terminal.constraints.new('COPY_ROTATION');copy.name='D87_'+kind.title()+'Orientation'
            copy.target=rig;copy.subtarget='CTRL_'+kind+'.'+side;copy.owner_space='WORLD';copy.target_space='WORLD'
            # The wrist/ankle range applies after target orientation.
            terminal.constraints.move(len(terminal.constraints)-1,0)
            setup.append(dict(limb=kind+'.'+side,pole_angle_degrees=math.degrees(c.pole_angle)))
    rig.show_in_front=True;rig.data.display_type='STICK'
    rig['D87_authoring']='Pose body FK bones and CTRL hand/foot/pole bones. Do not animate hidden limb bones.'
    rig['D87_export']='Tools/Bake-CityCitizenControlRig.py exports only the original20 bones.'
    reset(rig)
    return setup

def move_control(rig,name,delta):
    p=rig.pose.bones[name]
    # Convert a delta in armature axes to this control's own pose axes.
    p.location=rig.data.bones[name].matrix_local.to_3x3().inverted()@Vector(delta)
    update(rig)

def preset(rig,name):
    reset(rig)
    if name=='Crouch':
        move_control(rig,'pelvis',(0,0,-.22))
        for side in ('L','R'):move_control(rig,'CTRL_hand.'+side,(.22,0,.08))
    elif name=='Reach':
        for side in ('L','R'):move_control(rig,'CTRL_hand.'+side,(.22,0,.14))
    elif name=='Step':
        move_control(rig,'pelvis',(0,0,-.05))
        move_control(rig,'CTRL_foot.L',(.23,0,.16))
        move_control(rig,'CTRL_hand.R',(.12,0,.08))
    update(rig)

def bake(rig,mesh,action_name,frames,scene):
    """Evaluate authoring rig, then bake original20 matrices into a clean clone."""
    frames=tuple(frames)
    original=json.loads(rig['D87_original_bind'])
    old_bind={n:Matrix(original[n]['matrix']) for n in BASE_BONES}
    samples=[]
    for frame in frames:
        scene.frame_set(frame);update(rig)
        matrices={n:rig.pose.bones[n].matrix@rig.data.bones[n].matrix_local.inverted()@old_bind[n] for n in BASE_BONES}
        # FBX import reconstructs coincident parent/child bones as connected,
        # which suppresses animated pelvis translation in Blender. Put that
        # body translation on the non-deforming root, preserving all skinning.
        rest_pelvis=matrices['root']@old_bind['root'].inverted()@old_bind['pelvis']
        matrices['root'].translation+=matrices['pelvis'].translation-rest_pelvis.translation
        samples.append(matrices)
    target=rig.copy();target.data=rig.data.copy();scene.collection.objects.link(target)
    target.name='D87_ExportRig';target.animation_data_clear();target.parent=None
    target.location=(0,0,0);target.rotation_euler=(0,0,0);target.scale=(1,1,1)
    for p in target.pose.bones:
        for c in list(p.constraints):p.constraints.remove(c)
        p.custom_shape=None
        p.lock_location=p.lock_rotation=p.lock_scale=(False,)*3
        p.lock_ik_x=p.lock_ik_y=p.lock_ik_z=False
        p.use_ik_limit_x=p.use_ik_limit_y=p.use_ik_limit_z=False
    bpy.ops.object.select_all(action='DESELECT');target.select_set(True)
    bpy.context.view_layer.objects.active=target;bpy.ops.object.mode_set(mode='EDIT')
    for b in list(target.data.edit_bones):
        if b.name not in BASE_BONES:target.data.edit_bones.remove(b)
    for b in target.data.edit_bones:
        b.matrix=old_bind[b.name];b.length=original[b.name]['length']
        b.use_connect=False
    bpy.ops.object.mode_set(mode='OBJECT')
    assert all(max(abs(target.data.bones[n].matrix_local[i][j]-old_bind[n][i][j]) for i in range(4) for j in range(4))<1e-6 for n in BASE_BONES)
    target.animation_data_create();action=bpy.data.actions.new(action_name);target.animation_data.action=action
    for frame,matrices in zip(frames,samples):
        scene.frame_set(frame)
        for p in target.pose.bones:
            p.rotation_mode='QUATERNION'
            args=dict(parent_matrix=matrices[p.parent.name],parent_matrix_local=p.parent.bone.matrix_local) if p.parent else {}
            p.matrix_basis=p.bone.convert_local_to_pose(matrices[p.name],p.bone.matrix_local,invert=True,**args)
            for field in ('location','rotation_quaternion','scale'):p.keyframe_insert(field,frame=frame,group=p.name)
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for point in curve.keyframe_points:point.interpolation='LINEAR'
    skin=mesh.copy();skin.data=mesh.data.copy();scene.collection.objects.link(skin)
    skin.name='D87_ExportCitizen';skin.parent=target;skin.matrix_parent_inverse=Matrix.Identity(4)
    skin.location=(0,0,0);skin.rotation_euler=(0,0,0);skin.scale=(1,1,1)
    for m in skin.modifiers:
        if m.type=='ARMATURE':m.object=target
    skin.hide_set(False);skin.hide_render=False;target.hide_set(False)
    update(target)
    return target,skin
