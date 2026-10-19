"""Render original facility meshes for review first; export only with --export."""
from pathlib import Path
import bpy,bmesh,math,json,sys
from mathutils import Vector
root=Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from CityArtKit import (
    MATERIALS, beam, box, cylinder, export_one, initialize_materials,
)
initialize_materials()
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
names=['WaterTower','CoalPower','OilPower','WindPower','SolarPower','NuclearPower','College','PrimarySchool','SecondarySchool']
meshes=[];entries=[];report=[]
for mat in MATERIALS.values():
 mat.use_nodes=True;shader=mat.node_tree.nodes.get('Principled BSDF');shader.inputs['Base Color'].default_value=mat.diffuse_color;shader.inputs['Roughness'].default_value=.8
def cooling_tower(name,x):
 vertices=[]
 for z,r in ((.3,4),(3,2.8),(7,2.4),(10.3,4)):
  for i in range(12):vertices.append((x+r*math.cos(i*math.tau/12),3+r*math.sin(i*math.tau/12),z))
 faces=[tuple(reversed(range(12))),tuple(range(36,48))]
 for ring in range(3):
  for i in range(12):j=(i+1)%12;faces.append((ring*12+i,ring*12+j,(ring+1)*12+j,(ring+1)*12+i))
 data=bpy.data.meshes.new(name);data.from_pydata(vertices,[],faces);data.materials.append(MATERIALS['Trim']);obj=bpy.data.objects.new(name,data);bpy.context.collection.objects.link(obj);return obj
