"""#125 commercial pathless derivatives; preserve storefront geometry and materials."""
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


def walk_faces(mesh):
    # Identify the eight exact corners, avoiding the overlapping thin doorway
    # sill on detailed shops. Topological components fail for split UV corners.
    def corner(v):
        x,y,z=mesh.vertices[v].co
        return abs(abs(x)-1.5)<.0001 and min(abs(y+10.4),abs(y+7))<.0001 and min(abs(z),abs(z-.16))<.0001
    faces=[p.index for p in mesh.polygons if all(corner(v) for v in p.vertices)]
    assert len(faces)==6,faces
    return faces


def main():
    inputs = json.loads((ROOT / 'ArtSource/Manifests/city_commercial_ground_sources.json').read_text())
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    records = []
    for entry in inputs['assets']:
        source, original_export = ROOT / entry['source'], ROOT / entry['export']
        assert digest(source) == entry['source_sha256']
        assert digest(original_export) == entry['export_sha256']
        obj = load_object(source, entry['object'])
        before = obj.data
        removed = walk_faces(before)
        # Detailed low-rise doors and their thin sills are retained. Other
        # shops replace only the plain 3x4m doorway, as for the apartments.
        if not entry.get('appearance'):
            y = -entry['authored_entry_native_cm'][1] / 100
            panels = [p.index for p in before.polygons if p.index not in removed and all(
                abs(before.vertices[v].co.x) <= 1.50001 and
                y - .00001 <= before.vertices[v].co.y <= y + .30001 and
                -.00001 <= before.vertices[v].co.z <= 4.00001 for v in p.vertices)]
            assert len(panels) == 6, panels
            removed += panels
        keep = [p for p in before.polygons if p.index not in removed]
        used = sorted({v for p in keep for v in p.vertices})
        remap = {v: i for i, v in enumerate(used)}
        mesh = bpy.data.meshes.new(entry['target_name'])
        vertices = [tuple(before.vertices[i].co) for i in used]
        faces = [[remap[i] for i in p.vertices] for p in keep]
        additions = []
        def box(low, high, slot):
            start = len(vertices)
            x, y, z = low; X, Y, Z = high
            vertices.extend([(x,y,z),(X,y,z),(X,Y,z),(x,Y,z),(x,y,Z),(X,y,Z),(X,Y,Z),(x,Y,Z)])
            faces.extend([[start+i for i in f] for f in ((0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7))])
            additions.extend([entry['slots'].index(slot)]*6)
        if not entry.get('appearance'):
            front = -entry['authored_entry_native_cm'][1] / 100
            trim = 'Roof' if entry.get('appearance') else 'CharredTrim' if entry['damaged'] else 'Roof'
            pane = 'CharredRoofMembrane' if entry['damaged'] else 'RoofMembrane'
            # Full-height jambs own the corners; every horizontal butts into
            # them. Panes are recessed 4cm and do not intersect the framing.
            for x in (-1.5, 1.35): box((x,front,0),(x+.15,front+.30,4),trim)
            for z, Z in ((0,.12),(2.7,2.84),(3.85,4)):
                box((-1.35,front,z),(1.35,front+.30,Z),trim)
            box((-.05,front,.12),(.05,front+.30,2.7),trim)
            for x, X in ((-1.35,-.05),(.05,1.35)):
                box((x,front+.04,.12),(X,front+.30,2.7),pane)
            box((-1.35,front+.04,2.84),(1.35,front+.30,3.85),pane)
            # Pull plates are attached to the leaves below hand height;
            # their outer faces stay within the measured threshold plane.
            for x in (-.20,.14): box((x,front,1.05),(x+.06,front+.04,1.55),trim)
        elif entry['appearance'].startswith('D'):
            # Recess the complete vestibule assembly20cm into the facade.
            # Retain a genuine structural floor beneath it, independently of
            # the removed terrain walk. Canopy geometry stays unchanged.
            door_vertices = {remap[v] for p in keep if all(
                abs(before.vertices[v].co.x)<=1.62501 and
                -8.10001<=before.vertices[v].co.y<=-6.99999 and
                -.00001<=before.vertices[v].co.z<=3.40001 for v in p.vertices) for v in p.vertices}
            assert len(door_vertices)>100, len(door_vertices)
            for i in door_vertices:
                x,y,z=vertices[i]; vertices[i]=(x,y+.2,z)
        if entry.get('appearance'):
            half=entry['door_width_cm']/200
            front=(entry['authored_entry_native_cm'][1]-entry['vestibule_recess_cm'])/100
            box((-half,-front,0),(half,-7,.16),'Roof')
        # A70cm rear setback fits the retained rear roof and the complete
        # private stair width inside a2x2 plot even beside the wider avenue.
        vertices = [(x,y+.7,z) for x,y,z in vertices]
        mesh.from_pydata(vertices, [], faces)
        for material in before.materials:
            mesh.materials.append(material)
        for layer in before.uv_layers:
            target = mesh.uv_layers.new(name=layer.name)
            for old, new in zip(keep, mesh.polygons):
                for a, b in zip(old.loop_indices, new.loop_indices):
                    target.data[b].uv = layer.data[a].uv
            for poly in list(mesh.polygons)[len(keep):]:
                for loop, uv in zip(poly.loop_indices, ((0,0),(1,0),(1,1),(0,1))): target.data[loop].uv = uv
        for layer in before.color_attributes:
            assert layer.domain == 'CORNER'
            target = mesh.color_attributes.new(name=layer.name, type=layer.data_type, domain='CORNER')
            for old, new in zip(keep, mesh.polygons):
                for a, b in zip(old.loop_indices, new.loop_indices):
                    target.data[b].color = layer.data[a].color
            for poly in list(mesh.polygons)[len(keep):]:
                for loop in poly.loop_indices: target.data[loop].color = (1,1,1,1)
        for old, new in zip(keep, mesh.polygons):
            new.material_index, new.use_smooth = old.material_index, old.use_smooth
        for poly, slot in zip(list(mesh.polygons)[len(keep):], additions): poly.material_index = slot
        if before.has_custom_normals:
            mesh.normals_split_custom_set([before.corner_normals[i].vector for p in keep for i in p.loop_indices] +
                [p.normal.copy() for p in list(mesh.polygons)[len(keep):] for _ in p.loop_indices])
        obj.data, obj.name = mesh, entry['target_name']
        obj.location = (0, 0, 0)
        bpy.context.view_layer.update()
        assert all(abs(v.co.x)<=8.0001 and abs(v.co.y)<=8.0001 for v in mesh.vertices), entry['name']
        mesh.calc_loop_triangles()
        export = ROOT / 'ArtSource/Exports/CommercialGround' / (obj.name + '.fbx')
        export.parent.mkdir(parents=True, exist_ok=True)
        with checked_fbx(obj, export, entry['slots'], verify_alpha=True):
            pass
        assert len(mesh.loop_triangles) == sum(len(p.vertices) - 2 for p in before.polygons) - len(removed)*2 + len(additions)*2
        records.append(dict(entry, derived_export=export.relative_to(ROOT).as_posix(), derived_export_sha256=digest(export),
                            target='/Game/CitySim/Art/CommercialGround/' + obj.name, triangles=len(mesh.loop_triangles),
                            removed_faces=removed, retained_faces=len(keep), entrance_detail_boxes=len(additions)//6, dimensions_m=list(obj.dimensions)))
        assert digest(source) == entry['source_sha256'] and digest(original_export) == entry['export_sha256']
        print('COMMERCIAL_GROUND_DERIVED', obj.name, len(mesh.loop_triangles))
    blend = ROOT / 'ArtSource/Blender/CityCommercialGround.blend'
    bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.save_as_mainfile(filepath=str(blend))
    # Native low-rise business copies differ only in material bindings. Reuse
    # each checked FBX; preserve the accepted room/sign identity explicitly.
    rooms = json.loads((ROOT / 'ArtSource/Derived/StorefrontRooms/manifest.json').read_text())['assets']
    room_by_mesh = {mesh: room['native_material'] for room in rooms for mesh in room['native_meshes']}
    variants = []
    for entry in records:
        if entry['native'] in room_by_mesh:
            entry['bindings']['BuildingWindowOpaque'] = room_by_mesh[entry['native']]
        if not entry.get('appearance') or entry['damaged']:
            continue
        for room in rooms:
            if room['key'] == 'market': continue
            row = dict(entry)
            row['bindings'] = dict(entry['bindings'])
            for key in ('native', 'name', 'target_name', 'target'):
                row[key] += '_' + room['key']
            row['bindings']['StoreSignTall'] = '/Game/CitySim/Art/LowDensity/Materials/MI_Sign_' + room['key']
            row['bindings']['BuildingWindowOpaque'] = room['native_material']
            row['shared_geometry'] = entry['target']
            variants.append(row)
    manifest = dict(issue=125, source=blend.relative_to(ROOT).as_posix(), source_sha256=digest(blend), assets=records+variants,
                    source_objects=len(records),
                    preserved='Original sources unchanged; retained UVs/colors/materials, body/awning/canopy shapes and business identity. Derived bodies translated70cm rearward; D vestibule recessed20cm; structural entrance floors retained separately from terrain walks.')
    (ROOT / 'ArtSource/Manifests/city_commercial_ground.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print('COMMERCIAL_GROUND_SOURCE_PASS', len(records))


if __name__ == '__main__':
    main()
