"""Read saved column/three-role derivatives; prove source footprint and normals.

This is offline Blender FBX verification, not native or runtime acceptance.
"""
from pathlib import Path
from collections import Counter
import importlib.util,json,hashlib,math
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree
ROOT=Path(__file__).resolve().parents[3]
MANIFEST=ROOT/'ArtSource/Manifests/city_utility_ground_wind_preparation.json'
PROOF=ROOT/'ArtSource/Manifests/city_utility_ground_wind_verification.json'
spec=importlib.util.spec_from_file_location('utility_verify',ROOT/'Tools/Assets/Blender/Verify-CityUtilityIntegration.py')
old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old)
read,sha=old.read,old.sha

def corner_records(obj,offset=(0,0,0)):
 mesh=obj.data;mesh.calc_loop_triangles();normal_matrix=obj.matrix_world.to_3x3().inverted().transposed();result=[]
 for p in mesh.polygons:
  for vi,li in zip(p.vertices,p.loop_indices):
   result.append((obj.matrix_world@mesh.vertices[vi].co+Vector(offset),mesh.materials[p.material_index].name.split('.')[0],
    [Vector(uv.data[li].uv) for uv in mesh.uv_layers],(normal_matrix@mesh.corner_normals[li].vector).normalized()))
 return result

def matches(reference,candidate,label):
 kd=KDTree(len(reference))
 for i,r in enumerate(reference):kd.insert(r[0],i)
 kd.balance();position=normal=uv=0.
 for p,material,values,n in candidate:
  options=[]
  for _,index,distance in kd.find_range(p,.0002):
   r=reference[index]
   if r[1]!=material or len(r[2])!=len(values):continue
   uverror=max(((a-b).length for a,b in zip(values,r[2])),default=0.);nerror=(n-r[3]).length
   if uverror<.00002 and nerror<.0002:options.append((distance,nerror,uverror))
  assert options,('Corner position/UV/material/normal mismatch',label,list(p),material,list(n))
  pe,ne,ue=min(options);position=max(position,pe);normal=max(normal,ne);uv=max(uv,ue)
 return dict(corners=len(candidate),max_position_error_m=position,max_normal_vector_error=normal,max_uv_error=uv)

def fbx_check(path,reference,triangles):
 before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(path));bpy.context.view_layer.update()
 objects=[o for o in set(bpy.data.objects)-before if o.type=='MESH'];assert len(objects)==1
 imported=objects[0];imported.data.calc_loop_triangles();assert len(imported.data.loop_triangles)==triangles
 assert [m.name.split('.')[0] for m in imported.data.materials]==[m.name.split('.')[0] for m in reference.data.materials]
 result=matches(corner_records(reference),corner_records(imported),path.name)
 for obj in set(bpy.data.objects)-before:bpy.data.objects.remove(obj,do_unlink=True)
 return dict(fbx=path.relative_to(ROOT).as_posix(),triangles=triangles,**result)

def triangle_key(points):return tuple(sorted(tuple(round(x,7) for x in p[:2]) for p in points))

