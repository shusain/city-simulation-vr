"""Pinned, source-only solar foundation override after the frozen preflight.

The raw33c plan and all original evidence remain immutable. Only16 physical
feet enter the effective native plan; source cable/duct tags are not rewritten.
"""
import copy,hashlib,json
from Assets.Unreal.CityApprovedUtilityPlan import ROOT,PLAN,sha,read,checked_path

MANIFEST='ArtSource/Manifests/city_solar_structural_foundation_override.json'
MANIFEST_SHA='9539de559145bf5290e18e50ed2ba02ccce33e58c354e174a85ba7059b5fc633'
PROOF='ArtSource/Manifests/city_solar_structural_foundation_verification.json'
PROOF_SHA='2889e546d42598779d18482adc9717cfe8788803c42949bccce667763da4e2f9'
PLAN_SHA='8765c24b51df26308d838c12da1a7af867b0903e4ecc14b5a6e0aecbbb764b78'
FEET={f'R{r}T{t}_SteelFoot' for r in range(1,4) for t in range(1,5)}|{f'Inverter{i}_Foot{f}' for i in range(2) for f in range(2)}
DUCTS={f'GroundRowDuct{i}' for i in range(3)}

def contact_sha(region):return hashlib.sha256(json.dumps(region,sort_keys=True).encode()).hexdigest()

def apply_solar_foundation(plan):
 assert sha(PLAN)==PLAN_SHA and plan==read(PLAN),'Expected unmodified frozen plan after strict preflight'
 assert sha(checked_path(MANIFEST))==MANIFEST_SHA and sha(checked_path(PROOF))==PROOF_SHA
 d=read(ROOT/MANIFEST);proof=read(ROOT/PROOF)
 assert (d['source_issue'],d['version'],d['included_count'],d['excluded_count'])==(169,3,16,17)
 assert d['original_native_plan']==dict(path=PLAN.relative_to(ROOT).as_posix(),sha256=PLAN_SHA)
 assert proof['override_manifest']==MANIFEST and proof['override_manifest_sha256']==MANIFEST_SHA
 for path,pin in d['protected_files_unchanged'].items():assert sha(checked_path(path))==pin,('Frozen input changed',path)
 for path,pin in ((d['source'],d['source_sha256']),(d['export'],d['export_sha256'])):assert sha(checked_path(path))==pin,path
 source=d['source_integration_manifest'];assert sha(checked_path(source['path']))==source['sha256']
 solar=next(a for a in read(ROOT/source['path'])['assets'] if a['issue']==169)
 regions={r['component']:r for r in solar['support_regions']};assert len(regions)==33
 included={r['component']:r for r in d['included_components']};excluded={r['component']:r for r in d['excluded_contacts']}
 assert len(included)==16 and set(included)==FEET and len(excluded)==17 and set(excluded)==set(regions)-FEET
 cable={name for name,r in regions.items() if r['kind']=='cable_ground_contact'}
 assert len(cable)==14 and set(excluded)==cable|DUCTS and cable.isdisjoint(DUCTS)
 for name,entry in {**included,**excluded}.items():
  original=regions[name]
  assert entry['source_kind']==original['kind'] and entry['source_contact_sha256']==contact_sha(original)
  if name in included:assert original['kind']=='structural_ground_contact'
  else:
   assert entry['source_contact']==original['contact'] and entry['source_bounds_m']==original['bounds_m']
   if name in DUCTS:
    assert original['kind']=='structural_ground_contact' and 'semantic/name-based duct exclusion' in entry['exclusion_reason']
   else:assert entry['exclusion_reason']=='cable_ground_contact source kind'
 assert d['source_cable_kind_exclusions']==proof['source_cable_kind_exclusions']==14
 assert d['source_structural_kind_semantic_duct_exclusions']==proof['explicit_semantic_duct_exclusions']==3
 assert d['local_planes_m']==[-1.,0.] and d['source_support_top_z_m']==0.
 assert d['no_source_kind_reclassification'] and d['no_body_or_cable_geometry_change']
 assert not d['native_import_run'] and not d['runtime_cable_or_articulation_change']
 for flag in ('all_actual_mesh_groups_are_feet','no_extra_vertices_or_faces','no_excluded_cable_or_duct_column','all_original_contact_caps_and_unit_topology_exact','all_frozen_preparations_and_approved_assets_unchanged','no_original_source_tag_reclassification','all_checks_pass'):assert proof[flag],flag
 assert not proof['native_execution_or_body_cable_articulation_changes']
 assert proof['retained_component_names']==sorted(FEET) and proof['excluded_contact_names']==sorted(excluded)
 columns=d['included_columns'];rows=proof['retained_columns']
 assert len(columns)==len(rows)==16 and {r['component'] for r in columns}=={r['component'] for r in rows}==FEET
 for row in rows:
  assert row['original_contact_triangles_exact'] and row['unit_planes_m']==[-1,0] and row['signed_volume_m3']>0
  assert row['source_top_z_m']==0 and row['closed_edges']>0
 facts=d['facts'];assert facts['triangles_by_material']=={'Concrete':192} and facts['material_slots']==['Concrete']
 assert facts['uv_channels']==1 and facts['render_uv_channel']==0 and facts['uv_names']==['UV0']
 assert facts['bounds_m']['min'][2]==-1. and facts['bounds_m']['max'][2]==0. and facts['samples']['Concrete']
 fbx=proof['actual_fbx_readback'];assert fbx['fbx']==d['export'] and fbx['triangles']==192
 assert fbx['max_position_error_m']<.00001 and fbx['max_normal_vector_error']<.00001 and fbx['max_uv_error']<.00001
 original=next(m for m in plan['foundation_meshes'] if m['issue']==169);r=d['replacement']
 assert (original['key'],original['target'],original['lods'][0]['export'],original['lods'][0]['export_sha256'])==(r['original_key'],r['original_target'],r['original_export'],r['original_export_sha256'])
 assert original['source_support_top_z_m']==0 and len(original['lods'])==1
 others=[dict(key=m['key'],export=m['lods'][0]['export'],sha256=m['lods'][0]['export_sha256']) for m in plan['foundation_meshes'] if m['issue']!=169]
 assert len(others)==11 and d['preserved_other_foundation_groups']==proof['preserved_other_foundation_groups']==others
 result=copy.deepcopy(plan);effective=next(m for m in result['foundation_meshes'] if m['issue']==169)
 effective['lods'][0]=dict(lod=0,export=d['export'],export_sha256=d['export_sha256'],triangles=192,facts=copy.deepcopy(facts))
 result['native_import_overrides']=[dict(kind='solar_structural_feet',manifest=MANIFEST,manifest_sha256=MANIFEST_SHA,proof=PROOF,proof_sha256=PROOF_SHA,
  original_plan_sha256=PLAN_SHA,key=original['key'],target=original['target'],original_export=r['original_export'],original_export_sha256=r['original_export_sha256'],
  effective_export=d['export'],effective_export_sha256=d['export_sha256'],retained_foot_names=sorted(FEET),excluded_contacts=copy.deepcopy(d['excluded_contacts']),
  retained_count=16,excluded_count=17,source_cable_kind_exclusions=14,semantic_duct_exclusions=3,source_kind_tags_preserved=True,other11_foundation_groups_unchanged=True)]
 return result
