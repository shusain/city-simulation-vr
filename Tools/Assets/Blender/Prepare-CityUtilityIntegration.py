"""#157: exact eleven utility sources -> offline LOD/contact/portal handoff.

Run Blender --background --factory-startup --disable-autoexec --threads 2
--python-exit-code 1 --python <this file>. Never invokes Unreal or renders.
Writes only this batch's derived sources/exports/manifests. Approved LOD0
files remain immutable. A fresh output destination is required.
"""
from pathlib import Path
from collections import Counter,defaultdict
import hashlib,json,math,re,sys
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'Tools'))
from CityBuildingStudyIO import checked_fbx
from Assets.Manifests.CityApprovedSources import verified_export
INPUT=ROOT/'ArtSource/Manifests/city_utility_preparation_inputs.json'
OUT=ROOT/'ArtSource/Exports/UtilityIntegration'
BLENDS=ROOT/'ArtSource/Blender/UtilityIntegration'
MANIFEST=ROOT/'ArtSource/Manifests/city_utility_integration.json'
EPS=.0002

def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def relative(path):return Path(path).relative_to(ROOT).as_posix()
def read(path):return json.loads(Path(path).read_bytes())
def write(path,data):Path(path).write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8',newline='\n')
def bounds(points):return dict(min=[min(p[k] for p in points) for k in range(3)],max=[max(p[k] for p in points) for k in range(3)])
def native(point):return [point[0]*100,-point[1]*100,point[2]*100]
def scalar(value):
 if isinstance(value,(str,bool,int,float)):return value
 try:return list(value)
 except TypeError:return str(value)

def load_source(path):
 bpy.ops.wm.read_factory_settings(use_empty=True)
 bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
 with bpy.data.libraries.load(str(path),link=False) as (available,loaded):loaded.objects=list(available.objects)
 objects=[o for o in loaded.objects if o.type=='MESH' and not o.name.startswith('ReviewGround')]
 assert len(objects)==1,[(o.name,o.type) for o in loaded.objects]
 obj=objects[0];bpy.context.scene.collection.objects.link(obj);bpy.context.view_layer.update()
 assert all(abs(x)<1e-6 for x in list(obj.location)+list(obj.rotation_euler)) and all(abs(x-1)<1e-6 for x in obj.scale)
 return obj

def parts_of(obj,components):
 mesh=obj.data;parent=list(range(len(mesh.vertices)))
 def find(i):
  while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
  return i
 for e in mesh.edges:
  a,b=e.vertices;parent[find(a)]=find(b)
 groups=defaultdict(list)
 for v in mesh.vertices:groups[find(v.index)].append(v.index)
 remaining=list(components);parts=[]
 for ids in groups.values():
  measured=bounds([mesh.vertices[i].co for i in ids])
  errors=sorted((max(abs(x-y) for x,y in zip(measured['min']+measured['max'],c['min']+c['max'])),i) for i,c in enumerate(remaining))
  assert errors and errors[0][0]<EPS,(obj.name,measured,errors[:1])
  component=remaining.pop(errors[0][1]);idset=set(ids)
  polys=[p for p in mesh.polygons if p.vertices[0] in idset]
  assert polys and all(set(p.vertices)<=idset for p in polys)
  parts.append(dict(name=component['name'],ids=ids,polys=polys,bounds=measured))
 assert not remaining and len(parts)==len(components),(len(groups),len(components),remaining[:1])
 assert len({p['name'] for p in parts})==len(parts)
 return sorted(parts,key=lambda p:next(i for i,c in enumerate(components) if c['name']==p['name']))

def snapshot(obj):
 m=obj.data
 return dict(vertices=[list(v.co) for v in m.vertices],faces=[list(p.vertices) for p in m.polygons],
  materials=[p.material_index for p in m.polygons],smooth=[p.use_smooth for p in m.polygons],
  uvs={uv.name:[list(d.uv) for d in uv.data] for uv in m.uv_layers},slots=[x.name for x in m.materials])

