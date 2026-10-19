"""Exact native render expectations for two independently proved exceptions.

Frozen source counts/slots/probes remain true source facts. Only tower's54
zero-area roof triangles/LOD and SpinRotor's globally empty slot are routed.
No tolerance change or general degenerate/material-slot fallback is permitted.
"""
import copy,hashlib,json,math
from Assets.Unreal.CityApprovedUtilityPlan import ROOT,PLAN,read,sha,checked_path
from Assets.Unreal.CityApprovedUtilitySolar import apply_solar_foundation,PLAN_SHA
TOWER='ArtSource/Manifests/city_water_tower_render_surface_proof.json'
ROTOR='ArtSource/Manifests/city_wind_rotor_empty_slot_proof.json'
NATIVE='ArtSource/Manifests/city_utility_native_render_buffer_verification.json'
PINS={TOWER:'a98d18b2282e46beaf99b1258fcd141ba33791ef3c36fcc66f74fa54de588197',
 ROTOR:'9c0ac10c14f574943d4618756a9b2279f7f1f1690deab952c64a93540783a99c',
 NATIVE:'21340283929fcbdf6af745d8f720e39458a4bd390fd6c89004be2970730abb4c'}
def record_sha(value):return hashlib.sha256(json.dumps(value,sort_keys=True).encode()).hexdigest()

def tower_lods(record,proof,native):
 assert record['key']=='water_tower' and record['issue']==167 and record['version']==3 and len(record['lods'])==3
 assert proof['original_plan_sha256']==native['original_native_plan_sha256']==PLAN_SHA
 assert native['retained_tower_original_renderable_probes']==143 and native['omitted_exclusively_degenerate_source_probes']==1
 assert native['source_position_cm_tolerance']==.2 and native['source_uv_tolerance']==.0002 and native['source_normal_dot_min']==.99999 and native['tolerances_unchanged']
 assert proof['original_source_preparation_and_proof_bytes_unchanged'] and not proof['source_geometry_or_normals_uvs_edited']
 assert len(proof['lods'])==len(native['tower_lods'])==3
 result=copy.deepcopy(record['lods']);retained=omitted_total=0
 for index,(original,row,out,measured) in enumerate(zip(record['lods'],proof['lods'],result,native['tower_lods'])):
  assert original['lod']==row['lod']==measured['lod']==index and row['export']==original['export'] and row['export_sha256']==original['export_sha256']
  assert row['original_facts_sha256']==record_sha(original['facts']) and row['raw_source_triangles']==original['triangles']==[4224,3536,2544][index]
  assert row['raw_triangles_by_material']==original['facts']['triangles_by_material'] and row['exact_zero_triangles_by_material']=={'Paint_BlueGrey':54}
  zero=row['exact_zero_triangles'];assert len(zero)==54 and len({t['triangle'] for t in zero})==54
  for t in zero:
   assert t['component']=='Tank_roof' and t['material']=='Paint_BlueGrey' and t['area_m2']==0.
   assert len(set(t['indices']))==3 and len(set(map(tuple,t['positions_m'])))<3
  assert sorted(t['polygon'] for t in zero)==list(range(172,200))+[201]*26
  counts=copy.deepcopy(original['facts']['triangles_by_material']);counts['Paint_BlueGrey']-=54
  assert counts==row['expected_render_triangles_by_material']==measured['actual_native_material_triangles']
  assert original['triangles']-54==row['expected_render_triangles']==measured['actual_native_triangles']==[4170,3482,2490][index]
  excluded=row['omitted_degenerate_only_probes'];expected_exclusions=[('Paint_BlueGrey',15)] if index==2 else []
  assert [(p['material'],p['source_sample_index']) for p in excluded]==expected_exclusions
  all_coverage=row['original_probe_coverage'];assert len(all_coverage)==48
  samples={};native_matches={(p['material'],p['source_sample_index']):p for p in measured['retained_original_probe_matches']}
  assert len(native_matches)==(47 if index==2 else 48)
  for material,values in original['facts']['samples'].items():
   samples[material]=[]
   for sample_index,sample in enumerate(values):
    coverage=[p for p in all_coverage if p['material']==material and p['source_sample_index']==sample_index];assert len(coverage)==1
    p=coverage[0];assert p['sample']==sample and p['matching_source_loops']
    if (material,sample_index) in expected_exclusions:
     assert p['exclusively_zero_area'] and not p['matching_valid_face_loops'] and p in excluded
     assert sample==dict(position_m=[0.,-0.,15.449999809265137],normal=[0.,0.,1.],uv0=[0.,0.])
     assert all(next(t for t in zero if t['triangle']==ti)['polygon']==201 for ti in p['matching_source_triangles'])
     omitted_total+=1;continue
    assert not p['exclusively_zero_area'] and p['matching_valid_face_loops']
    actual=native_matches[(material,sample_index)];assert actual['sample']==sample and actual['native_vertex_referenced_by_render_indices']
    assert actual['position_error_cm']<=.2 and actual['uv_error']<=.0002 and actual['normal_dot']>.99999
    samples[material].append(sample);retained+=1
  assert samples==row['renderable_samples'] and sum(map(len,samples.values()))==row['retained_probe_count']
  out['triangles']=original['triangles']-54;out['facts']['triangles_by_material']=counts;out['facts']['samples']=samples
 assert retained==143 and omitted_total==1
 return result

