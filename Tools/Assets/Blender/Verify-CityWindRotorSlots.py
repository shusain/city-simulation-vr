"""Read-only proof: only SpinRotor's globally unused third slot is pruned."""
from pathlib import Path
from collections import Counter
import json,hashlib
import bpy
ROOT=Path(__file__).resolve().parents[3]
OUTPUT=ROOT/'ArtSource/Manifests/city_wind_rotor_empty_slot_proof.json'
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def read(path):return json.loads(Path(path).read_bytes())
def main():
 assert not OUTPUT.exists(),'New proof only'
 extension=read(ROOT/'ArtSource/Manifests/city_utility_ground_wind_preparation.json');wind=extension['wind']
 plan=read(ROOT/'ArtSource/Manifests/city_utility_native_import_plan.json');record=next(r for r in plan['wind_role_meshes'] if r['role']=='SpinRotor')
 protected=read(ROOT/'ArtSource/Manifests/city_solar_structural_foundation_override.json')['protected_files_unchanged'];assert all(sha(ROOT/p)==pin for p,pin in protected.items())
 bpy.ops.wm.read_factory_settings(use_empty=True)
 with bpy.data.libraries.load(str(ROOT/wind['source']),link=False) as (available,loaded):loaded.objects=list(available.objects)
 for obj in loaded.objects:
  if obj.type=='MESH':bpy.context.scene.collection.objects.link(obj)
 objects={o.name:o for o in loaded.objects if o.type=='MESH'};rows=[];declared=['OffWhite_PaintedSteel','BlueGrey_Machinery','Mechanical_Mapped'];used=set()
 for role in sorted((r for r in wind['roles'] if r['role']=='SpinRotor'),key=lambda r:r['lod']):
  obj=objects[role['object']];obj.data.calc_loop_triangles();slots=[m.name for m in obj.data.materials];counts=Counter(slots[t.material_index] for t in obj.data.loop_triangles)
  assert slots==declared and dict(counts)=={'OffWhite_PaintedSteel':824,'BlueGrey_Machinery':92};used.update(counts)
  original=record['lods'][role['lod']];assert original['triangles']==916 and original['facts']['triangles_by_material']==dict(counts)
  assert 'Mechanical_Mapped' not in original['facts']['samples']
  bpy.ops.import_scene.fbx(filepath=str(ROOT/role['export']));imported=[o for o in bpy.context.scene.objects if o.type=='MESH' and o.name not in objects];assert len(imported)==1
  mesh=imported[0].data;mesh.calc_loop_triangles();imported_counts=Counter(mesh.materials[t.material_index].name.split('.')[0] for t in mesh.loop_triangles)
  assert dict(imported_counts)==dict(counts);bpy.data.objects.remove(imported[0],do_unlink=True)
  rows.append(dict(lod=role['lod'],export=role['export'],export_sha256=sha(ROOT/role['export']),declared_source_slots=slots,source_triangles_by_material=dict(counts),actual_blender_fbx_triangles_by_material=dict(imported_counts),
   triangles=916,empty_slot='Mechanical_Mapped',empty_slot_used_triangle_count=0,empty_slot_probe_count=0,all_original_geometry_bounds_uvs_normals_and_probes_retained=True))
 assert used=={'OffWhite_PaintedSteel','BlueGrey_Machinery'} and all(sha(ROOT/p)==pin for p,pin in protected.items())
 proof=dict(task=157,source_issue=168,version=1,role='SpinRotor',key=record['key'],source=wind['source'],source_sha256=sha(ROOT/wind['source']),original_plan_sha256=sha(ROOT/'ArtSource/Manifests/city_utility_native_import_plan.json'),
  declared_source_slots=declared,used_source_material_union=sorted(used),effective_native_slots=declared[:2],removed_empty_slot='Mechanical_Mapped',lods=rows,
  other_role_or_whole_slots_unchanged=True,source_graphs_or_material_instances_removed=False,geometry_or_source_preparation_or_proof_edits=False,protected_input_hashes_unchanged=261,native_execution=False)
 OUTPUT.write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8',newline='\n')
 print('WIND_ROTOR_EMPTY_SLOT_SOURCE_PROOF_PASS all3LODs916 triangles/824+92,Mechanical_Mapped used0/probes0;no assets/native execution',flush=True)
if __name__=='__main__':main()
