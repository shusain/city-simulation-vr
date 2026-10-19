"""Offline extension of d668972: contact columns and a three-role wind split.

All original/pre-existing source and export bytes stay unchanged. Requires fresh
extension destinations, no renderer, native import, runtime or CI operations.
"""
from pathlib import Path
from collections import Counter,defaultdict
import importlib.util,json,hashlib,math
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[3]
BASE=ROOT/'ArtSource/Manifests/city_utility_integration.json'
OUT=ROOT/'ArtSource/Exports/UtilityIntegration/Foundations'
WIND_OUT=ROOT/'ArtSource/Exports/UtilityIntegration/wind_turbine/ThreeRoles'
FOUNDATION_BLEND=ROOT/'ArtSource/Blender/UtilityIntegration/CityUtility_foundation_columns_v1.blend'
WIND_BLEND=ROOT/'ArtSource/Blender/UtilityIntegration/CityUtility_wind_three_roles_v1.blend'
MANIFEST=ROOT/'ArtSource/Manifests/city_utility_ground_wind_preparation.json'
spec=importlib.util.spec_from_file_location('utility_prep',ROOT/'Tools/Assets/Blender/Prepare-CityUtilityIntegration.py')
prep=importlib.util.module_from_spec(spec);spec.loader.exec_module(prep)
read,write,sha,relative=prep.read,prep.write,prep.digest,prep.relative
REFERENCE_HEAD='d668972effba3c74ffdbb9b64b1dd5f6df216daf'

def polygon_area(poly):return abs(sum(a[0]*b[1]-b[0]*a[1] for a,b in zip(poly,poly[1:]+poly[:1])))*.5
def oriented(triangle):
 a,b,c=triangle
 signed=(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
 assert abs(signed)>1e-12,triangle
 return triangle if signed>0 else [a,c,b]

def intersection_area(first,second):
 subject=[tuple(p[:2]) for p in oriented(first)]
 for a,b in zip(oriented(second),oriented(second)[1:]+oriented(second)[:1]):
  def distance(p):return (b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0])
  result=[]
  if not subject:return 0.
  for p,q in zip(subject,subject[1:]+subject[:1]):
   dp,dq=distance(p),distance(q);ip,iq=dp>=0,dq>=0
   if ip:result.append(p)
   if ip!=iq:
    t=dp/(dp-dq);result.append((p[0]+t*(q[0]-p[0]),p[1]+t*(q[1]-p[1])))
  subject=result
 return polygon_area(subject) if len(subject)>=3 else 0.

def overlaps(regions):
 result=[]
 for i,first in enumerate(regions):
  for second in regions[i+1:]:
   a,b=first['bounds_m'],second['bounds_m']
   if any(min(a['max'][k],b['max'][k])-max(a['min'][k],b['min'][k])<=1e-8 for k in (0,1)):continue
   area=sum(intersection_area(t,u) for t in first['contact']['horizontal_face_triangles_m'] for u in second['contact']['horizontal_face_triangles_m'])
   if area>1e-8:result.append(dict(first=first['component'],second=second['component'],source_top_z_m=[first['support_top_z_m'],second['support_top_z_m']],
    overlap_area_xy_m2=area,policy='Preserved as separate source columns; no union, invented pad, overlap removal or automatic runtime arbitration.'))
 return result