def rotor_slots(record,proof,native):
 assert record['key']=='wind_SpinRotor' and record['role']=='SpinRotor' and record['issue']==168 and record['version']==1
 declared=['OffWhite_PaintedSteel','BlueGrey_Machinery','Mechanical_Mapped'];used=['OffWhite_PaintedSteel','BlueGrey_Machinery']
 assert record['material_slots']==proof['declared_source_slots']==declared and proof['effective_native_slots']==used
 assert proof['removed_empty_slot']=='Mechanical_Mapped' and proof['used_source_material_union']==sorted(used)
 assert proof['original_plan_sha256']==PLAN_SHA and not proof['source_graphs_or_material_instances_removed']
 union=set()
 for index,(lod,row,actual) in enumerate(zip(record['lods'],proof['lods'],native['rotor_lods'])):
  assert lod['lod']==row['lod']==actual['lod']==index and lod['triangles']==row['triangles']==actual['actual_native_triangles']==916
  assert lod['export']==row['export'] and lod['export_sha256']==row['export_sha256']
  assert lod['facts']['triangles_by_material']==row['source_triangles_by_material']==actual['actual_native_material_triangles']=={'OffWhite_PaintedSteel':824,'BlueGrey_Machinery':92}
  assert actual['actual_native_slots']==used and row['empty_slot_used_triangle_count']==row['empty_slot_probe_count']==0
  assert lod['facts']['material_slots']==declared and set(lod['facts']['samples'])==set(used);union.update(lod['facts']['triangles_by_material'])
 assert union==set(used) and len(record['lods'])==len(proof['lods'])==len(native['rotor_lods'])==3
 return used

def apply_native_render_expectations(plan):
 assert sha(PLAN)==PLAN_SHA and plan==apply_solar_foundation(read(PLAN)),'Expected frozen plan plus exact solar override only'
 for path,pin in PINS.items():assert sha(checked_path(path))==pin,('Render proof differs',path)
 tower_proof=read(ROOT/TOWER);rotor_proof=read(ROOT/ROTOR);native=read(ROOT/NATIVE)
 assert native['source_proofs']=={TOWER:PINS[TOWER],ROTOR:PINS[ROTOR]} and native['all_offline_comparisons_pass']
 for path,pin in tower_proof['protected_files_unchanged'].items():assert sha(checked_path(path))==pin,('Frozen source differs',path)
 for proof,path_key,sha_key in [(tower_proof,'prepared_source','prepared_source_sha256'),(rotor_proof,'source','source_sha256')]:assert sha(checked_path(proof[path_key]))==proof[sha_key]
 result=copy.deepcopy(plan);tower=next(r for r in result['whole_meshes'] if r['key']=='water_tower');rotor=next(r for r in result['wind_role_meshes'] if r['role']=='SpinRotor')
 tower['lods']=tower_lods(tower,tower_proof,native);rotor['material_slots']=rotor_slots(rotor,rotor_proof,native)
 result['native_import_overrides'].extend([
  dict(kind='water_tower_exact_zero_area_render_expectation',key=tower['key'],source_proof=TOWER,source_proof_sha256=PINS[TOWER],native_buffer_proof=NATIVE,native_buffer_proof_sha256=PINS[NATIVE],
   raw_source_triangles=[4224,3536,2544],expected_native_triangles=[4170,3482,2490],exact_zero_triangles_per_lod=54,only_affected_material='Paint_BlueGrey',retained_original_renderable_probes=143,
   excluded_probe=dict(lod=2,material='Paint_BlueGrey',source_sample_index=15,reason='Exclusively zero-area cap201; no renderable source or native indexed-face corner'),source_exports_and_frozen_raw_plan_unchanged=True,tolerances_unchanged=True),
  dict(kind='wind_SpinRotor_only_globally_unused_slot',key=rotor['key'],source_proof=ROTOR,source_proof_sha256=PINS[ROTOR],native_buffer_proof=NATIVE,native_buffer_proof_sha256=PINS[NATIVE],
   source_declared_slots=['OffWhite_PaintedSteel','BlueGrey_Machinery','Mechanical_Mapped'],effective_native_slots=rotor['material_slots'],removed_only_empty_slot='Mechanical_Mapped',all3lods_used_triangles=0,all3lods_probe_count=0,
   geometry_counts_bounds_uvs_normals_probes_and_source_graphs_unchanged=True)])
 return result
