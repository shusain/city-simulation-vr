"""Author D60 meshes. Render/review the saved source before running --export."""
from pathlib import Path
import sys
import json
import bpy
import bmesh

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from CityArtKit import (
    MATERIALS, box, combine, export_one, material, wheel, initialize_materials,
)
initialize_materials()
MATERIALS['ParkedBeacon'] = material('ParkedBeacon', (.1,.14,.19))
MATERIALS['MedicalRed'] = material('MedicalRed', (.75, .10, .065))
MATERIALS['Hospital'] = material('Hospital', (.88, .88, .83))
MATERIALS['Charred'] = material('Charred', (.19, .15, .13))
MATERIALS['CharredTrim'] = material('CharredTrim', (.065, .06, .055))
source = root / 'ArtSource/Blender/CityHospitalKit.blend'


def cross(parts, name, location, slot, size=1.8):
    x, y, z = location
    parts.extend([box(name+'_CrossV', (size*.28,.14,size), (x,y,z), slot),
                  box(name+'_CrossH', (size,.15,size*.28), (x,y-.01,z), slot)])


def ambulance(name, parked=False):
    detail = 'ParkedBeacon' if parked else 'Roof'
    parts = [box(name+'_Body', (5.6,2.1,1.7), (-.5,0,1.25), 'Hospital'),
             box(name+'_Cab', (1.7,2,1.2), (2,0,1.15), 'Hospital'),
             box(name+'_Windscreen', (.14,1.7,.65), (2.86,0,1.6), detail),
             box(name+'_Bumper', (.25,2.1,.28), (2.94,0,.5), detail),
             box(name+'_Bar', (.35,1.6,.16), (1.7,0,2.02), detail)]
    for side, color in ((-.55,(1,.015,.01,0)),(.55,(.01,.12,1,0))):
        lamp = box(name+'_Lamp', (.45,.6,.3), (1.7,side,2.2), detail)
        colors = lamp.data.color_attributes.new(name='WindowMask', type='BYTE_COLOR', domain='CORNER')
        for value in colors.data:
            value.color = color
        parts.append(lamp)
    for y in (-1.06,1.06):
        parts.append(box(name+'_Stripe', (4.5,.045,.22), (-.6,y,1), detail))
        cross(parts, name, (-.7,y,1.45), 'MedicalRed', size=.9)
    for x in (-2.1,1.9):
        for y in (-1.07,1.07):
            parts.append(wheel(name+'_Wheel', .4,.22,(x,y,.4)))
    return combine(name, parts)


def hospital(name, upgraded=False, damaged=False):
    body = 'Charred' if damaged else 'Hospital'
    trim = 'CharredTrim' if damaged else 'Roof'
    parts = [box(name+'_Lot', (32,24,.22), (0,0,.11), trim),
             box(name+'_Ward', (18,12,7), (-5,3,3.72), body),
             box(name+'_Roof', (18.6,12.6,.5), (-5,3,7.47), trim),
             box(name+'_Entry', (4,2.5,3.2), (-5,-4.2,1.82), body),
             box(name+'_Canopy', (6,3.5,.35), (-5,-4.5,3.6), trim),
             box(name+'_Door', (2,.15,2.6), (-5,-5.53,1.52), trim),
             box(name+'_Walk', (5,6,.12), (-5,-9,.28), body),
             box(name+'_Sign', (3.8,.2,2.3), (-5,-3.14,5.5), trim)]
    cross(parts, name, (-5,-3.3,5.5), body if damaged else 'MedicalRed')
    for x in (-11,-8,-2,1):
        for y in (-3.06,9.06):
            pane = box(name+'_Window', (1.8,.1,1.8), (x,y,4.4), trim)
            if not damaged:
                colors = pane.data.color_attributes.new(name='WindowMask', type='BYTE_COLOR', domain='CORNER')
                for value in colors.data:
                    value.color = (1,1,1,0)
            parts.append(pane)
    if upgraded:
        parts += [box(name+'_Wing', (8,12,10), (9,3,5.22), body),
                  box(name+'_WingRoof', (8.6,12.6,.5), (9,3,10.47), trim)]
        for x in (7,11):
            for z in (4,7.5):
                pane=box(name+'_WingWindow', (1.8,.12,1.8), (x,-3.08,z), trim)
                if not damaged:
                    colors=pane.data.color_attributes.new(name='WindowMask',type='BYTE_COLOR',domain='CORNER')
                    for value in colors.data: value.color=(1,1,1,0)
                parts.append(pane)
    if damaged:
        # Closed solids remain opaque from above; exposed charred beams add damage
        # without deleting roof faces or introducing single-sided roof patches.
        for x in (-10,-5,0):
            parts.append(box(name+'_Scorch', (1,7,.18), (x,3,7.82), body))
    else:
        parked = ambulance(name+'_Parked', parked=True)
        parked.rotation_euler.z = -1.57079632679
        parked.location = (9,-7,.22)
        bpy.context.view_layer.objects.active = parked
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
        parts.append(parked)
    for x in (6.5,11.5):
        parts.append(box(name+'_ParkingLine', (.12,7,.04), (x,-7,.25), body))
    return combine(name, parts)


