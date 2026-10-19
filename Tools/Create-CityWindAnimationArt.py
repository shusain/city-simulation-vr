"""Render split turbine body/rotor before exporting; retains the full mesh for previews/damage."""
from pathlib import Path
import bpy,bmesh,math,json,sys
from mathutils import Vector
root=Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from CityArtKit import (
    MATERIALS, beam, box, combine, cylinder, export_one, initialize_materials,
)
initialize_materials()
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
for m in MATERIALS.values():
 m.use_nodes=True;m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=m.diffuse_color
body=combine('SM_Building_WindPower_Base_A',[box('Pad',(15.6,15.6,.3),(0,0,.15),'Trim'),cylinder('Mast',.65,17,(0,0,8.8),'Trim'),box('Nacelle',(2,3,1.4),(0,0,17.8),'Power')])
parts=[box('Hub',(1.2,.8,1.2),(0,0,0),'Power')]
for a in (0,120,240):
 t=math.radians(a);parts.append(beam('Blade',(0,0,0),(6*math.sin(t),0,6*math.cos(t)),.65,'Trim'))
rotor=combine('SM_WindPower_Rotor_A',parts)
entries=[]
for obj,role in ((body,'body'),(rotor,'rotor')):
 bm=bmesh.new();bm.from_mesh(obj.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));assert not any(e.is_boundary for e in bm.edges);assert not any(f.calc_area()<1e-8 for f in bm.faces);bm.to_mesh(obj.data);bm.free();obj.data.calc_loop_triangles();assert len(obj.data.loop_triangles)<800
 entries.append(dict(asset_id='wind.animation.'+role,source='ArtSource/Blender/CityWindAnimation.blend',export='ArtSource/Exports/'+obj.name+'.fbx',mesh_destination='/Game/CitySim/Art/Meshes/'+obj.name,dimensions_m=[round(float(v),4) for v in obj.dimensions],footprint_cells=[2,2],material_slots={s.name:'/Game/CitySim/Art/Materials/MI_City'+s.name for s in obj.data.materials},lod_policy='one shared mesh below800 triangles',collision_policy='none',pivot_policy='ground center' if role=='body' else 'rotor axle center',provenance='Original project Blender primitives; Create-CityWindAnimationArt.py',wind_animation=True))
out=root/'Artifacts/Preview';out.mkdir(exist_ok=True,parents=True)
if '--export' in sys.argv:
 assert (out/'D56-Wind-Rotor-Blender.png').exists(),'Review preview before export'
 for obj in (body,rotor):export_one(obj)
 p=root/'ArtSource/Manifests/city_starter_kit.json';m=json.loads(p.read_text(encoding='utf-8'));m['assets']=[e for e in m['assets'] if not e.get('wind_animation')]+entries;p.write_text(json.dumps(m,indent=2)+'\n',encoding='utf-8')
else:
 # Source retains axle-centered rotor. Present three assembled orientations for QA.
 bpy.ops.wm.save_as_mainfile(filepath=str(root/'ArtSource/Blender/CityWindAnimation.blend'))
 rotor.location=(0,-1.7,17.8)
 for x,angle in ((22,40),(44,80)):
  b=body.copy();b.data=body.data.copy();bpy.context.collection.objects.link(b);b.location.x=x
  r=rotor.copy();r.data=rotor.data.copy();bpy.context.collection.objects.link(r);r.location.x=x;r.rotation_euler.y=math.radians(angle)
 scene=bpy.context.scene;scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1200;scene.render.resolution_y=800;scene.render.resolution_percentage=100;scene.world.color=(.18,.18,.18)
 bpy.ops.object.light_add(type='AREA',location=(20,-30,50));bpy.context.object.data.energy=45000;bpy.context.object.data.size=60
 bpy.ops.object.camera_add(location=(65,-85,50));cam=bpy.context.object;target=Vector((22,0,10));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=80;scene.camera=cam;scene.render.filepath=str(out/'D56-Wind-Rotor-Blender.png');bpy.ops.render.render(write_still=True)
print('CITYSIM_WIND_ART_READY',entries)
