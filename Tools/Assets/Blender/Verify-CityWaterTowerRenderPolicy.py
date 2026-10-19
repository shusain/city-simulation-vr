"""Read-only exact collapsed-roof proof; preserves every frozen asset/count.

Writes a NEW source render-surface proof, never source geometry or old proofs.
"""
from pathlib import Path
from collections import Counter,defaultdict
import json,hashlib,math
import bpy
from io_scene_fbx import parse_fbx
ROOT=Path(__file__).resolve().parents[3]
OUTPUT=ROOT/'ArtSource/Manifests/city_water_tower_render_surface_proof.json'
def read(path):return json.loads(Path(path).read_bytes())
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def record_sha(value):return hashlib.sha256(json.dumps(value,sort_keys=True).encode()).hexdigest()
def load(path):
 bpy.ops.wm.read_factory_settings(use_empty=True)
 with bpy.data.libraries.load(str(path),link=False) as (available,loaded):loaded.objects=list(available.objects)
 for obj in loaded.objects:
  if obj.type=='MESH':bpy.context.scene.collection.objects.link(obj)
 bpy.context.view_layer.update();return {o.name:o for o in loaded.objects if o.type=='MESH'}

def triangle_area(points):
 a,b,c=points;u=[b[k]-a[k] for k in range(3)];v=[c[k]-a[k] for k in range(3)]
 cross=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
 return math.sqrt(sum(x*x for x in cross))*.5

def raw_roof_proof(path):
 tree,version=parse_fbx.parse(str(path));objects=next(e for e in tree.elems if e.id==b'Objects')
 meshes=[e for e in objects.elems if e.id==b'Geometry' and e.props[-1]==b'Mesh'];assert len(meshes)==1
 geo=meshes[0];children={e.id:e for e in geo.elems};v=list(children[b'Vertices'].props[0]);vertices=[v[i:i+3] for i in range(0,len(v),3)]
 polygons=[];current=[]
 for index in children[b'PolygonVertexIndex'].props[0]:
  current.append(int(index if index>=0 else -index-1))
  if index<0:polygons.append(current);current=[]
 assert not current
 layer=next(e for e in geo.elems if e.id==b'LayerElementMaterial');materials=list(next(e for e in layer.elems if e.id==b'Materials').props[0])
 assert next(e for e in layer.elems if e.id==b'MappingInformationType').props[0]==b'ByPolygon'
 collapsed=[]
 for index,indices in enumerate(polygons):
  points=[vertices[i] for i in indices];unique=len(set(map(tuple,points)))
  if unique==len(indices):continue
  assert len(set(indices))==len(indices) and materials[index]==0
  assert (172<=index<=199 and len(indices)==4 and unique==3) or (index==201 and len(indices)==28 and unique==1)
  collapsed.append(dict(polygon=index,corners=len(indices),unique_positions=unique,material_index=materials[index],indices=indices,positions_file_units=points,
   zero_triangle_contribution=1 if unique==3 else 26))
 assert [p['polygon'] for p in collapsed]==list(range(172,200))+[201]
 assert sum(p['zero_triangle_contribution'] for p in collapsed)==54
 return dict(version=version,geometry_count=1,collapsed_polygons=collapsed,exact_zero_triangle_contribution=54,n_minus2_raw_triangles=sum(len(p)-2 for p in polygons))

