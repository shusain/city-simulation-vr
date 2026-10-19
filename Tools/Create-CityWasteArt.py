"""Author D61 waste assets; render/review source before --export."""
from pathlib import Path
import sys
import json
import math
import bpy
import bmesh
from mathutils import Vector

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from CityArtKit import (
    MATERIALS, beam, box, combine, export_one, material, wheel, initialize_materials,
)
initialize_materials()
MATERIALS['Charred'] = material('Charred', (.19,.15,.13))
MATERIALS['CharredTrim'] = material('CharredTrim', (.065,.06,.055))
source = root/'ArtSource/Blender/CityWasteKit.blend'


def truck(name):
    # Rear-loading compactor, separate from tanker/fire-engine silhouettes.
    parts = [box(name+'_Chassis', (7,2.2,.4), (0,0,.7), 'Roof'),
             box(name+'_Compactor', (4.8,2.1,2.3), (-.8,0,1.9), 'Service'),
             box(name+'_Cab', (1.8,2,1.7), (2.5,0,1.3), 'Service'),
             box(name+'_Screen', (.12,1.6,.65), (3.45,0,1.75), 'Roof'),
             box(name+'_Hopper', (.8,2.35,1), (-3.5,0,1), 'Roof'),
             box(name+'_RearPress', (.18,1.9,1.2), (-3.25,0,2), 'Roof'),
             box(name+'_Beacon', (.35,.45,.22), (2.5,0,2.3), 'Roof')]
    for y in (-1.08,1.08):
        for x in (-2,-.6,.8):
            parts.append(box(name+'_Rib', (.12,.1,2.1), (x,y,1.9), 'Roof'))
    for x in (-2.3,-.8,2.5):
        for y in (-1.12,1.12):
            parts.append(wheel(name+'_Wheel', .43,.24,(x,y,.43)))
    return combine(name, parts)


def recycling_symbol(parts, name, location, slot):
    # Three closed arrow shapes around a triangle; no decals/transparency.
    origin = Vector(location)
    points = [Vector((-1.2,0,-.7)), Vector((1.2,0,-.7)), Vector((0,0,1.3))]
    for i in range(3):
        a = points[i]
        b = points[(i+1)%3]
        direction = (b-a).normalized()
        parts.append(beam(name+'_ArrowShaft', origin+a, origin+b-direction*.35, .25, slot))
        perpendicular = Vector((-direction.z,0,direction.x))
        for sign in (-1,1):
            parts.append(beam(name+'_ArrowHead', origin+b, origin+b-direction*.6+perpendicular*sign*.45, .25, slot))


def facility(name, recycling=False, upgraded=False, damaged=False):
    body = 'Charred' if damaged else ('Service' if recycling else 'Industrial')
    trim = 'CharredTrim' if damaged else 'Roof'
    parts = [box(name+'_Lot', (32,24,.24), (0,0,.12), trim),
             box(name+'_Hall', (17,12,6.2), (-6,3,3.34), body),
             box(name+'_Roof', (17.6,12.6,.5), (-6,3,6.69), trim),
             box(name+'_Office', (4,3,3), (-11,-4,1.74), body),
             box(name+'_OfficeRoof', (4.4,3.4,.25), (-11,-4,3.37), trim),
             box(name+'_OfficeWindow', (2.6,.12,1), (-11,-5.56,2.1), trim),
             box(name+'_LoadingDoor', (5,.14,4), (-2,-3.08,2.24), trim),
             box(name+'_Sign', (4,.25,3), (-9,-3.18,4.7), trim)]
    if recycling:
        recycling_symbol(parts,name,(-9,-3.4,4.7),body)
        # Sorting conveyor and three bays remain readable at tabletop scale.
        if not upgraded:
            parts += [box(name+'_Conveyor', (5.5,1.5,.55), (6,1,1.25), trim),
                      box(name+'_ConveyorStand', (1,1.3,1), (6,1,.74), body)]
        for x,y in ((7.5,10.5),(11.5,10.5)) if upgraded else ((8,5),(8,8)):
            parts += [box(name+'_Bale', (3.2,2,1.6), (x,y,1.04), body),
                      box(name+'_BaleBand', (.3,2.1,1.7), (x,y,1.09), trim)]
    else:
        # A bin pictogram and enclosed waste containers distinguish disposal.
        parts += [box(name+'_BinIcon', (1.8,.16,1.6), (-9,-3.42,4.55), body),
                  box(name+'_BinLid', (2.2,.18,.25), (-9,-3.44,5.55), body)]
        for y in (10.25,) if upgraded else (3,8):
            parts += [box(name+'_Container', (6,3.5,2.8), (7,y,1.64), body),
                      box(name+'_ContainerLid', (6.2,3.7,.25), (7,y,3.17), trim)]
    if upgraded:
        parts += [box(name+'_ProcessingWing', (7,10,9), (10,3,4.74), body),
                  box(name+'_WingRoof', (7.6,10.6,.5), (10,3,9.49), trim),
                  box(name+'_Duct', (4,2,1), (3.5,3,6.7), trim)]
    if damaged:
        for x in (-11,-6,-1):
            parts.append(box(name+'_Scorch', (.8,7,.2), (x,3,7.04), body))
    else:
        parked = truck(name+'_Parked')
        # Building uses only its two semantic materials, including decoration.
        for i, mat in enumerate(parked.data.materials):
            parked.data.materials[i] = MATERIALS[trim if mat.name == 'Roof' else body]
        parked.rotation_euler.z = -math.pi/2
        parked.location = (7,-7,.24)
        bpy.context.view_layer.objects.active = parked
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
        parts.append(parked)
    for x in (4.5,9.5):
        parts.append(box(name+'_ParkingLine', (.12,8,.04), (x,-7,.28), body))
    return combine(name,parts)


