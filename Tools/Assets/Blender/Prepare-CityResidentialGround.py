"""#125 mechanical pathless derivatives of remaining low-rise homes.

Keep all porch/deck/post/body faces, UVs, corner colors and material bindings.
Only the six verified faces of each old1.3m ground walk are removed.
"""
from pathlib import Path
import hashlib
import json
import sys
import bpy

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'Tools'))
from CityBuildingStudyIO import checked_fbx, load_object

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    inputs = json.loads((ROOT / 'ArtSource/Manifests/city_residential_ground_sources.json').read_text())
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    records = []
    for entry in inputs['assets']:
        source, original_export = ROOT / entry['source'], ROOT / entry['export']
        assert digest(source) == entry['source_sha256']
        assert digest(original_export) == entry['export_sha256']
        obj = load_object(source, entry['object'])
        before = obj.data
        # B/C/D use split vertices, so component connectivity is insufficient.
        # This exact volume excludes their wider structural porch deck entirely.
        def in_walk(v):
            x, y, z = before.vertices[v].co
            return abs(x) <= .65001 and -8.00001 <= y <= -4.61999 and -.20001 <= z <= -.03999
        removed = [p.index for p in before.polygons if all(in_walk(v) for v in p.vertices)]
        assert len(removed) == 6, (entry['name'], removed)
        keep = [p for p in before.polygons if p.index not in removed]
        used = sorted({v for p in keep for v in p.vertices})
        remap = {v: i for i, v in enumerate(used)}
        mesh = bpy.data.meshes.new(entry['target_name'])
        mesh.from_pydata([before.vertices[i].co for i in used], [], [[remap[i] for i in p.vertices] for p in keep])
        for material in before.materials:
            mesh.materials.append(material)
        for layer in before.uv_layers:
            target = mesh.uv_layers.new(name=layer.name)
            for old, new in zip(keep, mesh.polygons):
                for a, b in zip(old.loop_indices, new.loop_indices):
                    target.data[b].uv = layer.data[a].uv
        for layer in before.color_attributes:
            assert layer.domain == 'CORNER'
            target = mesh.color_attributes.new(name=layer.name, type=layer.data_type, domain='CORNER')
            for old, new in zip(keep, mesh.polygons):
                for a, b in zip(old.loop_indices, new.loop_indices):
                    target.data[b].color = layer.data[a].color
        for old, new in zip(keep, mesh.polygons):
            new.material_index, new.use_smooth = old.material_index, old.use_smooth
        if before.has_custom_normals:
            mesh.normals_split_custom_set([before.corner_normals[i].vector for p in keep for i in p.loop_indices])
        obj.data, obj.name = mesh, entry['target_name']
        obj.location = (0, 0, 0)
        bpy.context.view_layer.update()
        mesh.calc_loop_triangles()
        export = ROOT / 'ArtSource/Exports/ResidentialGround' / (obj.name + '.fbx')
        export.parent.mkdir(parents=True, exist_ok=True)
        with checked_fbx(obj, export, entry['slots'], verify_alpha=True):
            pass
        assert len(mesh.loop_triangles) == sum(len(p.vertices) - 2 for p in before.polygons) - 12
        records.append(dict(entry, derived_export=export.relative_to(ROOT).as_posix(), derived_export_sha256=digest(export),
                            target='/Game/CitySim/Art/ResidentialGround/' + obj.name, triangles=len(mesh.loop_triangles),
                            removed_faces=removed, retained_faces=len(keep), dimensions_m=list(obj.dimensions)))
        assert digest(source) == entry['source_sha256']
        assert digest(original_export) == entry['export_sha256']
        print('RESIDENTIAL_GROUND_DERIVED', obj.name, len(mesh.loop_triangles))
    blend = ROOT / 'ArtSource/Blender/CityResidentialGroundPaths.blend'
    bpy.ops.wm.save_as_mainfile(filepath=str(blend))
    manifest = dict(issue=125, source=blend.relative_to(ROOT).as_posix(), source_sha256=digest(blend),
                    source_inputs='ArtSource/Manifests/city_residential_ground_sources.json', assets=records,
                    preserved='All retained positions/UVs/corner colors/materials and authored porches; approved originals unchanged')
    (ROOT / 'ArtSource/Manifests/city_residential_ground_paths.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print('RESIDENTIAL_GROUND_SOURCE_PASS', len(records))

if __name__ == '__main__':
    main()
