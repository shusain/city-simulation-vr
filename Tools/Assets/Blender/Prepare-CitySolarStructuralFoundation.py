"""Bounded solar derivative:16 actual feet,17 explicit cable/duct exclusions.

Never edits33c assets/manifests or the approved solar body. Writes new source
foundation files and a pinned import override; no Unreal/render/CI operations.
"""
from pathlib import Path
import importlib.util,re,hashlib,json
import bpy
ROOT=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('ground',ROOT/'Tools/Assets/Blender/Prepare-CityUtilityGroundAndWind.py')
ground=importlib.util.module_from_spec(spec);spec.loader.exec_module(ground)
prep=ground.prep
NAME='SM_Utility_solar_farm_v3_StructuralFeet_Z0'
BLEND=ROOT/'ArtSource/Blender/UtilityIntegration/CityUtility_solar_structural_feet_v3.blend'
FBX=ROOT/'ArtSource/Exports/UtilityIntegration/Foundations'/(NAME+'.fbx')
MANIFEST=ROOT/'ArtSource/Manifests/city_solar_structural_foundation_override.json'
PROOF=ROOT/'ArtSource/Manifests/city_solar_structural_foundation_verification.json'

def digest_record(record):return hashlib.sha256(json.dumps(record,sort_keys=True).encode()).hexdigest()
def main():
 assert not any(p.exists() for p in (BLEND,FBX,MANIFEST,PROOF)),'Fresh derivative required'
 base=prep.read(ROOT/'ArtSource/Manifests/city_utility_integration.json')
 extension=prep.read(ROOT/'ArtSource/Manifests/city_utility_ground_wind_preparation.json')
 plan=prep.read(ROOT/'ArtSource/Manifests/city_utility_native_import_plan.json')
 protected=dict(extension['protected_original_and_reference_files'])
 protected.update({prep.relative(p):prep.digest(p) for folder in ('ArtSource/Blender/UtilityIntegration','ArtSource/Exports/UtilityIntegration') for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix in ('.blend','.fbx') and p not in (BLEND,FBX)})
 protected.update({prep.relative(p):prep.digest(p) for p in (ROOT/'ArtSource/Manifests').glob('city_utility*.json')})
 protected.update(base['source_artifacts_unchanged'])
 solar=next(a for a in base['assets'] if a['issue']==169);regions=solar['support_regions'];assert len(regions)==33
 included=[];excluded=[]
 for region in regions:
  name=region['component'];foot=bool(re.fullmatch(r'R[123]T[1234]_SteelFoot|Inverter[01]_Foot[01]',name))
  record=dict(component=name,source_kind=region['kind'],source_contact_sha256=digest_record(region),source_bounds_m=region['bounds_m'],source_contact=region['contact'])
  if foot:
   assert region['kind']=='structural_ground_contact';included.append(region)
  else:
   assert region['kind']=='cable_ground_contact' or re.fullmatch(r'GroundRowDuct[012]',name)
   record['exclusion_reason']='cable_ground_contact source kind' if region['kind']=='cable_ground_contact' else 'explicit semantic/name-based duct exclusion; frozen kind remains structural_ground_contact'
   excluded.append(record)
 assert len(included)==16 and len(excluded)==17 and sum(r['source_kind']=='structural_ground_contact' for r in excluded)==3
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
 material=bpy.data.materials.new('Concrete');material.use_nodes=True
 obj,columns=ground.foundation(NAME,included,material);topology=ground.check_columns(obj,columns)
 with prep.checked_fbx(obj,FBX,['Concrete']):pass
 bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
 # Inspect actual saved-source facts for the new recipe route, not the old33
 # group's facts. Only the one foundation record is replaced at execution.
 inspector_spec=importlib.util.spec_from_file_location('inspect_recipe',ROOT/'Tools/Assets/Blender/Inspect-CityUtilityNativeRecipe.py')
 inspector=importlib.util.module_from_spec(inspector_spec);inspector_spec.loader.exec_module(inspector)
 facts=inspector.meshinfo(obj)
 previous=next(m for m in plan['foundation_meshes'] if m['issue']==169)
 assert len(previous['lods'])==1 and previous['source_support_top_z_m']==0
 assert all(prep.digest(ROOT/p)==pin for p,pin in protected.items())
 doc=dict(parent_issue=157,terrain_owner=125,source_issue=169,version=3,source_preparation_head='33c7f223c59a8f7af7c9419df458e160a26e42f0',material_recipe_head='3bfb7e74d38dae2fb5b6b60a401946db9b99eba8',
  original_native_plan=dict(path='ArtSource/Manifests/city_utility_native_import_plan.json',sha256=prep.digest(ROOT/'ArtSource/Manifests/city_utility_native_import_plan.json')),
  source_integration_manifest=dict(path='ArtSource/Manifests/city_utility_integration.json',sha256=prep.digest(ROOT/'ArtSource/Manifests/city_utility_integration.json')),
  source=prep.relative(BLEND),source_sha256=prep.digest(BLEND),object=NAME,export=prep.relative(FBX),export_sha256=prep.digest(FBX),facts=facts,
  source_support_top_z_m=0.,local_planes_m=[-1.,0.],included_columns=columns,closed_topology=topology,
  included_components=[dict(component=r['component'],source_kind=r['kind'],source_contact_sha256=digest_record(r)) for r in included],
  excluded_contacts=excluded,included_count=16,excluded_count=17,source_cable_kind_exclusions=14,source_structural_kind_semantic_duct_exclusions=3,
  replacement=dict(original_key=previous['key'],original_target=previous['target'],original_export=previous['lods'][0]['export'],original_export_sha256=previous['lods'][0]['export_sha256']),
  preserved_other_foundation_groups=[dict(key=m['key'],export=m['lods'][0]['export'],sha256=m['lods'][0]['export_sha256']) for m in plan['foundation_meshes'] if m['issue']!=169],
  protected_files_unchanged=protected,no_source_kind_reclassification=True,no_body_or_cable_geometry_change=True,
  exclusion_proof_policy='Generated mesh is exactly the16 original foot columns with no other faces/groups. An excluded cable contact may lie within a retained physical foot; no false requirement to remove that structural footprint.',
  native_import_run=False,runtime_cable_or_articulation_change=False)
 prep.write(MANIFEST,doc)
 print('SOLAR_STRUCTURAL_FOUNDATION_PREP_PASS 16 actual feet,17 exclusions;old33 input preserved',flush=True)
if __name__=='__main__':main()
