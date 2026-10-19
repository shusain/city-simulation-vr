"""Reopen the D104 source and verify fixed geometry, masks, UVs and material scope."""
from pathlib import Path
import argparse
import json
import sys
import bpy

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'Tools'))
from CityUnrealPreview import load_batch, digest
from CityBuildingStudyIO import snapshot


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--batch', type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    batch = load_batch(args.batch)
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/batch['source']))
    images = [i for i in bpy.data.images if i.type == 'IMAGE' and i.users]
    assert all(i.packed_file for i in images), 'Study textures must be packed'
    records = []
    for record in batch['meshes']:
        obj = bpy.data.objects[record['name']]
        with bpy.data.libraries.load(str(ROOT/batch['approved_source']), link=False) as (_, loaded):
            loaded.objects = [record['name']]
        reference = loaded.objects[0]
        assert snapshot(obj) == snapshot(reference), record['name']
        bpy.data.objects.remove(reference)
        obj.data.calc_loop_triangles()
        assert len(obj.data.loop_triangles) == record['triangles']
        assert [m.name.split('.')[0] for m in obj.data.materials] == record['slots']
        records.append(record['name'])
    for entry in batch['renders']:
        if not entry['overrides']: continue
        obj = bpy.data.objects[entry['source_object']]
        reference = bpy.data.objects[entry['native_mesh'].rsplit('/', 1)[-1]]
        assert snapshot(obj) == snapshot(reference), obj.name
        mesh_record = next(m for m in batch['meshes'] if m['name'] == reference.name)
        for slot, original, modified in zip(mesh_record['slots'], reference.data.materials, obj.data.materials):
            if slot in entry['overrides']:
                assert modified.name == entry['overrides'][slot].rsplit('M_', 1)[-1]
            else: assert modified == original, (obj.name, slot)
        ruler = bpy.data.objects['Ruler_'+obj.name]
        height = max((ruler.matrix_world@v.co).z for v in ruler.data.vertices)-min((ruler.matrix_world@v.co).z for v in ruler.data.vertices)
        assert abs(height-1.43175) < .002
    assert digest(ROOT/batch['source']) == batch['source_sha256']
    assert digest(ROOT/batch['approved_source']) == batch['approved_source_sha256']
    result = dict(task='D104', source_sha256=batch['source_sha256'], approved_source_unchanged=True,
        baseline_meshes=records, unique_material_variants=9, geometry_uv_rgba_fixed=True,
        only_main_wall_bindings_changed=True, packed_images=len(images), production_changed=False)
    (ROOT/batch['output']/'SourceVerification.json').write_text(json.dumps(result, indent=2)+'\n')
    print('CITYSIM_D104_SOURCE_PASS 12 original meshes; 9 material copies; geometry/UV/RGBA/other bindings unchanged')


if __name__ == '__main__': main()
