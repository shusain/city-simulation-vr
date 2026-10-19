"""Offline effective-route audit; no Unreal import or frozen proof rewrite."""
from pathlib import Path
import ast,copy,subprocess,sys
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from Assets.Unreal.CityApprovedUtilityPlan import ROOT,PLAN,preflight,sha,write
from Assets.Unreal.CityApprovedUtilitySolar import apply_solar_foundation,MANIFEST,MANIFEST_SHA,PROOF,PROOF_SHA,PLAN_SHA,FEET,DUCTS

def function(tree,name):return next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name==name)

def main():
 original=preflight();before=copy.deepcopy(original);effective=apply_solar_foundation(original)
 assert original==before and sha(PLAN)==PLAN_SHA
 assert set(effective)-set(original)=={'native_import_overrides'}
 for key in original:
  if key!='foundation_meshes':assert effective[key]==original[key],key
 changed=[(a,b) for a,b in zip(original['foundation_meshes'],effective['foundation_meshes']) if a!=b]
 assert len(changed)==1 and len(effective['foundation_meshes'])==12
 old,new=changed[0];assert old['issue']==new['issue']==169
 assert {k:v for k,v in old.items() if k!='lods'}=={k:v for k,v in new.items() if k!='lods'}
 route=effective['native_import_overrides'][0]
 assert new['key']==route['key'] and new['target']==route['target'] and new['source_support_top_z_m']==0
 assert old['lods'][0]['triangles']==396 and new['lods'][0]['triangles']==192
 assert old['lods'][0]['export']!=new['lods'][0]['export'] and Path(ROOT/old['lods'][0]['export']).is_file()
 assert len(new['lods'])==1 and new['material_slots']==['Concrete']
 records=effective['whole_meshes']+effective['wind_role_meshes']+effective['foundation_meshes']
 targets=[m['target'] for m in records]+[m['damaged_target'] for m in effective['whole_meshes']]
 assert len(targets)==len(set(targets))==37 and sum(len(m['lods']) for m in records)==54
 assert len(route['retained_foot_names'])==16 and set(route['retained_foot_names'])==FEET
 exclusions=route['excluded_contacts'];assert len(exclusions)==17
 assert {r['component'] for r in exclusions if r['source_kind']=='structural_ground_contact'}==DUCTS
 # A stale, already-routed or tampered input must fail without source writes.
 rejected=[]
 for label,bad in [('already routed plan',effective),('modified source target',copy.deepcopy(original))]:
  if label=='modified source target':bad['foundation_meshes'][-1]['target']+='_Wrong'
  try:apply_solar_foundation(bad)
  except AssertionError:rejected.append(label)
  else:raise AssertionError('Unexpected accepted input: '+label)
 checked=[]
 for filename,names in [('CityApprovedUtilityPlan.py',('preflight','validate_graph')),('CityApprovedUtilityMeshes.py',('check_mesh','import_mesh'))]:
  path='Tools/Assets/Unreal/'+filename
  baseline=subprocess.run(['git','show','45c19563e88dfa9e12139b2215968660aa2abd63:'+path],cwd=ROOT,capture_output=True,check=True).stdout.decode()
  previous=ast.parse(baseline);current=ast.parse((ROOT/path).read_text())
  for name in names:
   assert ast.dump(function(previous,name))==ast.dump(function(current,name)),('Validator changed',name)
   checked.append(name)
 # Both native entry points route after strict preflight and before native guard.
 for filename in ('CityApprovedUtilityMaterials.py','CityApprovedUtilityMeshes.py'):
  tree=ast.parse((ROOT/'Tools/Assets/Unreal'/filename).read_text());main=function(tree,'main')
  first=main.body[0];assert isinstance(first,ast.Assign)
  assert isinstance(first.value,ast.Call) and first.value.func.id=='apply_native_render_expectations'
  solar=first.value.args[0];assert isinstance(solar,ast.Call) and solar.func.id=='apply_solar_foundation'
  assert isinstance(solar.args[0],ast.Call) and solar.args[0].func.id=='preflight'
  receipt=next(n.value for n in main.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='receipt' for t in n.targets))
  assert any(k.arg=='native_import_overrides' for k in receipt.keywords)
 meshes=(ROOT/'Tools/Assets/Unreal/CityApprovedUtilityMeshes.py').read_text()
 assert "assert materials['native_import_overrides']==plan['native_import_overrides']" in meshes
 native=(ROOT/'Tools/Assets/Unreal/CityApprovedUtilityNative.py').read_text()
 assert 'Tools/Assets/Unreal/CityApprovedUtilitySolar.py' in native
 for relative in ('Tools/Assets/Blender/Prepare-CitySolarStructuralFoundation.py','Tools/Assets/Blender/Verify-CitySolarStructuralFoundation.py','Tools/Assets/Unreal/CityApprovedUtilitySolar.py'):
  ast.parse((ROOT/relative).read_text())
 receipt=dict(task=157,terrain_owner=125,source_issue=169,version=3,original_plan_sha256=sha(PLAN),override_manifest=MANIFEST,override_manifest_sha256=MANIFEST_SHA,
  independent_geometry_proof=PROOF,independent_geometry_proof_sha256=PROOF_SHA,changed_native_records=1,native_target_and_key_unchanged=True,
  retained_foot_names=sorted(FEET),excluded_contacts=exclusions,retained_count=16,excluded_count=17,source_cable_kind_exclusions=14,semantic_duct_exclusions=3,
  original396_triangle_foundation_retained_as_reference=True,effective_triangles=192,source_kind_tags_preserved=True,other11_foundation_groups_unchanged=True,
  healthy_damaged_body_and_wind_inputs_unchanged=True,mesh_packages=37,lod_inputs=54,source_and_native_probe_validators_unchanged_vs45c=checked,
  both_phases_route_before_native_guard=True,material_mesh_receipt_override_consistency_required=True,negative_cases_rejected=rejected,
  native_execution=False,status='Offline route and saved Blender/FBX proof pass; native import/runtime review pending125')
 output=ROOT/'Artifacts/UtilitySourcePreparation/SolarStructuralRecipe-OfflineVerification.json'
 output.parent.mkdir(parents=True,exist_ok=True);write(output,receipt)
 print('SOLAR_STRUCTURAL_RECIPE_OFFLINE_PASS 1 routed record,16 feet,17 exclusions,other11 foundations unchanged,37 packages/54LOD inputs; NO Unreal execution')

if __name__=='__main__':main()
