"""Offline corroboration of source surface proofs against125's native snapshot.

Reads already-produced buffers; never imports Unreal or edits source geometry.
"""
from pathlib import Path
import argparse,math,sys
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from Assets.Unreal.CityApprovedUtilityPlan import ROOT,PLAN,read,sha,write
TOWER='ArtSource/Manifests/city_water_tower_render_surface_proof.json'
ROTOR='ArtSource/Manifests/city_wind_rotor_empty_slot_proof.json'
SNAPSHOT_SHA='5c89be5bb4e2a40054e78e4e086ba2467e964b2160cf1546cfe8c4564d295f85'

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--native-input',required=True);parser.add_argument('--output',required=True);args=parser.parse_args()
 native_path=Path(args.native_input);assert sha(native_path)==SNAPSHOT_SHA,'Expected125 exact isolated diagnostic buffer snapshot'
 data=read(native_path);assert data['diagnostic_only'] and len(data['results'])==18
 source=read(ROOT/TOWER);rotor_source=read(ROOT/ROTOR);original=next(m for m in read(PLAN)['whole_meshes'] if m['key']=='water_tower')
 tower=next(m for m in data['results'] if m['key']=='water_tower');rows=[];probe_total=0
 for source_lod,lod in zip(source['lods'],tower['lods']):
  index=source_lod['lod'];assert lod['lod']==index and lod['native_triangles']==source_lod['expected_render_triangles']
  actual={s['material_slot']:s['triangles'] for s in lod['sections']};assert actual==source_lod['expected_render_triangles_by_material']
  assert sum(actual.values())==lod['native_triangles'];sections={s['material_slot']:s for s in lod['sections']};matches=[];absent=[]
  for s in lod['sections']:
   assert len(s['indices'])==s['triangles']*3 and len(s['positions_cm'])==len(s['uv0'])==len(s['normals'])==s['vertices']
   assert all(0<=i<s['vertices'] for i in s['indices'])
  for material,samples in original['lods'][index]['facts']['samples'].items():
   s=sections[material];used=set(s['indices'])
   for sample_index,sample in enumerate(samples):
    p=sample['position_m'];target=[p[0]*100,-p[1]*100,p[2]*100];uv=[sample['uv0'][0],1-sample['uv0'][1]]
    n=sample['normal'];normal=[n[0],-n[1],n[2]];source_length=math.sqrt(sum(x*x for x in normal));options=[]
    for vertex in used:
     actual_p=s['positions_cm'][vertex];distance=math.sqrt(sum((a-b)**2 for a,b in zip(target,actual_p)))
     if distance>.2:continue
     uv_error=max(abs(a-b) for a,b in zip(uv,s['uv0'][vertex]))
     if uv_error>.0002:continue
     actual_n=s['normals'][vertex];length=math.sqrt(sum(x*x for x in actual_n));dot=sum(a*b for a,b in zip(normal,actual_n))/source_length/length if source_length*length>1e-12 else -1.
     if dot>.99999:options.append((distance,uv_error,dot,vertex))
    source_row=next(r for r in source_lod['original_probe_coverage'] if r['material']==material and r['source_sample_index']==sample_index)
    if source_row['exclusively_zero_area']:
     assert not options,'Source-only exclusion must not mask a surviving native render surface'
     absent.append(dict(material=material,source_sample_index=sample_index,sample=sample,source_proven_exclusively_zero_area=True,native_render_index_match=False));continue
    assert options,('Renderable source probe absent',index,material,sample_index)
    distance,uv_error,dot,vertex=min(options,key=lambda r:(r[0],r[1],-r[2]));matches.append(dict(material=material,source_sample_index=sample_index,sample=sample,
     native_vertex=vertex,native_vertex_referenced_by_render_indices=True,position_error_cm=distance,uv_error=uv_error,normal_dot=dot))
  assert len(matches)==source_lod['retained_probe_count'] and len(absent)==(1 if index==2 else 0)
  probe_total+=len(matches);rows.append(dict(lod=index,raw_source_triangles=source_lod['raw_source_triangles'],actual_native_triangles=lod['native_triangles'],actual_native_material_triangles=actual,
   exact54_source_zero_triangles_explain_native_count=True,retained_original_probe_matches=matches,omitted_degenerate_only_probes=absent,
   max_position_error_cm=max(r['position_error_cm'] for r in matches),max_uv_error=max(r['uv_error'] for r in matches),min_normal_dot=min(r['normal_dot'] for r in matches)))
 rotor=next(r for r in data['results'] if r['key']=='wind_SpinRotor');assert rotor['actual_slots']==rotor_source['effective_native_slots']
 rotor_rows=[]
 for lod,source_lod in zip(rotor['lods'],rotor_source['lods']):
  actual={s['material_slot']:s['triangles'] for s in lod['sections']};assert actual==source_lod['source_triangles_by_material'] and lod['native_triangles']==916
  rotor_rows.append(dict(lod=lod['lod'],actual_native_triangles=916,actual_native_slots=rotor['actual_slots'],actual_native_material_triangles=actual,only_declared_but_globally_unused_slot_pruned=True))
 assert probe_total==143
 proof=dict(task=157,native_owner=125,source_proofs={TOWER:sha(ROOT/TOWER),ROTOR:sha(ROOT/ROTOR)},original_native_plan_sha256=sha(PLAN),
  native_snapshot_sha256=SNAPSHOT_SHA,native_snapshot_name='ImportDiagnostics1-Geometry.json',native_execution_performed_by='125 terrain/stairs owner,isolated ImportDiagnostics,not source-preparation chat',
  diagnostic_default_materials_only=True,no_production_asset_or_gameplay_acceptance=True,tower_lods=rows,retained_tower_original_renderable_probes=143,omitted_exclusively_degenerate_source_probes=1,
  source_position_cm_tolerance=.2,source_uv_tolerance=.0002,source_normal_dot_min=.99999,tolerances_unchanged=True,rotor_lods=rotor_rows,
  rotor_native_uv_normal_probes='Pending unchanged core validator after explicit source-used slot routing in aggregate native batch',all_offline_comparisons_pass=True)
 output=Path(args.output);output.parent.mkdir(parents=True,exist_ok=True);write(output,proof)
 print('UTILITY_NATIVE_RENDER_BUFFER_CORROBORATION_PASS towerexact54/LOD,143 render-indexed probes,1 source-proven zero-area-only probe;rotor916/2usedslots')
if __name__=='__main__':main()
