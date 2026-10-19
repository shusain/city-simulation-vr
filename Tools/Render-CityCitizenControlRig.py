"""D87 source renders and projected control coordinates; never saves source."""
from pathlib import Path
import bpy,sys,json,hashlib
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityCitizenRigControls import preset,update
OUT=ROOT/'Artifacts/D87/Preview';OUT.mkdir(parents=True,exist_ok=True)
SOURCE=ROOT/'ArtSource/Blender/CityCitizenControlRig.blend';digest=hashlib.sha256(SOURCE.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
if bpy.context.object.mode!='OBJECT':bpy.ops.object.mode_set(mode='OBJECT')
scene=bpy.context.scene;hero=bpy.data.objects['SK_Citizen_Rig_Casual'];rig=hero.parent
for obj in scene.objects:obj.hide_render=True
hero.hide_render=False;hero.hide_set(False);rig.hide_set(False);rig.location=(0,0,0)
scene.render.engine='BLENDER_WORKBENCH';scene.render.resolution_x=1100;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
scene.display.shading.light='STUDIO';scene.display.shading.color_type='VERTEX'
scene.display.shading.show_shadows=False;scene.display.shading.show_cavity=True
scene.display.shading.background_type='WORLD';scene.world.color=(.065,.085,.115)
scene.view_settings.view_transform='Standard'
data=bpy.data.cameras.new('D87_PreviewCamera');cam=bpy.data.objects.new('D87_PreviewCamera',data)
scene.collection.objects.link(cam);scene.camera=cam;data.type='ORTHO';data.ortho_scale=2.6
aim=Vector((0,0,.8));cam.location=aim+Vector((6,-4,1.0));cam.rotation_euler=(aim-cam.location).to_track_quat('-Z','Y').to_euler()
views={}
def capture(case):
    scene.render.filepath=str(OUT/(case+'-raw.png'));bpy.ops.render.render(write_still=True)
    def project(v):
        p=world_to_camera_view(scene,cam,rig.matrix_world@v)
        return [p.x*1100,(1-p.y)*1000]
    views[case]=[dict(name=p.name,head=project(p.head),tail=project(p.tail)) for p in rig.pose.bones]
for case in ('Neutral','Crouch','Reach','Step'):
    preset(rig,case);capture(case)
rig.animation_data_create();rig.animation_data.action=bpy.data.actions['D87_Demo_PlantedCrouch']
for frame in range(1,49,4):
    scene.frame_set(frame);update(rig);capture('Demo-'+str(frame).zfill(2))
(OUT/'Views.json').write_text(json.dumps(views,indent=2)+'\n')
assert digest==hashlib.sha256(SOURCE.read_bytes()).hexdigest()
print('CITYSIM_D87_RENDER_PASS 4 source poses;12 demo frames;source unchanged')