def validate(obj):
    obj.data.calc_loop_triangles()
    count = len(obj.data.loop_triangles)
    if count > (800 if 'Building' in obj.name else 500) or len(obj.data.materials) != (2 if 'Damaged' in obj.name else 3 if 'Traffic' in obj.name else 4):
        raise RuntimeError(f'{obj.name}: geometry budget/slots failed: {count}')
    mesh = bmesh.new()
    mesh.from_mesh(obj.data)
    if any(not edge.is_manifold for edge in mesh.edges):
        raise RuntimeError(f'{obj.name}: open geometry')
    if mesh.calc_volume(signed=True) <= 0:
        raise RuntimeError(f'{obj.name}: inverted geometry')
    mesh.free()
    return count


if '--export' in sys.argv:
    bpy.ops.wm.open_mainfile(filepath=str(source))
    for obj in bpy.context.scene.objects:
        if obj.type == 'MESH':
            preview = root / 'Artifacts/ArtPreview/D60' / (obj.name+'.png')
            if not preview.exists() or preview.stat().st_mtime < source.stat().st_mtime:
                raise RuntimeError('Render and review the current source before export: '+obj.name)
else:
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    bpy.context.scene.unit_settings.system = 'METRIC'
    bpy.context.scene.unit_settings.scale_length = 1
    hospital('SM_Building_Hospital_A')
    hospital('SM_Building_Hospital_Medium_A', True)
    hospital('SM_Building_Hospital_Damaged_A', damaged=True)
    hospital('SM_Building_Hospital_Medium_Damaged_A', True, True)
    ambulance('SM_Traffic_Ambulance_A')
    for obj in bpy.context.scene.objects:
        validate(obj)
    bpy.ops.wm.save_as_mainfile(filepath=str(source))

entries = []
measurements = []
for obj in bpy.context.scene.objects:
    if obj.type != 'MESH':
        continue
    count = validate(obj)
    measurements.append({'name': obj.name, 'triangles': count, 'slots': [m.name for m in obj.data.materials], 'closed_solids': True})
    if '--export' not in sys.argv:
        continue
    export_one(obj)
    damaged = 'Damaged' in obj.name
    vehicle = 'Traffic' in obj.name
    entries.append({'asset_id': 'traffic.ambulance.a' if vehicle else 'building.hospital.'+('medium.' if 'Medium' in obj.name else '')+('damaged.' if damaged else '')+'a',
                    'hospital_asset': True, 'civic_service': 'hospital', 'damaged_variant': damaged,
                    'source': 'ArtSource/Blender/CityHospitalKit.blend', 'export': 'ArtSource/Exports/'+obj.name+'.fbx',
                    'dimensions_m': [round(v,4) for v in obj.dimensions], 'footprint_cells': [1,1] if vehicle else [4,3],
                    'mesh_destination': '/Game/CitySim/Art/Meshes/'+obj.name,
                    'material_slots': {m.name: '/Game/CitySim/Art/Materials/MI_City'+({'ParkedBeacon':'PoliceDetail','MedicalRed':'Fire','Hospital':'Traffic','Roof':'PoliceDetail' if vehicle else 'WindowDetail','Charred':'Charred','CharredTrim':'CharredTrim'}[m.name]) for m in obj.data.materials},
                    'lod_policy': 'one mesh; below800 building/500 vehicle triangles', 'collision_policy': 'none; logical footprint is authoritative',
                    'pivot_policy': 'ground center', 'provenance': 'Original Blender primitives; Create-CityHospitalArt.py',
                    'entrance_sides_local': ['-Y'], 'presentation_yaw_offset_quarter_turns': 2})
report = root / 'Artifacts/AssetImport/D60-hospital-geometry.json'
report.parent.mkdir(parents=True, exist_ok=True)
report.write_text(json.dumps(measurements,indent=2),encoding='utf-8')
if entries:
    path = root / 'ArtSource/Manifests/city_starter_kit.json'
    manifest = json.loads(path.read_text(encoding='utf-8'))
    replacements = {e['asset_id']: e for e in entries}
    manifest['assets'] = [replacements.pop(e['asset_id'], e) for e in manifest['assets']] + list(replacements.values())
    path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