def main():
 assert not OUTPUT.exists(),'New proof only; do not overwrite delivered evidence'
 plan_path=ROOT/'ArtSource/Manifests/city_utility_native_import_plan.json';plan=read(plan_path);record=next(m for m in plan['whole_meshes'] if m['key']=='water_tower')
 base=read(ROOT/'ArtSource/Manifests/city_utility_integration.json');asset=next(a for a in base['assets'] if a['issue']==167)
 protected=read(ROOT/'ArtSource/Manifests/city_solar_structural_foundation_override.json')['protected_files_unchanged'];assert all(sha(ROOT/p)==pin for p,pin in protected.items())
 objects=load(ROOT/asset['prepared_source']);rows=[]
 for lod in asset['lods']:
  original=record['lods'][lod['lod']];assert sha(ROOT/lod['export'])==lod['export_sha256']
  obj=objects[lod['object']];mesh=obj.data;mesh.calc_loop_triangles();materials=[m.name for m in mesh.materials]
  counts=Counter(materials[t.material_index] for t in mesh.loop_triangles);assert dict(counts)==original['facts']['triangles_by_material']
  assert len(mesh.loop_triangles)==lod['triangles'];zero=[];valid_loops=set();zero_loops=set();corner_triangles=defaultdict(list)
  memberships={v.index:{obj.vertex_groups[g.group].name for g in v.groups if g.weight>.5} for v in mesh.vertices}
  for index,triangle in enumerate(mesh.loop_triangles):
   points=[list(mesh.vertices[vi].co) for vi in triangle.vertices];area=triangle_area(points)
   for li in triangle.loops:corner_triangles[int(li)].append(index)
   if area!=0:valid_loops.update(triangle.loops);continue
   zero_loops.update(triangle.loops);component=set.intersection(*(memberships[vi] for vi in triangle.vertices));assert component=={'Tank_roof'}
   assert materials[triangle.material_index]=='Paint_BlueGrey' and len(set(map(tuple,points)))<3 and len(set(triangle.vertices))==3
   zero.append(dict(triangle=index,polygon=triangle.polygon_index,indices=list(triangle.vertices),loops=list(triangle.loops),positions_m=points,area_m2=0.,component='Tank_roof',material='Paint_BlueGrey'))
  assert len(zero)==54 and Counter(z['polygon'] for z in zero)==Counter({**{i:1 for i in range(172,200)},201:26})
  corner_lookup=defaultdict(list)
  for polygon in mesh.polygons:
   for vi,li in zip(polygon.vertices,polygon.loop_indices):
    key=(materials[polygon.material_index],tuple(mesh.vertices[vi].co),tuple(mesh.corner_normals[li].vector),tuple(mesh.uv_layers[0].data[li].uv))
    corner_lookup[key].append(int(li))
  coverage=[];omitted=[];kept={}
  for material,samples in original['facts']['samples'].items():
   kept[material]=[]
   for index,sample in enumerate(samples):
    key=(material,tuple(sample['position_m']),tuple(sample['normal']),tuple(sample['uv0']));loops=corner_lookup[key];assert loops,('No exact saved-source corner',lod['lod'],material,index)
    valid=[li for li in loops if li in valid_loops];only_zero=not valid and all(li in zero_loops for li in loops)
    entry=dict(material=material,source_sample_index=index,sample=sample,matching_source_loops=loops,matching_valid_face_loops=valid,matching_source_triangles=sorted({t for li in loops for t in corner_triangles[li]}),exclusively_zero_area=only_zero)
    coverage.append(entry)
    if only_zero:omitted.append(entry)
    else:assert valid;kept[material].append(sample)
  assert [(r['material'],r['source_sample_index']) for r in omitted]==([('Paint_BlueGrey',15)] if lod['lod']==2 else [])
  native_counts=dict(counts);native_counts['Paint_BlueGrey']-=54;binary=raw_roof_proof(ROOT/lod['export']);assert binary['n_minus2_raw_triangles']==lod['triangles']
  bpy.ops.import_scene.fbx(filepath=str(ROOT/lod['export']));bpy.context.view_layer.update()
  imported=[o for o in bpy.context.scene.objects if o.type=='MESH' and o.name not in objects];assert len(imported)==1
  imported[0].data.calc_loop_triangles();fbx_counts=Counter(imported[0].data.materials[t.material_index].name.split('.')[0] for t in imported[0].data.loop_triangles)
  fbx_zero=Counter()
  for t in imported[0].data.loop_triangles:
   if triangle_area([list(imported[0].data.vertices[vi].co) for vi in t.vertices])==0:fbx_zero[imported[0].data.materials[t.material_index].name.split('.')[0]]+=1
  assert dict(fbx_counts)==dict(counts) and dict(fbx_zero)=={'Paint_BlueGrey':54}
  bpy.data.objects.remove(imported[0],do_unlink=True)
  rows.append(dict(lod=lod['lod'],export=lod['export'],export_sha256=lod['export_sha256'],original_facts_sha256=record_sha(original['facts']),raw_source_triangles=lod['triangles'],raw_triangles_by_material=dict(counts),
   exact_zero_triangles=zero,exact_zero_triangles_by_material={'Paint_BlueGrey':54},expected_render_triangles=lod['triangles']-54,expected_render_triangles_by_material=native_counts,
   raw_binary_fbx=binary,actual_blender_fbx_counts=dict(fbx_counts),actual_blender_fbx_zero_area_counts=dict(fbx_zero),original_probe_coverage=coverage,omitted_degenerate_only_probes=omitted,renderable_samples=kept,
   retained_probe_count=sum(map(len,kept.values())),source_triangle_counts_not_rewritten=True,source_export_unchanged=True))
 proof=dict(task=157,source_issue=167,version=3,native_owner=125,original_plan_sha256=sha(plan_path),prepared_source=asset['prepared_source'],prepared_source_sha256=sha(ROOT/asset['prepared_source']),
  lods=rows,protected_files_unchanged=protected,original_source_preparation_and_proof_bytes_unchanged=True,source_geometry_or_normals_uvs_edited=False,native_execution=False,
  policy='Exact54 collapsed Tank_roof/Paint_BlueGrey triangles perLOD only. Exclude onlyLOD2 BlueGrey probe15 proven to belong exclusively to zero-area source faces; all143 renderable probes are retained unchanged.')
 assert [r['expected_render_triangles'] for r in rows]==[4170,3482,2490] and sum(r['retained_probe_count'] for r in rows)==143
 assert all(sha(ROOT/p)==pin for p,pin in protected.items())
 OUTPUT.write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8',newline='\n')
 print('WATER_TOWER_RENDER_SOURCE_PROOF_PASS exact54/LOD,143 unchanged renderable probes,1 exclusively degenerate source probe;no assets/native execution',flush=True)
if __name__=='__main__':main()