def foundation_check(obj,column,region):
 m=obj.data;m.calc_loop_triangles();ids=set(range(column['vertex_start'],column['vertex_start']+column['vertex_count']))
 polygons=list(m.polygons)[column['face_start']:column['face_start']+column['face_count']]
 assert {round(m.vertices[i].co.z,6) for i in ids}=={-1.,0.}
 edges=Counter(tuple(sorted(e)) for p in polygons for e in p.edge_keys)
 assert all(n==2 for n in edges.values())
 half=Counter((a,b) for p in polygons for a,b in zip(list(p.vertices),list(p.vertices)[1:]+list(p.vertices)[:1]))
 assert all(half[(b,a)]==n for (a,b),n in half.items())
 top=[t for t in m.loop_triangles if set(t.vertices)<=ids and all(abs(m.vertices[i].co.z)<1e-7 for i in t.vertices)]
 expected=Counter(triangle_key(t) for t in region['contact']['horizontal_face_triangles_m'])
 assert Counter(triangle_key([m.vertices[i].co for i in t.vertices]) for t in top)==expected
 bottom=[t for t in m.loop_triangles if set(t.vertices)<=ids and all(abs(m.vertices[i].co.z+1)<1e-7 for i in t.vertices)]
 assert Counter(triangle_key([m.vertices[i].co for i in t.vertices]) for t in bottom)==expected
 volume=0.;area=0.
 for t in m.loop_triangles:
  if set(t.vertices)<=ids:
   a,b,c=[m.vertices[i].co for i in t.vertices];volume+=a.dot(b.cross(c))/6
 for t in top:
  a,b,c=[m.vertices[i].co for i in t.vertices];normal=(b-a).cross(c-a);assert normal.z>0;area+=normal.length*.5
 for t in bottom:
  a,b,c=[m.vertices[i].co for i in t.vertices];assert (b-a).cross(c-a).z<0
 assert abs(volume-area)<max(.0002,area*2e-5) and volume>0
 for p in polygons:
  assert math.isfinite(p.normal.length) and abs(p.normal.length-1)<1e-5
 assert all(math.isfinite(x) for uv in m.uv_layers for p in polygons for li in p.loop_indices for x in uv.data[li].uv)
 tree=BVHTree.FromPolygons([v.co for v in m.vertices],[list(p.vertices) for p in polygons],all_triangles=False)
 holes=[]
 for loop in region['contact']['loops']:
  if not loop['hole']:continue
  centre=Vector((sum(v[0] for v in loop['vertices_m'])/len(loop['vertices_m']),sum(v[1] for v in loop['vertices_m'])/len(loop['vertices_m']),1.))
  location,*_=tree.ray_cast(centre,Vector((0,0,-1)),3.)
  assert location is None,('Hole filled',column['component'],list(centre))
  holes.append(list(centre))
 return dict(component=column['component'],closed_edges=len(edges),signed_volume_m3=volume,cap_area_m2=area,
  source_top_z_m=column['source_top_z_m'],original_contact_triangles_exact=True,unit_planes_m=[-1,0],hole_ray_probes=holes)