for n in names:
 slot='Service' if n=='WaterTower' else 'Trim' if n in ('College','PrimarySchool','SecondarySchool') else 'Roof' if n=='SolarPower' else 'Power'
 width=1 if n=='WaterTower' else 3 if n in ('SolarPower','NuclearPower','College','SecondarySchool') else 2
 accent='Roof' if n in ('College','PrimarySchool','SecondarySchool') else 'Trim'
 half=width*4-.2
 parts=[box(n+'_Pad',(half*2,half*2,.3),(0,0,.15),accent)]
 if n=='WaterTower':
  for x in (-2,2):
   for y in (-2,2):parts.append(box(n+'_Leg',(.5,.5,8),(x,y,4.3),'Trim'))
  parts.extend([cylinder(n+'_Tank',3,4,(0,0,10),slot),cylinder(n+'_Lid',3.2,.35,(0,0,12.2),'Trim'),box(n+'_Pipe',(.4,.4,9),(0,2.5,4.8),slot)])
 elif n in ('CoalPower','OilPower'):
  parts.extend([box(n+'_Hall',(9,12,6),(-2,0,3.3),slot),box(n+'_Roof',(9.4,12.4,.4),(-2,0,6.5),'Trim')])
  if n=='CoalPower':
   for y in (-3,3):parts.extend([cylinder(n+'_Stack',.85,12,(4,y,6.3),'Trim'),cylinder(n+'_StackRing',1,.4,(4,y,10),slot)])
  else:
   for y in (-3,3):parts.extend([cylinder(n+'_OilTank',2.2,5,(4,y,2.8),'Trim'),cylinder(n+'_OilCap',2.3,.25,(4,y,5.4),slot)])
   parts.append(cylinder(n+'_Stack',.7,10,(-3,2,8.3),'Trim'))
 elif n=='WindPower':
  parts.extend([cylinder(n+'_Mast',.65,17,(0,0,8.8),'Trim'),box(n+'_Nacelle',(2,3,1.4),(0,0,17.8),slot)])
  for a in (0,120,240):
   angle=math.radians(a)
   parts.append(beam(n+'_Blade',(0,-1.7,17.8),(6*math.sin(angle),-1.7,17.8+6*math.cos(angle)),.65,'Trim'))
  parts.append(box(n+'_Hub',(1.2,.8,1.2),(0,-1.7,17.8),slot))
 elif n=='SolarPower':
  for x in (-8,-3,2,7):
   for y in (-7,0,7):
    parts.append(box(n+'_Stand',(.5,.5,1.8),(x,y,1.2),'Trim'))
    p=box(n+'_Panel',(4.5,5.5,.3),(x,y,2.2),slot);p.rotation_euler.x=math.radians(15);parts.append(p)
 elif n=='NuclearPower':
  for x in (-6,6):
   parts.extend([cooling_tower(n+'_CoolingTower',x),cylinder(n+'_CoolingRim',4.3,.5,(x,3,10.5),slot)])
  parts.extend([box(n+'_Reactor',(9,7,7),(0,-6,3.8),slot),cylinder(n+'_ReactorCap',4,1,(0,-6,7.8),'Trim')])
 elif n in ('PrimarySchool','SecondarySchool'):
  h=4.5 if n=='PrimarySchool' else 7
  parts.extend([box(n+'_Classrooms',(half*1.5,half*1.2,h),(0,1,h/2+.3),slot),box(n+'_Roof',(half*1.5+.4,half*1.2+.4,.4),(0,1,h+.5),accent),box(n+'_Entrance',(3,2,3),(0,-half*.6,1.8),slot),box(n+'_Door',(1.5,.15,2.5),(0,-half*.6-1.08,1.55),accent)])
  for x in (-half*.5,-half*.25,half*.25,half*.5):
   for z in ([2.8] if n=='PrimarySchool' else [2.8,5.6]):parts.append(box(n+'_Window',(1.6,.12,1.5),(x,1-half*.6-.07,z),accent))
  if n=='PrimarySchool':
   for x in (-3,3):parts.extend([box(n+'_PlayPost',(.3,.3,2),(x,half*.8,1.3),slot),beam(n+'_PlayBeam',(-3,half*.8,2.3),(3,half*.8,2.3),.25,accent)])
  else:parts.extend([box(n+'_Gym',(6,7,4),(half*.6,-half*.4,2.3),slot),box(n+'_GymRoof',(6.4,7.4,.35),(half*.6,-half*.4,4.5),accent)])
 else:
  parts.extend([box(n+'_Main',(18,9,8),(0,3,4.3),slot),box(n+'_Roof',(18.4,9.4,.35),(0,3,8.5),accent)])
  for x in (-8,8):parts.extend([box(n+'_Wing',(5,10,5),(x,-5,2.8),slot),box(n+'_WingRoof',(5.3,10.3,.3),(x,-5,5.45),accent)])
  for x in (-3,0,3):parts.append(box(n+'_Column',(.6,.6,5),(x,-2.5,2.8),accent))
  parts.extend([box(n+'_Portico',(9,4,.5),(0,-2.5,5.5),accent),box(n+'_Door',(2,.3,4),(0,-1.6,2.5),accent)])
  for x in (-6,-3,0,3,6):parts.append(box(n+'_UpperWindow',(1.4,.15,1.4),(x,-1.6,6.8),accent))
 bpy.ops.object.select_all(action='DESELECT')
 for p in parts:p.select_set(True)
 bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();obj=parts[0]
 # Ground-center pivot independent of the presentation arrangement.
 bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
 obj.name=f'SM_Building_{n}_A';old=list(obj.data.materials);indices=[0 if old[f.material_index].name.split('.')[0]==slot else 1 for f in obj.data.polygons]
 obj.data.materials.clear();obj.data.materials.append(MATERIALS[slot]);obj.data.materials.append(MATERIALS[accent])
 for f,i in zip(obj.data.polygons,indices):f.material_index=i
 for damaged in (False,True):
  mesh=obj if not damaged else obj.copy()
  if damaged:
   mesh.data=obj.data.copy();bpy.context.collection.objects.link(mesh);mesh.name=obj.name+'_Damaged'
   for v in mesh.data.vertices:
    if v.co.z>3:v.co.z*=.72+.07*math.sin(v.co.x*.5+v.co.y*.4)
  bm=bmesh.new();bm.from_mesh(mesh.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
  if any(e.is_boundary for e in bm.edges) or any(f.calc_area()<1e-8 for f in bm.faces):raise RuntimeError(f'Open/degenerate mesh {mesh.name}')
  bm.to_mesh(mesh.data);bm.free();mesh.data.update();bpy.context.view_layer.update();mesh.data.calc_loop_triangles();tris=len(mesh.data.loop_triangles)
  if tris>800:raise RuntimeError(f'Triangle budget {mesh.name}: {tris}')
  e=dict(asset_id=f'building.{n.lower()}.a'+('.damaged' if damaged else ''),source='ArtSource/Blender/CityAlternativeFacilityKit.blend',export=f'ArtSource/Exports/{mesh.name}.fbx',mesh_destination=f'/Game/CitySim/Art/Meshes/{mesh.name}',dimensions_m=[round(float(v),4) for v in mesh.dimensions],footprint_cells=[width,width],entrance_sides_local=['-Y'],presentation_yaw_offset_quarter_turns=2,material_slots={slot:'/Game/CitySim/Art/Materials/'+('MI_CityCharred' if damaged else 'MI_City'+slot),accent:'/Game/CitySim/Art/Materials/'+('MI_CityCharredTrim' if damaged else 'MI_City'+accent)},lod_policy='one original mesh below800 triangles',collision_policy='none; logical footprint',provenance='Original Blender primitives; Create-CityAlternativeFacilityArt.py',alternative_facility=True,civic_service=True,damaged_variant=damaged)
  entries.append(e);report.append(dict(name=mesh.name,triangles=tris,closed=True));meshes.append(mesh)
  mesh.location=(names.index(n)%5*32,names.index(n)//5*38,0)
  if damaged:mesh.location.y+=90
scene=bpy.context.scene;scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=1500;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
scene.world.color=(.18,.18,.18)
bpy.ops.object.light_add(type='AREA',location=(35,-30,70));bpy.context.object.data.energy=150000;bpy.context.object.data.shape='DISK';bpy.context.object.data.size=100
bpy.ops.object.camera_add(location=(110,-125,110));cam=bpy.context.object;cam.data.type='ORTHO';cam.data.ortho_scale=170;scene.camera=cam
out=root/'Artifacts/Preview';out.mkdir(parents=True,exist_ok=True)
if '--export' not in sys.argv:
 for damage in (False,True):
  for m in meshes:m.hide_render=('_Damaged' in m.name)!=damage
  target=Vector((60,20+(90 if damage else 0),6));cam.location=target+Vector((70,-140,110));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
  scene.render.filepath=str(out/('D56-Facilities-Damaged.png' if damage else 'D56-Facilities-Blender.png'));bpy.ops.render.render(write_still=True)
 for m in meshes:m.hide_render=False
 bpy.ops.wm.save_as_mainfile(filepath=str(root/'ArtSource/Blender/CityAlternativeFacilityKit.blend'))
 (root/'Artifacts/AssetImport/D56-facility-geometry.json').write_text(json.dumps(report,indent=2))
 print('CITYSIM_FACILITY_PREVIEW_READY; review before --export')
else:
 if not (out/'D56-Facilities-Blender.png').exists():raise RuntimeError('Render review must precede export')
 for m in meshes:
  m.location=(0,0,0);export_one(m)
 manifest_path=root/'ArtSource/Manifests/city_starter_kit.json';manifest=json.loads(manifest_path.read_text())
 manifest['assets']=[e for e in manifest['assets'] if not e.get('alternative_facility')]+entries
 manifest_path.write_text(json.dumps(manifest,indent=2)+'\n')
 print('CITYSIM_FACILITY_EXPORT_READY',len(entries))