def subset(obj,parts,name,offset=(0,0,0)):
 old=obj.data;ids=[i for part in parts for i in part['ids']];mapping={i:k for k,i in enumerate(ids)}
 polys=[p for part in parts for p in part['polys']]
 mesh=bpy.data.meshes.new(name)
 mesh.from_pydata([tuple(old.vertices[i].co-Vector(offset)) for i in ids],[],[[mapping[i] for i in p.vertices] for p in polys]);mesh.update()
 for mat in old.materials:mesh.materials.append(mat)
 for p,source in zip(mesh.polygons,polys):p.material_index=source.material_index;p.use_smooth=source.use_smooth
 for olduv in old.uv_layers:
  uv=mesh.uv_layers.new(name=olduv.name)
  for p,source in zip(mesh.polygons,polys):
   for li,oldli in zip(p.loop_indices,source.loop_indices):uv.data[li].uv=olduv.data[oldli].uv
 mesh.uv_layers.active_index=old.uv_layers.active_index
 result=bpy.data.objects.new(name,mesh);bpy.context.scene.collection.objects.link(result)
 for part in parts:result.vertex_groups.new(name=part['name']).add([mapping[i] for i in part['ids']],1.,'REPLACE')
 return result

def contours(mesh,part,z):
 ids={i for i in part['ids'] if abs(mesh.vertices[i].co.z-z)<EPS}
 assert ids,(part['name'],z)
 faces=[p for p in part['polys'] if set(p.vertices)<=ids]
 counts=Counter(tuple(sorted(edge)) for p in faces for edge in p.edge_keys)
 edges={edge for edge,count in counts.items() if count==1}
 mode='horizontal_mesh_faces'
 if not faces:
  edges={tuple(sorted(edge)) for p in part['polys'] for edge in p.edge_keys if set(edge)<=ids}
  mode='open_shell_lower_boundary'
 assert edges,(part['name'],mode,z)
 adjacency=defaultdict(set)
 for a,b in edges:adjacency[a].add(b);adjacency[b].add(a)
 assert all(len(values)==2 for values in adjacency.values()),(part['name'],'non-loop contact',[(i,len(v)) for i,v in adjacency.items() if len(v)!=2])
 remaining=set(edges);loops=[]
 while remaining:
  first,nxt=min(remaining);loop=[first];previous=first;current=nxt;remaining.remove(tuple(sorted((first,nxt))))
  while current!=first:
   loop.append(current);following=next(i for i in adjacency[current] if i!=previous)
   edge=tuple(sorted((current,following)));assert edge in remaining;remaining.remove(edge)
   previous,current=current,following
  loops.append([list(mesh.vertices[i].co) for i in loop])
 def inside(p,poly):
  yes=False
  for a,b in zip(poly,poly[1:]+poly[:1]):
   if (a[1]>p[1])!=(b[1]>p[1]) and p[0]<(b[0]-a[0])*(p[1]-a[1])/(b[1]-a[1])+a[0]:yes=not yes
  return yes
 result=[]
 for i,loop in enumerate(loops):
  depth=sum(inside(loop[0],other) for j,other in enumerate(loops) if j!=i)
  area=abs(sum(a[0]*b[1]-b[0]*a[1] for a,b in zip(loop,loop[1:]+loop[:1])))/2
  result.append(dict(vertices_m=loop,predicted_native_vertices_cm=[native(v) for v in loop],nesting_depth=depth,hole=bool(depth%2),area_m2=area))
 mesh.calc_loop_triangles();faceids={p.index for p in faces}
 triangles=[[list(mesh.vertices[i].co) for i in tri.vertices] for tri in mesh.loop_triangles if tri.polygon_index in faceids]
 return dict(method=mode,plane_z_m=z,loops=result,horizontal_face_triangles_m=triangles,
  contact_area_m2=sum((-1 if loop['hole'] else 1)*loop['area_m2'] for loop in result),
  contact_vertices_m=[list(mesh.vertices[i].co) for i in sorted(ids)],
  policy='Retain actual loops and holes; never replace curved or hollow contacts with a filled AABB.')

