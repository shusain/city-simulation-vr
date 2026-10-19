"""Independent actual saved mesh/FBX proof for the16-foot solar override."""
from pathlib import Path
import importlib.util,json,re,hashlib
import bpy
ROOT=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('ground_verify',ROOT/'Tools/Assets/Blender/Verify-CityUtilityGroundAndWind.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
MANIFEST=ROOT/'ArtSource/Manifests/city_solar_structural_foundation_override.json'
PROOF=ROOT/'ArtSource/Manifests/city_solar_structural_foundation_verification.json'
def main():
 d=v.read(MANIFEST);base=v.read(ROOT/d['source_integration_manifest']['path']);solar=next(a for a in base['assets'] if a['issue']==169);regions={r['component']:r for r in solar['support_regions']}
 assert v.sha(ROOT/d['source'])==d['source_sha256'] and v.sha(ROOT/d['export'])==d['export_sha256']
 bpy.ops.wm.read_factory_settings(use_empty=True);objects=v.old.load(ROOT/d['source']);assert len(objects)==1
 obj=objects[0];groups=v.old.memberships(obj);included={r['component'] for r in d['included_components']};excluded={r['component'] for r in d['excluded_contacts']}
 assert len(groups)==len(included)==16 and len(excluded)==17 and set(groups)==included and included.isdisjoint(excluded) and included|excluded==set(regions)
 assert all(re.fullmatch(r'R[123]T[1234]_SteelFoot|Inverter[01]_Foot[01]',name) for name in included)
 assert sum(regions[name]['kind']=='cable_ground_contact' for name in excluded)==14
 assert {name for name in excluded if regions[name]['kind']=='structural_ground_contact'}=={'GroundRowDuct0','GroundRowDuct1','GroundRowDuct2'}
 rows=[v.foundation_check(obj,c,regions[c['component']]) for c in d['included_columns']]
 assert sum(c['vertex_count'] for c in d['included_columns'])==len(obj.data.vertices)
 assert sum(c['face_count'] for c in d['included_columns'])==len(obj.data.polygons)
 assert set(c['component'] for c in d['included_columns'])==included
 readback=v.fbx_check(ROOT/d['export'],obj,sum(d['facts']['triangles_by_material'].values()))
 assert readback['triangles']==192
 for entry in d['excluded_contacts']:
  original=regions[entry['component']]
  assert entry['source_kind']==original['kind'] and entry['source_contact']==original['contact'] and entry['source_bounds_m']==original['bounds_m']
  assert hashlib.sha256(json.dumps(original,sort_keys=True).encode()).hexdigest()==entry['source_contact_sha256']
 assert all(v.sha(ROOT/p)==pin for p,pin in d['protected_files_unchanged'].items())
 proof=dict(parent_issue=157,source_issue=169,version=3,override_manifest=MANIFEST.relative_to(ROOT).as_posix(),override_manifest_sha256=v.sha(MANIFEST),
  retained_columns=rows,retained_component_names=sorted(included),excluded_contact_names=sorted(excluded),source_cable_kind_exclusions=14,explicit_semantic_duct_exclusions=3,
  all_actual_mesh_groups_are_feet=True,no_extra_vertices_or_faces=True,no_excluded_cable_or_duct_column=True,all_original_contact_caps_and_unit_topology_exact=True,
  actual_fbx_readback=readback,preserved_other_foundation_groups=d['preserved_other_foundation_groups'],all_frozen_preparations_and_approved_assets_unchanged=True,
  no_original_source_tag_reclassification=True,native_execution_or_body_cable_articulation_changes=False,all_checks_pass=True)
 PROOF.write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8',newline='\n')
 print('SOLAR_STRUCTURAL_FOUNDATION_INDEPENDENT_PASS 16 foot columns,17 exclusions,192 triangles,1 actual FBX readback',flush=True)
if __name__=='__main__':main()