def main():
 doc=read(MANIFEST);base=read(ROOT/doc['base_manifest']['path']);assert sha(ROOT/doc['base_manifest']['path'])==doc['base_manifest']['sha256']
 assert all(sha(ROOT/p)==value for p,value in doc['protected_original_and_reference_files'].items())
 assert sha(ROOT/doc['foundation']['source'])==doc['foundation']['source_sha256']
 bpy.ops.wm.read_factory_settings(use_empty=True);foundation=old.load(ROOT/doc['foundation']['source']);lookup={o.name:o for o in foundation}
 assets=[];total=holes=0;checks=[]
 for a in doc['foundation']['assets']:
  source=next(s for s in base['assets'] if s['issue']==a['issue']);regions={r['component']:r for r in source['support_regions']};seen=set();groups=[]
  for group in a['groups']:
   obj=lookup[group['object']];obj.hide_set(False);bpy.context.view_layer.update()
   assert sha(ROOT/group['export'])==group['export_sha256']
   measurements=[]
   for column in group['columns']:
    region=regions[column['component']];assert column['component'] not in seen;seen.add(column['component'])
    assert column['source_top_z_m']==group['source_support_top_z_m']==region['support_top_z_m']
    measurement=foundation_check(obj,column,region);measurements.append(measurement);total+=1;holes+=len(measurement['hole_ray_probes'])
   checks.append(fbx_check(ROOT/group['export'],obj,group['triangles']))
   groups.append(dict(object=obj.name,source_support_top_z_m=group['source_support_top_z_m'],columns=measurements))
  assert seen==set(regions)
  overlaparea=sum(p['overlap_area_xy_m2'] for p in a['preserved_overlap_pairs']);assert all(p['overlap_area_xy_m2']>1e-8 and p['first'] in regions and p['second'] in regions for p in a['preserved_overlap_pairs'])
  assets.append(dict(issue=a['issue'],groups=groups,flagged_positive_area_overlap_pairs=len(a['preserved_overlap_pairs']),sum_flagged_pairwise_overlap_m2=overlaparea))
  print('FOUNDATION_INDEPENDENT_VERIFIED',a['issue'],len(seen),'closed columns',flush=True)
 # Compare saved three-role wind meshes with exact approved source normals,
 # and all three reassembled whole LOD face/UV/material sets.
 bpy.ops.wm.read_factory_settings(use_empty=True);wind=doc['wind'];assert sha(ROOT/wind['source'])==wind['source_sha256']
 meshes=old.load(ROOT/wind['source']);lookup={o.name:o for o in meshes};approved=lookup['WindApprovedReference'];reference=corner_records(approved)
 groups=old.memberships(approved);collar=[approved.data.vertices[i].co for i in groups['Yaw_Bearing_Collar']]
 measured=[(min(v[k] for v in collar)+max(v[k] for v in collar))*.5 for k in range(3)]
 assert max(abs(a-b) for a,b in zip(measured,wind['yaw_pivot_source_m']))<1e-7
 # The authored collar has multiple concentric profile radii, not one cylinder.
 rings={}
 for v in collar:rings.setdefault((round(v.z,5),round(math.hypot(v.x,v.y),5)),[]).append(v)
 ringproof=[]
 for (z,radius),points in rings.items():
  assert abs(sum(v.x for v in points)/len(points))<.00001 and abs(sum(v.y for v in points)/len(points))<.00001,'Collar profile is not centred on measured Z axis'
  assert radius<.00001 or len(points)>=8
  ringproof.append(dict(z_m=z,radius_m=radius,vertices=len(points)))
 assert len(ringproof)>=2
 sourcewind=next(a for a in base['assets'] if a['issue']==168)
 original_lods=old.load(ROOT/sourcewind['prepared_source']);whole={o.name.split('.')[0]:o for o in original_lods}
 roleproof=[]
 for lod in sourcewind['lods']:
  pieces=[r for r in wind['roles'] if r['lod']==lod['lod']];combined=Counter();names=set()
  assert {p['role'] for p in pieces}=={'Stationary','YawNacelle','SpinRotor'}
  for piece in pieces:
   obj=lookup[piece['object']];obj.hide_set(False);bpy.context.view_layer.update();assert sha(ROOT/piece['export'])==piece['export_sha256']
   normalproof=matches(reference,corner_records(obj,piece['source_pivot_m']),piece['object'])
   combined+=old.faces(obj,piece['source_pivot_m']);assert names.isdisjoint(piece['component_names']);names.update(piece['component_names'])
   checks.append(fbx_check(ROOT/piece['export'],obj,piece['triangles']));roleproof.append(dict(object=piece['object'],role=piece['role'],lod=piece['lod'],source_corner_match=normalproof))
  assert combined==old.faces(whole[lod['object']]),('Three roles fail source rest reassembly',lod['lod'])
  assert names==set(groups)-set(lod.get('omitted_components',[]))
  stationary=next(p for p in pieces if p['role']=='Stationary')
  assert {'Mast_Tapered_Shell','Door_Flush_Panel','Door_Metal_Rim'}<=set(stationary['component_names'])
 assert total==295 and holes==9 and len(checks)==21 and len(roleproof)==9
 assert all(sha(ROOT/p)==value for p,value in doc['protected_original_and_reference_files'].items())
 receipt=dict(parent_issue=157,extension_manifest=MANIFEST.relative_to(ROOT).as_posix(),extension_manifest_sha256=sha(MANIFEST),extends_head=doc['extends_local_preparation_head'],
  foundation_assets=assets,foundation_groups=12,closed_unit_columns=total,preserved_holes=holes,
  actual_fbx_readbacks=checks,wind_role_proof=roleproof,wind_exact_source_reassembly_all3_lods=True,wind_positions_uvs_materials_normals_match=True,
  measured_source_yaw_pivot_m=measured,measured_concentric_collar_profile=ringproof,wind_mast_and_ground_door_stationary_all3_lods=True,
  all_original_and_reference_bytes_unchanged=True,all_checks_pass=True,native_import_run=False,ci_run=False,
  pending='Native #125 import/stock concrete/material/basis verification, foundation depth placement, weather yaw/spin runtime adaptation and Windows playtest remain next.')
 PROOF.write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8',newline='\n')
 print('UTILITY_GROUND_WIND_INDEPENDENT_VERIFICATION_PASS',len(checks),'FBXs',total,'closed columns',holes,'holes,9 wind role meshes',flush=True)
if __name__=='__main__':main()
