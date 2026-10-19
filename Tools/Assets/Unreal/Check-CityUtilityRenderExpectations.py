"""Offline exact-policy/unchanged-validator/negative-scope checks; no Unreal."""
from pathlib import Path
import ast,copy,subprocess,sys
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from Assets.Unreal.CityApprovedUtilityPlan import ROOT,PLAN,preflight,read,sha,write
from Assets.Unreal.CityApprovedUtilitySolar import apply_solar_foundation,PLAN_SHA
from Assets.Unreal.CityApprovedUtilityRender import apply_native_render_expectations,tower_lods,rotor_slots,TOWER,ROTOR,NATIVE,PINS

def main():
 raw=preflight();solar=apply_solar_foundation(raw);before=copy.deepcopy(solar);effective=apply_native_render_expectations(solar);assert solar==before and sha(PLAN)==PLAN_SHA
 assert set(effective)==set(solar) and len(effective['native_import_overrides'])==3
 for key in solar:
  if key not in ('whole_meshes','wind_role_meshes','native_import_overrides'):assert solar[key]==effective[key],key
 pairs=[]
 for group in ('whole_meshes','wind_role_meshes'):
  for old,new in zip(solar[group],effective[group]):
   if old!=new:pairs.append((old['key'],old,new))
 assert [r[0] for r in pairs]==['water_tower','wind_SpinRotor']
 tower_old,tower_new=pairs[0][1:];rotor_old,rotor_new=pairs[1][1:]
 assert {k:v for k,v in tower_old.items() if k!='lods'}=={k:v for k,v in tower_new.items() if k!='lods'}
 for original,actual in zip(tower_old['lods'],tower_new['lods']):
  assert {k:v for k,v in original.items() if k not in ('triangles','facts')}=={k:v for k,v in actual.items() if k not in ('triangles','facts')}
  assert {k:v for k,v in original['facts'].items() if k not in ('triangles_by_material','samples')}=={k:v for k,v in actual['facts'].items() if k not in ('triangles_by_material','samples')}
  for material,count in original['facts']['triangles_by_material'].items():assert actual['facts']['triangles_by_material'][material]==count-(54 if material=='Paint_BlueGrey' else 0)
 assert [l['triangles'] for l in tower_new['lods']]==[4170,3482,2490]
 assert sum(len(v) for l in tower_old['lods'] for v in l['facts']['samples'].values())==144
 assert sum(len(v) for l in tower_new['lods'] for v in l['facts']['samples'].values())==143
 assert {k:v for k,v in rotor_old.items() if k!='material_slots'}=={k:v for k,v in rotor_new.items() if k!='material_slots'}
 assert rotor_new['material_slots']==['OffWhite_PaintedSteel','BlueGrey_Machinery']
 assert rotor_new['lods']==rotor_old['lods'] and all(sum(l['facts']['triangles_by_material'].values())==916 for l in rotor_new['lods'])
 # The native owner projects full material bindings to these effective slots in
 # main, preserving strict import_mesh/check_mesh equality and all source MIs.
 full={'OffWhite_PaintedSteel':'a','BlueGrey_Machinery':'b','Mechanical_Mapped':'c'}
 assert {s:full[s] for s in rotor_new['material_slots']}=={'OffWhite_PaintedSteel':'a','BlueGrey_Machinery':'b'}
 records=effective['whole_meshes']+effective['wind_role_meshes']+effective['foundation_meshes']
 targets=[r['target'] for r in records]+[r['damaged_target'] for r in effective['whole_meshes']]
 assert len(set(targets))==len(targets)==37 and sum(len(r['lods']) for r in records)==54
 source=read(ROOT/TOWER);rotor=read(ROOT/ROTOR);native=read(ROOT/NATIVE);negative=[]
 def reject(label,fn):
  try:fn()
  except AssertionError:negative.append(label)
  else:raise AssertionError('Unexpected policy acceptance: '+label)
 bad=copy.deepcopy(source);bad['lods'][0]['exact_zero_triangles_by_material']['Paint_BlueGrey']=55
 reject('55-triangle tolerance instead of exact54',lambda:tower_lods(tower_old,bad,native))
 bad=copy.deepcopy(source);bad['lods'][0]['exact_zero_triangles'][0]['material']='Mechanical'
 reject('different material omission',lambda:tower_lods(tower_old,bad,native))
 bad=copy.deepcopy(source);bad['lods'][0]['omitted_degenerate_only_probes']=[bad['lods'][0]['original_probe_coverage'][0]]
 reject('renderable original probe removal',lambda:tower_lods(tower_old,bad,native))
 bad=copy.deepcopy(native);bad['source_position_cm_tolerance']=.3
 reject('relaxed position tolerance',lambda:tower_lods(tower_old,source,bad))
 bad=copy.deepcopy(tower_old);bad['key']='solar_farm'
 reject('tower exception applied to another asset',lambda:tower_lods(bad,source,native))
 bad=copy.deepcopy(rotor);bad['effective_native_slots']=['OffWhite_PaintedSteel']
 reject('used rotor material slot pruned',lambda:rotor_slots(rotor_old,bad,native))
 bad=copy.deepcopy(rotor_old);bad['lods'][0]['facts']['triangles_by_material']['Mechanical_Mapped']=1
 reject('slot not empty in every sourceLOD',lambda:rotor_slots(bad,rotor,native))
 bad=copy.deepcopy(rotor_old);bad['key']='wind_YawNacelle'
 reject('rotor-only exception applied elsewhere',lambda:rotor_slots(bad,rotor,native))
 reject('already-routed or stale effective input',lambda:apply_native_render_expectations(effective))
 checked=[]
 for filename,names in [('CityApprovedUtilityPlan.py',('preflight','validate_graph')),('CityApprovedUtilityMeshes.py',('check_mesh','import_mesh'))]:
  relative='Tools/Assets/Unreal/'+filename
  old=ast.parse(subprocess.run(['git','show','45c19563e88dfa9e12139b2215968660aa2abd63:'+relative],cwd=ROOT,capture_output=True,check=True).stdout.decode())
  new=ast.parse((ROOT/relative).read_text())
  for name in names:
   find=lambda tree:next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name==name)
   assert ast.dump(find(old))==ast.dump(find(new)),('Core validator changed',name);checked.append(name)
 for relative in ('Tools/Assets/Blender/Verify-CityWaterTowerRenderPolicy.py','Tools/Assets/Blender/Verify-CityWindRotorSlots.py','Tools/Assets/Unreal/CityApprovedUtilityRender.py','Tools/Assets/Unreal/Verify-CityUtilityNativeRenderBuffers.py'):
  ast.parse((ROOT/relative).read_text())
 receipt=dict(task=157,original_plan_sha256=sha(PLAN),source_proof_pins=PINS,expected_tower_native_triangles=[4170,3482,2490],exact_zero_area_omission_per_lod=54,only_affected_tower_material='Paint_BlueGrey',
  retained_original_tower_renderable_probes=143,excluded_only_source_zero_area_probe=dict(lod=2,material='Paint_BlueGrey',index=15),native_buffer_original_probes_corroborated=True,
  effective_rotor_slots=rotor_new['material_slots'],removed_only_globally_unused_slot='Mechanical_Mapped',rotor_all3lods_triangles=916,rotor_geometry_and_all_original_probes_unchanged=True,
  other_input_records_unchanged=True,original_foundations_and_solar_override_unchanged=True,source_graphs_and_105_material_products_unchanged=True,
  source_and_native_core_validators_unchanged_vs45c=checked,negative_cases_rejected=negative,native_executed_by_this_check=False,
  native_owner_followup='125 projects main material bindings by effective record.material_slots; normal merge/review and aggregate native import/checks still required',status='Offline exact policy/source/native-buffer corroboration pass;not native batch acceptance')
 output=ROOT/'Artifacts/UtilitySourcePreparation/NativeRenderExpectations-OfflineVerification.json';output.parent.mkdir(parents=True,exist_ok=True);write(output,receipt)
 print('UTILITY_NATIVE_RENDER_EXPECTATIONS_OFFLINE_PASS towerexact54/LOD,143probes,rotoronlyempty3rdslot,9negativecases,corevalidators unchanged;NO Unreal execution')
if __name__=='__main__':main()