def ground_supports(obj,parts,issue):
 result=[]
 for part in parts:
  z=part['bounds']['min'][2];name=part['name'];normalized=re.sub('[^a-z0-9]','',name.lower())
  equipment_under=issue==158 and re.fullmatch(r'Transformer_Tank(?:\.\d+)?',name)
  if abs(z)>EPS and not equipment_under:continue
  kind='equipment_under_support' if equipment_under else 'structural_ground_contact'
  if any(token in normalized for token in ('door','shutter','jamb','portalside')):kind='portal_sill_or_trim_contact'
  if any(token in normalized for token in ('groundduct','groundfeed','mastcableriser')):kind='cable_ground_contact'
  result.append(dict(component=name,kind=kind,bounds_m=part['bounds'],support_top_z_m=z,
   contact=contours(obj.data,part,z),native_observation='Not imported; candidate basis only'))
 assert result
 return result

def portals(parts,source):
 lookup={p['name']:p for p in parts};records=[]
 for anchor in read(source/'entrance-anchors.json')['anchors']:
  assert anchor['target'] in lookup
  corridor=anchor['reserved_free_corridor'];assert corridor['max'][1]<lookup[anchor['target']]['bounds']['min'][1]+.01
  overlaps=[p['name'] for p in parts if all(min(p['bounds']['max'][k],corridor['max'][k])-max(p['bounds']['min'][k],corridor['min'][k])>EPS for k in range(3))]
  assert not overlaps,(anchor['target'],overlaps)
  target=lookup[anchor['target']];fixtures=anchor.get('portal_components',[])
  for fixture in fixtures:
   p=lookup[fixture['name']]
   assert max(abs(x-y) for x,y in zip(p['bounds']['min']+p['bounds']['max'],fixture['min']+fixture['max']))<EPS
  records.append(dict(source_anchor=anchor,actual_portal_bounds_m=target['bounds'],actual_sill_z_m=target['bounds']['min'][2],
   source_grade_m=anchor.get('grade_m',0),kind='ladder_access' if 'ladder' in anchor['target'].lower() else 'service_bay' if anchor['role']=='vehicle_service' else 'personnel_or_maintenance_door',
   actual_fixture_components=[dict(component=f['name'],bounds_m=lookup[f['name']]['bounds']) for f in fixtures] or [dict(component=anchor['target'],bounds_m=target['bounds'])],
   lane_all_components_checked=len(parts),lane_overlaps=overlaps,
   predicted_native_anchor_cm=native(anchor['xyz_m']),predicted_native_boundary_cm=native(anchor['boundary_connector']['xyz_m']),
   predicted_native_outward=[anchor['outward'][0],-anchor['outward'][1],anchor['outward'][2]],
   path_policy='Native owner joins actual sill/ladder access to terrain/public edge; lane metadata is not authored paving.'))
 return records

def omission_policy(parts,lod):
 result=[];ordinary=[]
 envelope=bounds([Vector(p['bounds'][side]) for p in parts for side in ('min','max')])
 tokens=('seam','louver','louvre','flange','radiatorfin','coolingfin','insulator','clamp','handwheel','handle','latch','flexiblepitchloop','yawserviceloop','ventslat')
 for part in parts:
  name=part['name'];compact=re.sub('[^a-z0-9]','',name.lower())
  if part['bounds']['min'][2]<EPS:continue
  # A small fixture can still own a silhouette extremum. Retain those parts.
  if any(abs(part['bounds'][side][axis]-envelope[side][axis])<EPS for side in ('min','max') for axis in range(3)):continue
  if name.startswith('Ladder_rung_'):
   if int(name.rsplit('_',1)[1])%(2 if lod==1 else 3):result.append(name)
  elif any(token in compact for token in tokens):ordinary.append(name)
 for i,name in enumerate(ordinary):
  if lod==2 or i%2:result.append(name)
 assert result,'No conservative detail omissions found'
 return result