def foundation(name,regions,material):
 vertices=[];faces=[];uvcoords=[];columns=[]
 for region in regions:
  triangles=region['contact']['horizontal_face_triangles_m'];assert triangles
  xy=sorted({tuple(v[:2]) for triangle in triangles for v in triangle});indices={p:i for i,p in enumerate(xy)};count=len(xy);offset=len(vertices)
  vertices.extend([(x,y,0.) for x,y in xy]+[(x,y,-1.) for x,y in xy])
  caps=[[indices[tuple(v[:2])] for v in oriented(triangle)] for triangle in triangles]
  directed=Counter((a,b) for face in caps for a,b in zip(face,face[1:]+face[:1]))
  counts=Counter(tuple(sorted(edge)) for edge,n in directed.items() for _ in range(n))
  boundary=[edge for edge,n in directed.items() if counts[tuple(sorted(edge))]==1]
  assert all(n in (1,2) for n in counts.values()) and len(boundary)==sum(len(l['vertices_m']) for l in region['contact']['loops'])
  start=len(faces)
  for cap in caps:
   faces.append([offset+i for i in cap]);uvcoords.append([(xy[i][0]*.5,xy[i][1]*.5) for i in cap])
   faces.append([offset+count+i for i in reversed(cap)]);uvcoords.append([(xy[i][0]*.5,xy[i][1]*.5) for i in reversed(cap)])
  for a,b in boundary:
   length=math.dist(xy[a],xy[b])*.5
   faces.append([offset+b,offset+a,offset+count+a,offset+count+b]);uvcoords.append([(0.,0.),(length,0.),(length,.5),(0.,.5)])
  columns.append(dict(component=region['component'],kind=region['kind'],source_top_z_m=region['support_top_z_m'],
   vertex_start=offset,vertex_count=count*2,face_start=start,face_count=len(faces)-start,
   top_triangle_count=len(caps),boundary_edge_count=len(boundary),source_loop_count=len(region['contact']['loops']),source_hole_count=sum(l['hole'] for l in region['contact']['loops']),
   contact_area_m2=region['contact']['contact_area_m2']))
 mesh=bpy.data.meshes.new(name);mesh.from_pydata(vertices,[],faces);mesh.update();mesh.materials.append(material)
 uv=mesh.uv_layers.new(name='UV0')
 for p,coords in zip(mesh.polygons,uvcoords):
  p.use_smooth=False
  for i,value in zip(p.loop_indices,coords):uv.data[i].uv=value
 obj=bpy.data.objects.new(name,mesh);bpy.context.scene.collection.objects.link(obj)
 for column in columns:obj.vertex_groups.new(name=column['component']).add(list(range(column['vertex_start'],column['vertex_start']+column['vertex_count'])),1.,'REPLACE')
 return obj,columns

def check_columns(obj,columns):
 mesh=obj.data;mesh.calc_loop_triangles();records=[]
 for column in columns:
  first=column['face_start'];polys=list(mesh.polygons)[first:first+column['face_count']]
  edges=Counter(tuple(sorted(edge)) for p in polys for edge in p.edge_keys)
  assert edges and all(n==2 for n in edges.values()),('Not closed manifold',column['component'])
  half=Counter(edge for p in polys for edge in zip(list(p.vertices),list(p.vertices)[1:]+list(p.vertices)[:1]))
  assert all(half[(b,a)]==n for (a,b),n in half.items()),('Winding mismatch',column['component'])
  ids=set(range(column['vertex_start'],column['vertex_start']+column['vertex_count']))
  volume=0.
  for triangle in mesh.loop_triangles:
   if triangle.vertices[0] in ids:
    a,b,c=[mesh.vertices[i].co for i in triangle.vertices];volume+=a.dot(b.cross(c))/6
  assert abs(volume-column['contact_area_m2'])<max(.0002,column['contact_area_m2']*2e-5),(column['component'],volume,column['contact_area_m2'])
  assert volume>0
  records.append(dict(component=column['component'],closed_edges=len(edges),outward_signed_volume_m3=volume,area_times_unit_depth_m3=column['contact_area_m2']))
 return records

