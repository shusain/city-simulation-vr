"""Independent saved-source/FBX validation for the offline eleven-utility batch.

No import into Unreal, render, catalog change, or re-authoring. Writes a proof
receipt beside the batch manifest. Rejects altered originals, dropped contact
contours, changed retained faces/UVs/materials or a broken wind reassembly.
"""
from pathlib import Path
from collections import Counter
import hashlib,json,math,sys
import bpy
from mathutils import Vector
from mathutils.kdtree import KDTree
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'Tools'))
from Assets.Manifests.CityApprovedSources import verified_export
MANIFEST=ROOT/'ArtSource/Manifests/city_utility_integration.json'

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def read(path):return json.loads(Path(path).read_bytes())
def q(values):return tuple(round(x,4) for x in values)
def load(path):
 with bpy.data.libraries.load(str(path),link=False) as (available,loaded):loaded.objects=list(available.objects)
 result=[]
 for obj in loaded.objects:
  if obj.type=='MESH' and not obj.name.startswith('ReviewGround'):
   bpy.context.scene.collection.objects.link(obj);result.append(obj)
 bpy.context.view_layer.update();return result
def faces(obj,offset=(0,0,0)):
 mesh=obj.data;uvs=list(mesh.uv_layers);result=Counter()
 for p in mesh.polygons:
  corners=[(q(mesh.vertices[vi].co+Vector(offset)),tuple(q(uv.data[li].uv) for uv in uvs)) for vi,li in zip(p.vertices,p.loop_indices)]
  key=min(tuple(corners[i:]+corners[:i]) for i in range(len(corners)))
  result[(mesh.materials[p.material_index].name.split('.')[0],p.use_smooth,key)]+=1
 return result
def original_snapshot(obj):
 mesh=obj.data
 return dict(vertices=[list(v.co) for v in mesh.vertices],polygons=[list(p.vertices) for p in mesh.polygons],
  indices=[p.material_index for p in mesh.polygons],smooth=[p.use_smooth for p in mesh.polygons],
  uv_names=[uv.name for uv in mesh.uv_layers],uvs=[[list(d.uv) for d in uv.data] for uv in mesh.uv_layers],
  slots=[m.name.split('.')[0] for m in mesh.materials])
def memberships(obj):
 groups={g.index:g.name for g in obj.vertex_groups};result={name:set() for name in groups.values()}
 for vertex in obj.data.vertices:
  assert len(vertex.groups)==1 and abs(vertex.groups[0].weight-1)<1e-6
  result[groups[vertex.groups[0].group]].add(vertex.index)
 return result
def compare_fbx(path,reference,expected):
 before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(path));bpy.context.view_layer.update()
 imported=[o for o in set(bpy.data.objects)-before if o.type=='MESH'];assert len(imported)==1
 obj=imported[0];obj.data.calc_loop_triangles();reference.data.calc_loop_triangles()
 assert len(obj.data.loop_triangles)==expected['triangles']
 assert [m.name.split('.')[0] for m in obj.data.materials]==[m.name.split('.')[0] for m in reference.data.materials]
 assert len(obj.data.uv_layers)==len(reference.data.uv_layers)
 # Match actual FBX face corners against source position/material/all-UV tuples.
 # KD matching tolerates FBX float precision and Blender's triangulation order.
 corners=[]
 for p in reference.data.polygons:
  for vi,li in zip(p.vertices,p.loop_indices):
   corners.append((reference.matrix_world@reference.data.vertices[vi].co,p.material_index,[Vector(uv.data[li].uv) for uv in reference.data.uv_layers]))
 kd=KDTree(len(corners))
 for i,(pos,_,_) in enumerate(corners):kd.insert(pos,i)
 kd.balance();worst=0.;checked=0
 for p in obj.data.polygons:
  for vi,li in zip(p.vertices,p.loop_indices):
   point=obj.matrix_world@obj.data.vertices[vi].co
   options=[(distance,index) for _,index,distance in kd.find_range(point,.0002) if corners[index][1]==p.material_index and all((Vector(uv.data[li].uv)-value).length<.00002 for uv,value in zip(obj.data.uv_layers,corners[index][2]))]
   assert options,('FBX position/material/UV corner differs',path.name,list(point),p.material_index)
   distance,_=min(options);worst=max(worst,distance);checked+=1
 # Reverse spatial coverage prevents losing a vertex while retaining the count.
 reverse=KDTree(len(obj.data.vertices))
 for i,v in enumerate(obj.data.vertices):reverse.insert(obj.matrix_world@v.co,i)
 reverse.balance()
 assert all(reverse.find(reference.matrix_world@v.co)[2]<.0002 for v in reference.data.vertices)
 for other in set(bpy.data.objects)-before:bpy.data.objects.remove(other,do_unlink=True)
 return dict(fbx=path.relative_to(ROOT).as_posix(),triangles=expected['triangles'],corners_checked=checked,max_position_error_m=worst,all_uv_layers_and_material_corners_match=True)