def measure(obj):
 bpy.context.view_layer.update();m=obj.data;m.calc_loop_triangles()
 assert all(math.isfinite(x) for v in m.vertices for x in v.co)
 assert m.uv_layers and all(math.isfinite(x) for uv in m.uv_layers for d in uv.data for x in d.uv)
 return dict(triangles=len(m.loop_triangles),sections=len(m.materials),vertices=len(m.vertices),bounds_m=bounds([v.co for v in m.vertices]),dimensions_m=list(obj.dimensions))

def texture_materials(obj,source):
 contract=read(source/'material-manifest.json');textures=[]
 for path in sorted((source/'textures').glob('*')):
  if not path.is_file():continue
  common=[p for p in (ROOT/'ArtSource/Textures').rglob(path.name) if digest(p)==digest(path)]
  assert common,('No verified shared source texture',path)
  textures.append(dict(exported_path=relative(path),sha256=digest(path),reuse_source_paths=[relative(p) for p in common],
   candidate_native_texture='/Game/CitySim/Art/Textures/T_'+path.stem,
   colorspace='sRGB' if 'BaseColor' in path.name else 'Non-Color',
   channels='RGB tangent normal; native green orientation must be verified' if 'Normal' in path.name else 'R AO/G roughness/B metallic' if 'ORM' in path.name else 'RGB base color'))
 materials=[]
 for material in obj.data.materials:
  nodes=[]
  for node in material.node_tree.nodes:
   entry=dict(name=node.name,type=node.bl_idname,inputs={socket.name:scalar(socket.default_value) for socket in node.inputs if hasattr(socket,'default_value')})
   for attribute in ('operation','blend_type','data_type','noise_dimensions','voronoi_dimensions'):
    if hasattr(node,attribute):entry[attribute]=getattr(node,attribute)
   if node.type=='TEX_IMAGE' and node.image:
    image=node.image;data=bytes(image.packed_file.data) if image.packed_file else None
    assert data,'Source images must remain packed'
    imagehash=hashlib.sha256(data).hexdigest();assert any(x['sha256']==imagehash for x in textures)
    entry['image']=dict(name=image.name,filepath=image.filepath,sha256=imagehash,packed=True,colorspace=image.colorspace_settings.name,size=list(image.size))
   nodes.append(entry)
  materials.append(dict(region=material.name,diffuse_color=list(material.diffuse_color),nodes=nodes,
   links=[dict(from_node=l.from_node.name,from_socket=l.from_socket.name,to_node=l.to_node.name,to_socket=l.to_socket.name) for l in material.node_tree.links]))
 return dict(source_contract=contract,semantic_slot_order=[m.name for m in obj.data.materials],measured_source_graphs=materials,textures=textures,
  native_reconstruction='Rebuild actual recorded links/constants, UV scale, tint multiplies and Normal/ORM policy; FBX does not transfer graphs. Procedural concrete/PV patterns need explicit native reproduction or a recorded reviewed simplification; do not silently replace with flat material.')