def main():
 assert not any(p.exists() for p in (OUT,WIND_OUT,FOUNDATION_BLEND,WIND_BLEND,MANIFEST)),'Use fresh extension destinations'
 base=read(BASE);assert len(base['assets'])==11
 protected={relative(p):sha(p) for folder in ['ArtSource/Blender/UtilityIntegration','ArtSource/Exports/UtilityIntegration'] for p in (ROOT/folder).rglob('*') if p.is_file()}
 protected.update(base['source_artifacts_unchanged']);protected[relative(BASE)]=sha(BASE)
 original_proof=ROOT/'ArtSource/Manifests/city_utility_preparation_verification.json';protected[relative(original_proof)]=sha(original_proof)
 OUT.mkdir();WIND_OUT.mkdir()
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
 concrete=bpy.data.materials.new('Concrete');concrete.use_nodes=True
 principal=concrete.node_tree.nodes.get('Principled BSDF');principal.inputs['Base Color'].default_value=(.52,.50,.46,1.);principal.inputs['Roughness'].default_value=.85
 assets=[]
 for a in base['assets']:
  groups=defaultdict(list)
  for region in a['support_regions']:groups[region['support_top_z_m']].append(region)
  entries=[]
  for z,regions in sorted(groups.items()):
   token=('%.6f'%z).rstrip('0').rstrip('.').replace('.','p') or '0'
   name=f'SM_Utility_{a["key"]}_v{a["version"]}_Foundation_Z{token}'
   obj,columns=foundation(name,regions,concrete);topology=check_columns(obj,columns)
   path=OUT/(name+'.fbx')
   with prep.checked_fbx(obj,path,['Concrete']):pass
   entries.append(dict(object=name,export=relative(path),export_sha256=sha(path),source_support_top_z_m=z,
    local_top_z_m=0.,local_bottom_z_m=-1.,local_xy='Exact source ground-centred XY, no horizontal offset or scaling',
    material_slot='Concrete',native_binding='Existing stock concrete material; native owner resolves actual asset path',
    columns=columns,topology=topology,roundtrip_pass=True,**prep.measure(obj)))
   obj.hide_set(True);obj.hide_render=True
  assets.append(dict(issue=a['issue'],version=a['version'],key=a['key'],source_regions=len(a['support_regions']),groups=entries,preserved_overlap_pairs=overlaps(a['support_regions'])))
  print('FOUNDATION_PREPARED',a['issue'],len(entries),'Z groups',len(a['support_regions']),'columns',len(assets[-1]['preserved_overlap_pairs']),'overlaps',flush=True)
 bpy.ops.wm.save_as_mainfile(filepath=str(FOUNDATION_BLEND))
 # Three-role split, preserving d668's joined and two-role reference exports.
 wind=next(a for a in base['assets'] if a['issue']==168);source=ROOT/wind['export_directory']
 obj=prep.load_source(source/'asset.blend');validation=read(source/'validation.json');parts=prep.parts_of(obj,validation['components']);byname={p['name']:p for p in parts}
 collar=byname['Yaw_Bearing_Collar'];lo,hi=collar['bounds']['min'],collar['bounds']['max'];pivot=[(x+y)*.5 for x,y in zip(lo,hi)]
 assert abs(pivot[0])<1e-7 and abs(pivot[1])<1e-7
 axle=wind['wind']['axle_xyz_m'];contract=read(source/'landmark-contract.json')
 stationary={p['name'] for p in parts if p['name'].startswith(('Mast_','Door_'))}
 rotor=set(contract['rotor_component_names']);yaw={p['name'] for p in parts}-stationary-rotor
 assert stationary and yaw and rotor and not stationary&rotor
 assert 'Yaw_Bearing_Collar' in yaw and 'Nacelle_Contoured_Housing' in yaw and 'Door_Flush_Panel' in stationary
 role_records=[]
 obj.data.calc_loop_triangles();original_normals=[tuple(n.vector) for n in obj.data.corner_normals]
 for lod in wind['lods']:
  omitted=set(lod.get('omitted_components',[]));selected=[p for p in parts if p['name'] not in omitted]
  for role,names,offset in [('Stationary',stationary,[0.,0.,0.]),('YawNacelle',yaw,pivot),('SpinRotor',rotor,axle)]:
   pieces=[p for p in selected if p['name'] in names];name=f'SM_Utility_wind_turbine_v1_{role}_LOD{lod["lod"]}'
   derived=prep.subset(obj,pieces,name,offset)
   normals=[original_normals[li] for part in pieces for p in part['polys'] for li in p.loop_indices]
   derived.data.normals_split_custom_set(normals)
   path=WIND_OUT/(name+'.fbx')
   with prep.checked_fbx(derived,path,[m.name for m in obj.data.materials]):pass
   role_records.append(dict(role=role,lod=lod['lod'],object=name,export=relative(path),export_sha256=sha(path),
    component_names=[p['name'] for p in pieces],source_pivot_m=offset,roundtrip_pass=True,source_corner_normals_copied=True,**prep.measure(derived)))
   derived.hide_set(True);derived.hide_render=True
 obj.name='WindApprovedReference'
 obj.hide_set(True);obj.hide_render=True
 bpy.ops.wm.save_as_mainfile(filepath=str(WIND_BLEND))
 assert all(sha(ROOT/path)==value for path,value in protected.items())
 doc=dict(parent_issue=157,terrain_owner=125,extends_local_preparation_head=REFERENCE_HEAD,
  base_manifest=dict(path=relative(BASE),sha256=sha(BASE)),base_verification=dict(path=relative(original_proof),sha256=sha(original_proof)),
  protected_original_and_reference_files=protected,
  foundation=dict(source=relative(FOUNDATION_BLEND),source_sha256=sha(FOUNDATION_BLEND),assets=assets,
   groups=sum(len(a['groups']) for a in assets),columns=sum(a['source_regions'] for a in assets),
   placement='Native local XY uses verified source-to-native basis. Unit column top is0m, bottom-1m. Translate top to body base plus original source_support_top_z_m; stretch ONLY vertical depth beneath terrain. Never recenter XY or use uniform mesh scale.',
   overlap_policy='All individual contact columns preserved, including positive XY overlaps, coplanar top faces and coincident/touching boundaries. Measured positive-area pairs are flagged per asset; no union or invented pad.',
   holes_policy='Caps are original measured contact triangles, not bounding boxes or new triangulated hulls. Closed boundary side walls preserve all9 holes.'),
  wind=dict(source=relative(WIND_BLEND),source_sha256=sha(WIND_BLEND),source_issue=168,version=1,
   yaw_pivot_source_m=pivot,yaw_axis_source=[0,0,1],yaw_pivot_measurement=dict(component='Yaw_Bearing_Collar',measured_bounds_m=collar['bounds'],method='Midpoint of measured collar min/max; vertical axis through actual collar centre'),
   rotor_axle_source_m=axle,rotor_axis_source=[0,1,0],rotor_offset_from_yaw_pivot_m=[a-b for a,b in zip(axle,pivot)],
   roles=role_records,stationary_components=sorted(stationary),weather_yaw_components=sorted(yaw),spinning_rotor_components=sorted(rotor),
   hierarchy='Building/entity presentation rotates stationary root and all children. Weather yaw rotates only YawNacelle about measured collar Z axis. SpinRotor is its child, offset axle-yawPivot, spinning around rotor-local source Y. Stationary mast/seams/door/access remain fixed to their path.',
   rest_reassembly='stationary_local; yaw_local+yawPivot; spin_local+rotorAxle. Apply weather yaw about yawPivot to the yaw and rotor branches only, then rotor spin in rotor-local coordinates.',
   source_truth_preserved=True,predicted_native=dict(yaw_pivot_cm=prep.native(pivot),rotor_axle_cm=prep.native(axle),rotor_offset_from_yaw_cm=prep.native([a-b for a,b in zip(axle,pivot)]),yaw_axis=[0,0,1],spin_axis=[0,-1,0],basis='[x,-y,z]*100; nuclear334 measured, new wind roles await actual native verification',reflection='Negative determinant changes angular handedness; verify weather-yaw and spin signs in native runtime, separately from entity presentation yaw.')),
  approved_geometry_changed=False,previous_whole_body_rotor_exports_changed=False,new_native_runtime_or_ci_work=False)
 write(MANIFEST,doc)
 print('UTILITY_GROUND_WIND_PREPARATION_PASS',doc['foundation']['groups'],'foundation groups',doc['foundation']['columns'],'columns',len(role_records),'wind role meshes',flush=True)
if __name__=='__main__':main()