def main():
 doc=read(MANIFEST);records=[];totalcontacts=0;totalportals=0;fbxchecks=0
 for a in doc['assets']:
  bpy.ops.wm.read_factory_settings(use_empty=True)
  source=ROOT/a['export_directory'];verified_export(source,a['issue'],a['version'],a['approval_sha256'],kind='blender')
  assert sha(source/'asset.blend')==a['source_sha256'] and sha(source/'asset.fbx')==a['fbx_sha256']
  original=load(source/'asset.blend');assert len(original)==1
  snap=original_snapshot(original[0]);original[0].name='OriginalReference'
  assert sha(ROOT/a['prepared_source'])==a['prepared_source_sha256']
  prepared=load(ROOT/a['prepared_source']);lookup={o.name:o for o in prepared}
  base=lookup[a['lods'][0]['object']];assert original_snapshot(base)==snap
  group=memberships(base);assert set(group)=={p['name'] for p in a['components']}
  # Every retained derivative face is an exact source face (including winding,
  # all UV layers, semantic region and smooth/flat flag).
  sourcefaces=faces(base);details=[]
  for lod in a['lods']:
   obj=lookup[lod['object']];obj.hide_set(False);bpy.context.view_layer.update()
   obj.data.calc_loop_triangles();assert len(obj.data.loop_triangles)==lod['triangles']
   assert sha(ROOT/lod['export'])==lod['export_sha256']
   if lod['lod']:
    assert not (faces(obj)-sourcefaces)
    assert set(memberships(obj))==set(group)-set(lod['omitted_components'])
    assert set(lod['omitted_components']).isdisjoint({r['component'] for r in a['support_regions']})
   details.append(compare_fbx(ROOT/lod['export'],obj,lod));fbxchecks+=1
  measured_contacts=[]
  for region in a['support_regions']:
   ids=group[region['component']];verts={q(base.data.vertices[i].co) for i in ids}
   contact=region['contact'];plane=contact['plane_z_m'];assert abs(plane-region['support_top_z_m'])<.0002
   expected={q(base.data.vertices[i].co) for i in ids if abs(base.data.vertices[i].co.z-plane)<.0002}
   assert expected=={q(v) for v in contact['contact_vertices_m']}
   edgekeys={tuple(sorted((q(base.data.vertices[e.vertices[0]].co),q(base.data.vertices[e.vertices[1]].co)))) for e in base.data.edges if set(e.vertices)<=ids}
   for loop in contact['loops']:
    points=loop['vertices_m'];assert len(points)>=3 and all(q(p) in verts for p in points)
    assert all(tuple(sorted((q(p),q(r)))) in edgekeys for p,r in zip(points,points[1:]+points[:1]))
    for point,predicted in zip(points,loop['predicted_native_vertices_cm']):assert max(abs(x-y) for x,y in zip(predicted,[100*point[0],-100*point[1],100*point[2]]))<.00001
   for lod in a['lods'][1:]:
    other=lookup[lod['object']];vg=memberships(other)[region['component']]
    assert expected<={q(other.data.vertices[i].co) for i in vg}
   measured_contacts.append(dict(component=region['component'],loops=len(contact['loops']),holes=sum(x['hole'] for x in contact['loops']),vertices=len(expected)))
  # Rerun clearance from the saved mesh's real component vertices, not its report.
  for portal in a['portals']:
   lane=portal['source_anchor']['reserved_free_corridor'];hits=[]
   for name,ids in group.items():
    lo=[min(base.data.vertices[i].co[k] for i in ids) for k in range(3)];hi=[max(base.data.vertices[i].co[k] for i in ids) for k in range(3)]
    if all(min(hi[k],lane['max'][k])-max(lo[k],lane['min'][k])>.0002 for k in range(3)):hits.append(name)
   assert not hits and portal['source_anchor']['target'] in group
   target=group[portal['source_anchor']['target']]
   assert abs(min(base.data.vertices[i].co.z for i in target)-portal['actual_sill_z_m'])<.0002
  if 'wind' in a:
   wind=a['wind'];assert wind['axle_xyz_m']==[0,-2.9,22] and wind['source_axis']==[0,1,0]
   for lod in a['lods']:
    pieces=[r for r in wind['static_split_lods'] if r['lod']==lod['lod']];combined=Counter()
    for piece in pieces:
     obj=lookup[piece['object']];combined+=faces(obj,piece['local_offset_m'])
     assert sha(ROOT/piece['export'])==piece['export_sha256']
     details.append(compare_fbx(ROOT/piece['export'],obj,piece));fbxchecks+=1
    assert combined==faces(lookup[lod['object']]),'Wind split does not reassemble exact rest geometry'
  if 'solar' in a:
   solar=a['solar'];tracking=read(ROOT/solar['tracking_contract_path'])
   assert sha(ROOT/solar['tracking_contract_path'])==solar['tracking_contract_sha256']
   assert tracking==solar['tracking_contract'] and len(tracking['tables'])==12
   for table in tracking['tables']:
    assert set(table['stationary_components']+table['yaw_components']+table['pitch_components'])<=set(group)
  for texture in a['materials']['textures']:
   assert sha(ROOT/texture['exported_path'])==texture['sha256']
   assert all(sha(ROOT/p)==texture['sha256'] for p in texture['reuse_source_paths'])
  totalcontacts+=len(a['support_regions']);totalportals+=len(a['portals'])
  records.append(dict(issue=a['issue'],version=a['version'],prepared_source=a['prepared_source'],lod_triangles=[l['triangles'] for l in a['lods']],
   approved_lod0_geometry_uv_materials_unchanged=True,all_retained_lod_faces_match_source=True,all_support_contours_and_lod_contacts_pass=True,
   support_regions=measured_contacts,all_saved_mesh_portal_lanes_clear=True,fbx_roundtrip_checks=details,
   wind_split_reassembly_pass='wind' in a,solar_all12_group_pivots_retained='solar' in a))
  print('UTILITY_INDEPENDENT_VERIFIED',a['issue'],len(details),'FBXs',len(measured_contacts),'contacts',flush=True)
 assert len(records)==11 and totalcontacts==295 and totalportals==19 and fbxchecks==39
 assert all(sha(ROOT/path)==value for path,value in doc['source_artifacts_unchanged'].items())
 receipt=dict(parent_issue=157,source_manifest=MANIFEST.relative_to(ROOT).as_posix(),source_manifest_sha256=sha(MANIFEST),assets=records,
  asset_count=11,unchanged_original_blend_fbx_files=22,verified_original_artifacts=len(doc['source_artifacts_unchanged']),
  fbx_readbacks=fbxchecks,support_regions=totalcontacts,portals=totalportals,all_checks_pass=True,native_import_run=False,
  notes='FBX readbacks are Blender checks. Native handedness/contact/material/LOD/runtime acceptance remains #125. No render, build or CI run.')
 output=ROOT/'ArtSource/Manifests/city_utility_preparation_verification.json'
 output.write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8',newline='\n')
 print('ELEVEN_UTILITY_INDEPENDENT_VERIFICATION_PASS',fbxchecks,'FBX readbacks',totalcontacts,'contacts',totalportals,'portals',flush=True)
if __name__=='__main__':main()