def split_wind(obj,parts,source,outdir,stem,lod_parts):
 contract=read(source/'landmark-contract.json');axle=contract['axle_xyz_m'];axis=contract['axis']
 body=set(contract['body_component_names']);rotor=set(contract['rotor_component_names'])
 assert body.isdisjoint(rotor) and body|rotor=={p['name'] for p in parts}
 records=[]
 for lod,selected in enumerate(lod_parts):
  group=[]
  for role,names,offset in [('Body',body,[0,0,0]),('Rotor',rotor,axle)]:
   included=[p for p in selected if p['name'] in names];derived=subset(obj,included,stem+'_'+role+'_LOD'+str(lod),offset)
   path=outdir/(derived.name+'.fbx');slots=[m.name for m in obj.data.materials]
   with checked_fbx(derived,path,slots):pass
   info=measure(derived);group.append(dict(role=role,lod=lod,object=derived.name,export=relative(path),export_sha256=digest(path),component_names=[p['name'] for p in included],
    local_offset_m=offset,reassembly='body p; rotor p_local + source axle (rest pose)',roundtrip_pass=True,**info))
   derived.hide_set(True);derived.hide_render=True
  records.extend(group)
 return dict(source_contract_path=relative(source/'landmark-contract.json'),source_contract_sha256=digest(source/'landmark-contract.json'),
  axle_xyz_m=axle,source_axis=axis,predicted_native_axle_cm=native(axle),predicted_native_axis=[axis[0],-axis[1],axis[2]],
  positive_rotation_policy='Reflection changes handedness: verify native rotation sign using actual imported local blade coordinates; do not infer angular sign from axis direction alone.',
  static_split_lods=records,original_joined_lod0_preserved=True,runtime_animation_implemented=False,
  continuous_sweep_envelope=contract['analytic_full_rotor_envelope'])

