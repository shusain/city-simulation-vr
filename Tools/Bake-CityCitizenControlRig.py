"""Bake a D87 control action to an isolated20-bone FBX; no native import."""
from pathlib import Path
import bpy, sys, argparse
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Tools'))
from CityCitizenRigControls import bake, update, BASE_BONES

def export(rig,mesh,path):
    bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);mesh.select_set(True)
    bpy.context.view_layer.objects.active=rig
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH','ARMATURE'},
        apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',
        mesh_smooth_type='FACE',add_leaf_bones=False,use_armature_deform_only=False,
        bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,
        bake_anim_simplify_factor=0,bake_anim_step=1,path_mode='STRIP')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=ROOT/'ArtSource/Blender/CityCitizenControlRig.blend')
    parser.add_argument('--mesh',default='SK_Citizen_Rig_Casual')
    parser.add_argument('--action',default='D87_Demo_PlantedCrouch')
    parser.add_argument('--start',type=int,default=1);parser.add_argument('--end',type=int,default=49)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    if args.output.exists():raise RuntimeError('Choose a new output path; existing exports are preserved')
    if args.end<args.start:raise RuntimeError('End must not precede start')
    bpy.ops.wm.open_mainfile(filepath=str(args.source.resolve()))
    if bpy.context.object and bpy.context.object.mode!='OBJECT':bpy.ops.object.mode_set(mode='OBJECT')
    mesh=bpy.data.objects[args.mesh];rig=mesh.parent
    rig.hide_set(False);mesh.hide_set(False)
    rig.location=(0,0,0);rig.rotation_euler=(0,0,0);rig.scale=(1,1,1)
    rig.animation_data_create();rig.animation_data.action=bpy.data.actions[args.action]
    bpy.context.scene.frame_start=args.start;bpy.context.scene.frame_end=args.end
    clean,skin=bake(rig,mesh,args.action+'_Baked',range(args.start,args.end+1),bpy.context.scene)
    assert set(clean.data.bones.keys())==set(BASE_BONES)
    assert not any(p.constraints for p in clean.pose.bones)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    export(clean,skin,args.output.resolve())
    print('CITYSIM_D87_BAKE_PASS',args.end-args.start+1,'frames;20 bones;0 controls;0 constraints;',args.output)
