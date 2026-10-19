"""#125 measured civic derivatives; original buildings and materials stay pinned."""
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
    inputs = json.loads((ROOT / 'ArtSource/Manifests/city_civic_ground_sources.json').read_text())
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    records = []
    for entry in inputs['assets']:
        for key in ('source', 'export'):
            assert digest(ROOT / entry[key]) == entry[key + '_sha256']
        if entry['source_object_missing']:
            # The historical damage blend predates this school export. Use
            # the pinned production FBX, not an invented source substitute.
            bpy.ops.object.select_all(action='DESELECT')
            bpy.ops.import_scene.fbx(filepath=str(ROOT / entry['export']))
            obj = next(o for o in bpy.context.selected_objects if o.type == 'MESH')
            bpy.context.view_layer.objects.active = obj
            bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
        else:
            obj = load_object(ROOT / entry['source'], entry['object'])
        before = obj.data
        slots = [m.name.split('.')[0] for m in before.materials]
        assert set(slots) == set(entry['bindings']), (entry['name'], slots)
        removed = set(entry['removed_faces'])
        keep = [p for p in before.polygons if p.index not in removed]
        used = sorted({v for p in keep for v in p.vertices})
        remap = {v: i for i, v in enumerate(used)}
        vertices = [tuple(before.vertices[i].co) for i in used]
        faces = [[remap[v] for v in p.vertices] for p in keep]
        additions = []
        trim = 'CharredTrim' if 'CharredTrim' in slots else 'Roof'
        pane = 'BuildingWindowOpaque' if 'BuildingWindowOpaque' in slots else trim

        def box(low, high, material, glass=False):
            x, y, z = low
            X, Y, Z = high
            start = len(vertices)
            vertices.extend(((x,y,z),(X,y,z),(X,Y,z),(x,Y,z),(x,y,Z),(X,y,Z),(X,Y,Z),(x,Y,Z)))
            faces.extend([[start+i for i in f] for f in ((0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7))])
            additions.extend([(slots.index(material), glass)] * 6)

        low, high = entry['door_bounds']
        x0, x1, front, back = low[0], high[0], low[1], high[1]
        floor, top = entry['door_floor_m'], high[2]
        # The structural threshold bridges back into the actual entrance
        # enclosure. The terrain walk starts on its measured outer top edge.
        box((x0-.15,front-.12,floor),(x1+.15,back+.1,floor+.08),trim)
        # Vertical jambs own the corners; horizontals butt into them. Recessed
        # glazing never shares a face with jambs, transom or centre separator.
        for x in (x0,x1-.10):
            box((x,front,floor+.08),(x+.10,back,top),trim)
        box((x0+.10,front,top-.10),(x1-.10,back,top),trim)
        transom = min(top-.10, floor+2.55)
        if top > floor+3.0:
            box((x0+.10,front,transom),(x1-.10,back,transom+.10),trim)
            box((x0+.10,front+.04,transom+.10),(x1-.10,back,top-.10),pane,True)
        center = (x0+x1)*.5
        box((center-.035,front,floor+.08),(center+.035,back,transom),trim)
        for a,b in ((x0+.10,center-.035),(center+.035,x1-.10)):
            box((a,front+.04,floor+.08),(b,back,transom),pane,True)
        for x in (center-.15,center+.10):
            box((x,front-.04,floor+1.0),(x+.05,front+.04,floor+1.45),trim)
        mesh = bpy.data.meshes.new(entry['target_name'])
        mesh.from_pydata(vertices, [], faces)
        for material in before.materials:
            mesh.materials.append(material)
        for layer in before.uv_layers:
            target = mesh.uv_layers.new(name=layer.name)
            for old, new in zip(keep, mesh.polygons):
                for a,b in zip(old.loop_indices,new.loop_indices):
                    target.data[b].uv = layer.data[a].uv
            for poly in list(mesh.polygons)[len(keep):]:
                for li,uv in zip(poly.loop_indices,((0,0),(1,0),(1,1),(0,1))):
                    target.data[li].uv = uv
        for layer in before.color_attributes:
            assert layer.domain == 'CORNER'
            target = mesh.color_attributes.new(name=layer.name,type=layer.data_type,domain='CORNER')
            for old,new in zip(keep,mesh.polygons):
                for a,b in zip(old.loop_indices,new.loop_indices):
                    target.data[b].color = layer.data[a].color
            for poly,(_,glass) in zip(list(mesh.polygons)[len(keep):],additions):
                for li in poly.loop_indices:
                    target.data[li].color = (1,1,1,0 if glass else 1)
        for old,new in zip(keep,mesh.polygons):
            new.material_index,new.use_smooth = old.material_index,old.use_smooth
        for poly,(slot,_) in zip(list(mesh.polygons)[len(keep):],additions):
            poly.material_index = slot
        if before.has_custom_normals:
            mesh.normals_split_custom_set([before.corner_normals[i].vector for p in keep for i in p.loop_indices] +
                [p.normal.copy() for p in list(mesh.polygons)[len(keep):] for _ in p.loop_indices])
        # FBX/Unreal drops unused slots. Remove the parked-beacon-only slot
        # explicitly so source and native manifests describe the same asset.
        used_slots = sorted({p.material_index for p in mesh.polygons})
        materials = [mesh.materials[i] for i in used_slots]
        remapped_slots = [used_slots.index(poly.material_index) for poly in mesh.polygons]
        mesh.materials.clear()
        for material in materials:
            mesh.materials.append(material)
        for poly,slot in zip(mesh.polygons,remapped_slots):
            poly.material_index = slot
        slots = [slots[i] for i in used_slots]
        obj.data,obj.name = mesh,entry['target_name']
        obj.location = (0,0,0)
        bpy.context.view_layer.update()
        half = [v*4 for v in entry['footprint_cells']]
        assert all(abs(v.co.x)<=half[0]+.001 and abs(v.co.y)<=half[1]+.001 for v in mesh.vertices),entry['name']
        mesh.calc_loop_triangles()
        export = ROOT/'ArtSource/Exports/CivicGround'/(obj.name+'.fbx')
        export.parent.mkdir(parents=True,exist_ok=True)
        with checked_fbx(obj,export,slots,verify_alpha=True):
            pass
        records.append(dict(entry,derived_export=export.relative_to(ROOT).as_posix(),derived_export_sha256=digest(export),
            target='/Game/CitySim/Art/CivicGround/'+obj.name,triangles=len(mesh.loop_triangles),
            retained_faces=len(keep),dimensions_m=list(obj.dimensions),slots=slots,bindings={slot:entry['bindings'][slot] for slot in slots}))
        for key in ('source','export'):
            assert digest(ROOT/entry[key]) == entry[key+'_sha256']
        print('CIVIC_GROUND_DERIVED',obj.name,len(mesh.loop_triangles))
    blend = ROOT/'ArtSource/Blender/CityCivicGround.blend'
    bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.save_as_mainfile(filepath=str(blend))
    doc = dict(issue=125,source=blend.relative_to(ROOT).as_posix(),source_sha256=digest(blend),assets=records,
        preserved='Original production source/exports unchanged; retained vertices, UVs, colors, normals, materials and body dimensions. Parcel slabs, rigid walks/driveways and fixed parked decorations removed. Door thresholds/detail added; college centre post removed from its doorway.')
    (ROOT/'ArtSource/Manifests/city_civic_ground.json').write_text(json.dumps(doc,indent=2),encoding='utf-8')
    print('CIVIC_GROUND_SOURCE_PASS',len(records))


if __name__ == '__main__':
    main()