def main():
 assert not OUT.exists() and not BLENDS.exists() and not MANIFEST.exists(),'Fresh batch output required'
 OUT.mkdir(parents=True);BLENDS.mkdir(parents=True)
 inputs=read(INPUT);records=[];source_hashes={}
 for a in inputs['assets']:
  source=ROOT/a['export_directory'];verified_export(source,a['issue'],a['version'],a['approval_sha256'],kind='blender')
  assert digest(source/'asset.blend')==a['source_sha256'] and digest(source/'asset.fbx')==a['fbx_sha256']
  source_hashes.update({relative(p):digest(p) for p in source.rglob('*') if p.is_file()})
  obj=load_source(source/'asset.blend');before=snapshot(obj)
  validation=read(source/'validation.json');parts=parts_of(obj,validation.get('components',validation.get('components_before_join')))
  source_measure=measure(obj);assert source_measure['triangles']==validation['triangles']
  assert all(source_measure['bounds_m']['min'][k]>=-a['site_m'][k]/2-EPS and source_measure['bounds_m']['max'][k]<=a['site_m'][k]/2+EPS for k in (0,1))
  supports=ground_supports(obj,parts,a['issue']);entrances=portals(parts,source);materials=texture_materials(obj,source)
  stem='SM_Utility_'+a['key']+'_v'+str(a['version']);outdir=OUT/a['key'];outdir.mkdir()
  obj.name=stem+'_LOD0';lods=[dict(lod=0,object=obj.name,export=relative(source/'asset.fbx'),export_sha256=digest(source/'asset.fbx'),approved_original=True,**source_measure)]
  if not obj.vertex_groups:
   for part in parts:obj.vertex_groups.new(name=part['name']).add(part['ids'],1.,'REPLACE')
  selected_lods=[parts]
  for lod in (1,2):
   omitted=omission_policy(parts,lod);selected=[p for p in parts if p['name'] not in omitted];selected_lods.append(selected)
   derived=subset(obj,selected,stem+'_LOD'+str(lod));path=outdir/(derived.name+'.fbx')
   info=measure(derived);assert info['triangles']<lods[-1]['triangles']
   assert max(abs(x-y) for x,y in zip(info['bounds_m']['min']+info['bounds_m']['max'],source_measure['bounds_m']['min']+source_measure['bounds_m']['max']))<EPS
   with checked_fbx(derived,path,materials['semantic_slot_order']):pass
   lods.append(dict(lod=lod,object=derived.name,export=relative(path),export_sha256=digest(path),omitted_components=omitted,
    retained_component_names=[p['name'] for p in selected],roundtrip_pass=True,**info))
   derived.hide_set(True);derived.hide_render=True
  extras={}
  if a['issue']==168:extras['wind']=split_wind(obj,parts,source,outdir,stem,selected_lods)
  if a['issue']==169:
   tracking=read(source/'tracking-contract.json');assert len(tracking['tables'])==12
   names={p['name'] for p in parts}
   for table in tracking['tables']:
    allnames=table['stationary_components']+table['yaw_components']+table['pitch_components'];assert len(set(allnames))==len(allnames) and set(allnames)<=names
   extras['solar']=dict(tracking_contract=tracking,tracking_contract_path=relative(source/'tracking-contract.json'),tracking_contract_sha256=digest(source/'tracking-contract.json'),
    table_count=12,all_articulation_component_groups_retained_in_prepared_lod0=True,whole_farm_static=True,runtime_tracking_or_cable_deformation_implemented=False)
  assert snapshot(obj)==before,'Approved source geometry/UV/material assignments altered in memory'
  # Remove unused library studio objects, retain exact packed source images.
  keep={o for o in bpy.context.scene.objects}
  for other in list(bpy.data.objects):
   if other not in keep:bpy.data.objects.remove(other,do_unlink=True)
  bpy.context.view_layer.objects.active=obj;obj.hide_set(False);obj.select_set(True)
  blend=BLENDS/('CityUtility_'+a['key']+'_v'+str(a['version'])+'.blend')
  bpy.ops.wm.save_as_mainfile(filepath=str(blend))
  record=dict(**a,prepared_source=relative(blend),prepared_source_sha256=digest(blend),lods=lods,
   source_front='-Y',up='Z',units='metres',pivot_m=[0,0,0],nominal_grade_m=0,cell_size_m=8,
   components=[dict(name=p['name'],bounds_m=p['bounds'],vertex_count=len(p['ids']),face_count=len(p['polys'])) for p in parts],
   support_regions=supports,portals=entrances,materials=materials,
   source_metadata={p.name:dict(path=relative(p),sha256=digest(p)) for p in source.glob('*.json')},
   upgrade_stage=read(source/'upgrade-stage.json') if (source/'upgrade-stage.json').exists() else None,
   native_contact_basis=dict(formula='source metres (x,y,z) -> candidate imported centimetres (100*x,-100*y,100*z)',
    presentation_quarter_turns=2,native_import_measured=False,basis_reference='Nuclear334 measured legacy FbxFactory convert_scene/convert_scene_unit=True, uniform scale1; source forward-Y/upZ. Other exports remain predicted until measured.',requirement='Verify actual importer basis, presentation yaw and every measured contact/portal before runtime adoption; rotation yaw applied separately. Wind moving actors require separate pose adaptation.'),
   lod_policy='Conservative named small-fixture omissions only; kept geometry/UVs/material slots and structural contacts unchanged. No blanket decimation. Native owner chooses reviewed screen thresholds/platform minimums; these are not measured VR budgets.',
   original_source_unchanged=True,site_paving=[],structural_decks=[],native_import_run=False,**extras)
  records.append(record);print('UTILITY_PREPARED',a['issue'],a['key'],[x['triangles'] for x in lods],len(supports),'contacts',len(entrances),'portals',flush=True)
 assert len(records)==11 and all(digest(ROOT/name)==value for name,value in source_hashes.items())
 manifest=dict(parent_issue=157,terrain_owner_issue=125,source_base=inputs['source_base'],assets=records,
  source_artifacts_unchanged=source_hashes,native_import_or_render_run=False,production_catalog_or_runtime_changed=False,
  source_only_preparation=True,device_appearance='unconfirmed',next_action='Native owner imports exact batch, verifies actual coordinate/material/contact/LOD mapping, then adopts plots/support/path/runtime with focused Windows checks.')
 write(MANIFEST,manifest)
 print('ELEVEN_UTILITY_SOURCE_PREPARATION_PASS',len(records),sum(len(a['support_regions']) for a in records),'support regions',flush=True)

if __name__=='__main__':main()