def validate(obj):
    obj.data.calc_loop_triangles()
    triangles = len(obj.data.loop_triangles)
    if triangles > (800 if 'Building' in obj.name else 500) or len(obj.data.materials) != 2:
        raise RuntimeError(f'{obj.name}: triangle/slot budget failed: {triangles}')
    mesh = bmesh.new()
    mesh.from_mesh(obj.data)
    if any(not edge.is_manifold for edge in mesh.edges) or mesh.calc_volume(signed=True) <= 0:
        raise RuntimeError(obj.name+': open/inverted geometry')
    mesh.free()
    return triangles


if '--export' in sys.argv:
    bpy.ops.wm.open_mainfile(filepath=str(source))
    for obj in bpy.context.scene.objects:
        if obj.type == 'MESH':
            preview = root/'Artifacts/ArtPreview/D61'/(obj.name+'.png')
            if not preview.exists() or preview.stat().st_mtime < source.stat().st_mtime:
                raise RuntimeError('Render/review the current source before export: '+obj.name)
else:
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    bpy.context.scene.unit_settings.system = 'METRIC'
    bpy.context.scene.unit_settings.scale_length = 1
    for recycling, label in ((False,'WasteDepot'),(True,'Recycling')):
        for upgraded in (False,True):
            for damaged in (False,True):
                name = 'SM_Building_'+label+('_Medium' if upgraded else '')+('_Damaged' if damaged else '')+'_A'
                facility(name,recycling,upgraded,damaged)
    truck('SM_Traffic_GarbageTruck_A')
    for obj in bpy.context.scene.objects:
        validate(obj)
    bpy.ops.wm.save_as_mainfile(filepath=str(source))

entries = []
measurements = []
for obj in bpy.context.scene.objects:
    if obj.type != 'MESH':
        continue
    triangles = validate(obj)
    measurements.append({'name':obj.name,'triangles':triangles,'slots':[m.name for m in obj.data.materials],'closed_solids':True})
    if '--export' not in sys.argv:
        continue
    export_one(obj)
    vehicle = 'Traffic' in obj.name
    family = 'recycling' if 'Recycling' in obj.name else 'wastedepot'
    asset_id = 'traffic.garbagetruck.a' if vehicle else 'building.'+family+'.'+('medium.' if 'Medium' in obj.name else '')+('damaged.' if 'Damaged' in obj.name else '')+'a'
    entries.append({'asset_id':asset_id,'waste_asset':True,'civic_service':'waste','damaged_variant':'Damaged' in obj.name,
                    'source':'ArtSource/Blender/CityWasteKit.blend','export':'ArtSource/Exports/'+obj.name+'.fbx',
                    'dimensions_m':[round(v,4) for v in obj.dimensions],'footprint_cells':[1,1] if vehicle else [4,3],
                    'mesh_destination':'/Game/CitySim/Art/Meshes/'+obj.name,
                    'material_slots':{m.name:'/Game/CitySim/Art/Materials/MI_City'+m.name for m in obj.data.materials},
                    'lod_policy':'one mesh; below800 building/500 vehicle triangles','collision_policy':'none; logical footprint is authoritative',
                    'pivot_policy':'ground center','provenance':'Original Blender primitives; Create-CityWasteArt.py',
                    'entrance_sides_local':['-Y'],'presentation_yaw_offset_quarter_turns':2})
report = root/'Artifacts/AssetImport/D61-waste-geometry.json'
report.parent.mkdir(parents=True,exist_ok=True)
report.write_text(json.dumps(measurements,indent=2),encoding='utf-8')
if entries:
    path = root/'ArtSource/Manifests/city_starter_kit.json'
    manifest = json.loads(path.read_text(encoding='utf-8'))
    manifest['assets'] = [e for e in manifest['assets'] if not e.get('waste_asset')]+entries
    path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
