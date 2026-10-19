"""D79 focused Blender authoring, without touching Unreal or benchmark packages.

Run with Blender --background --factory-startup --python this-file.
Review Artifacts/ArtPreview/D79, then run again with -- --publish to promote
the already validated FBXs/manifest. Only the fourteen affected entries change.
The source kit is separate so other authoring work remains intact.
"""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from Assets.Manifests.CityAssetCapabilities import set_capability
import bpy, bmesh, json, math, shutil, sys, hashlib
from mathutils import Vector

root = Path(__file__).resolve().parents[1]
stage = root / 'Artifacts/D79'
out = root / 'Artifacts/ArtPreview/D79'
manifest_path = root / 'ArtSource/Manifests/city_starter_kit.json'
source = root / 'ArtSource/Blender/CityBuildingRefinement.blend'
stage.mkdir(parents=True, exist_ok=True)
out.mkdir(parents=True, exist_ok=True)
manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
base_ids = ('building.residential.high.a', 'building.power.a', 'building.service.a')
selected = [e for e in manifest['assets'] if e['asset_id'] in base_ids
            or e['asset_id'] in tuple(x+'.damaged' for x in base_ids)
            or e.get('utility_upgrade')]
assert len(selected) == 14

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

if '--publish' in sys.argv:
    draft = json.loads((stage/'draft.json').read_text(encoding='utf-8'))
    roundtrip = json.loads((stage/'roundtrip.json').read_text(encoding='utf-8'))
    validated = {m['sha256'] for m in roundtrip if m['roundtrip_pass']}
    assert set(draft['export_hashes'].values()).issubset(validated), 'Round-trip staged exports before promotion'
    assert digest(source) == draft['source_sha256']
    assert digest(manifest_path) == draft['manifest_sha256'], 'Manifest changed since authoring; regenerate draft'
    original_hashes = draft['original_export_hashes']
    publishing = [e for e in draft['assets'] if not '--residential-only' in sys.argv
                  or e['asset_id'].startswith('building.residential.high.a')]
    for e in draft['assets']:
        # A retry may find an already promoted file after an interrupted copy.
        # Accept only the exact original or exact validated staged content.
        assert digest(root/e['export']) in (original_hashes[e['asset_id']], draft['export_hashes'][e['asset_id']]), 'Export changed since authoring'
        assert digest(stage/'Exports'/Path(e['export']).name) == draft['export_hashes'][e['asset_id']]
    for e in publishing:
        target = root/e['export']
        if digest(target) == draft['export_hashes'][e['asset_id']]: continue
        # Copy before replacing to avoid truncating a live export on failure.
        temporary = target.with_suffix('.fbx.tmp')
        shutil.copy2(stage/'Exports'/target.name, temporary)
        temporary.replace(target)
    assert digest(manifest_path) == draft['manifest_sha256'], 'Manifest changed during promotion'
    replacement = {e['asset_id']:e for e in publishing}
    manifest['assets'] = [replacement.get(e['asset_id'], e) for e in manifest['assets']]
    manifest_path.write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    print('CITYSIM_D79_PROMOTED', len(replacement))
    sys.exit(0)

# Keep a first-run backup of every input, including the original manifest.
backup = stage/'Before'
backup.mkdir(exist_ok=True)
if not (backup/manifest_path.name).exists():
    shutil.copy2(manifest_path, backup/manifest_path.name)
for e in selected:
    path = root/e['export']
    if not (backup/path.name).exists():
        shutil.copy2(path, backup/path.name)

bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
sys.path.insert(0, str(Path(__file__).resolve().parent))
from CityArtKit import (
    MATERIALS,
    box,
    combine,
    cylinder,
    export_one,
    factory,
    material,
    power_station,
    residential_high,
    water_treatment,
    initialize_materials,
)
initialize_materials()
EXPORT_DIR = str(stage/'Exports')
Path(EXPORT_DIR).mkdir(exist_ok=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1
scene.render.engine = 'BLENDER_WORKBENCH'
scene.render.resolution_x = scene.render.resolution_y = 800
scene.render.resolution_percentage = 100
scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'MATERIAL'
scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True
scene.display.shading.background_type = 'WORLD'
scene.world.color = (.10,.12,.15)
scene.view_settings.view_transform = 'Standard'
# Preview the actual accepted game palette, rather than the old source colors.
for slot, color in {'Roof':(.10,.14,.19), 'Trim':(.95,.62,.16),
                    'Residential':(.22,.58,.32), 'Power':(.70,.22,.08),
                    'Service':(.08,.52,.58)}.items():
    MATERIALS[slot].diffuse_color = (*color,1)
cam = bpy.data.cameras.new('D79ReviewCamera')
camera = bpy.data.objects.new('D79ReviewCamera', cam)
scene.collection.objects.link(camera)
scene.camera = camera
cam.type = 'ORTHO'

def render(obj, label, roof_detail=False):
    for o in scene.objects:
        if o.type == 'MESH': o.hide_render = o != obj
    aim = Vector((0,0,17 if 'Residential' in obj.name else 6.5))
    camera.location = aim + Vector((30,-44,23))
    camera.rotation_euler = (aim-camera.location).to_track_quat('-Z','Y').to_euler()
    cam.ortho_scale = 48 if 'Residential' in obj.name else 28
    if roof_detail:
        aim=Vector((0,0,37.0))
        camera.location=aim+Vector((18,-24,17))
        camera.rotation_euler=(aim-camera.location).to_track_quat('-Z','Y').to_euler()
        cam.ortho_scale=22
    scene.render.filepath = str(out/(label+'.png'))
    bpy.ops.render.render(write_still=True)

def imported(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(path))
    obj = next(o for o in set(bpy.data.objects)-before if o.type == 'MESH')
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    for slot in obj.material_slots:
        key=slot.material.name.split('.')[0]
        if key in MATERIALS: slot.material = MATERIALS[key]
    return obj

before_geometry = {}
for e in selected:
    obj = imported(backup/Path(e['export']).name)
    obj.data.calc_loop_triangles()
    before_geometry[e['asset_id']] = len(obj.data.loop_triangles)
    if e['asset_id'] in base_ids: render(obj, obj.name.split('.')[0]+'-Before')
    bpy.data.objects.remove(obj, do_unlink=True)
# A medium facade is the unchanged alignment reference.
reference = next(e for e in manifest['assets'] if e['asset_id']=='building.residential.medium.a')
obj = imported(root/reference['export'])
render(obj, 'Residential-Medium-Reference')
bpy.data.objects.remove(obj, do_unlink=True)

assets = {}
for asset_id, factory in zip(base_ids, (residential_high, power_station, water_treatment)):
    entry = next(e for e in selected if e['asset_id']==asset_id)
    name = Path(entry['export']).stem
    obj = factory(name)
    assets[asset_id] = obj
    render(obj, name+'-After')
    if asset_id == 'building.residential.high.a': render(obj, name+'-Rooftop', roof_detail=True)

# Reuse the D56 upgrade recipes so the refined base survives paid expansion.
for service in ('Power','Service'):
    for level, suffix in ((1,'Medium'),(2,'High')):
        asset_id = f'building.{service.lower()}.{suffix.lower()}.a'
        name = 'SM_Building_'+service+'_'+suffix+'_A'
        obj = assets[f'building.{service.lower()}.a'].copy()
        obj.data = obj.data.copy()
        scene.collection.objects.link(obj)
        obj.name = name
        parts = [obj]
        for x in (-4,4):
            if service == 'Power':
                parts += [box(name+'_Generator',(3,4,2.4),(x,0,10.8),'Power'),
                          box(name+'_Cap',(3.3,4.3,.25),(x,0,12.13),'Trim')]
                if level==2:
                    parts += [cylinder(name+'_Vent',.7,3,(x,0,13.7),'Trim'),
                              box(name+'_Conduit',(.3,6,.3),(x,0,8.1),'Trim')]
            else:
                parts += [box(name+'_FilterSupport',(.6,.6,5),(x,0,6.5),'Trim'),
                          cylinder(name+'_Filter',1.5,3,(x,0,10.5),'Service'),
                          cylinder(name+'_Lid',1.65,.25,(x,0,12.13),'Trim')]
                if level==2:
                    parts += [cylinder(name+'_UpperFilter',1.2,3,(x,0,13.75),'Service'),
                              box(name+'_Pipe',(.4,5,.4),(x,0,12.4),'Trim')]
        assets[asset_id] = combine(name, parts)
        if level==2: render(assets[asset_id], name+'-After')

char = material('Charred', (.19,.15,.13))
char_trim = material('CharredTrim', (.065,.06,.055))
originals = [e for e in manifest['assets'] if e['asset_id'].startswith('building.') and not e.get('damaged_variant')]
for asset_id, original in list(assets.items()):
    entry = next(e for e in selected if e['asset_id']==asset_id+'.damaged')
    obj = original.copy()
    obj.data = original.data.copy()
    scene.collection.objects.link(obj)
    obj.name = Path(entry['export']).stem
    height = max(v.co.z for v in obj.data.vertices)
    phase = next(i for i,e in enumerate(originals) if e['asset_id']==asset_id)
    for v in obj.data.vertices:
        if v.co.z > height*.45:
            v.co.z *= (.78+.08*math.sin(v.co.x*.55+v.co.y*.43) if entry.get('utility_upgrade')
                       else .72+.14*math.sin(v.co.x*.55+v.co.y*.43+phase))
    indices = [p.material_index for p in obj.data.polygons]
    obj.data.materials.clear()
    # D56 damage retains the base semantic slot names and maps them to charred
    # material instances at import. The starter derivatives use Charred slots.
    for m in (list(original.data.materials) if entry.get('utility_upgrade') else (char,char_trim)):
        obj.data.materials.append(m)
    for p,index in zip(obj.data.polygons,indices): p.material_index=index
    for colors in obj.data.color_attributes:
        for c in colors.data: c.color=(1,1,1,1)
    bm=bmesh.new();bm.from_mesh(obj.data)
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()
    assets[entry['asset_id']]=obj
    if asset_id in base_ids: render(obj, obj.name+'-After')

measurements=[]
updated=[]
bpy.context.view_layer.update()
for asset_id,obj in assets.items():
    obj.data.calc_loop_triangles()
    triangles=len(obj.data.loop_triangles)
    assert 0 < triangles <= 1200 and len(obj.data.materials)<=2, (obj.name,triangles)
    bm=bmesh.new();bm.from_mesh(obj.data)
    assert not any(f.calc_area()<1e-8 for f in bm.faces), obj.name
    boundaries=sum(e.is_boundary for e in bm.edges)
    bm.free()
    original=next(e for e in selected if e['asset_id']==asset_id)
    expected_slots=set(original['material_slots'])
    assert {m.name for m in obj.data.materials}==expected_slots, (asset_id,expected_slots)
    vertices=[obj.matrix_world@v.co for v in obj.data.vertices]
    assert abs(min(v.z for v in vertices))<1e-5
    if not original.get('damaged_variant'):
        # Footprint and entrance direction are not widened by detail.
        assert all(abs(a-b)<.02 for a,b in zip(obj.dimensions[:2],original['dimensions_m'][:2]))
    entry=dict(original)
    entry.update(source=str(source.relative_to(root)).replace('\\','/'),
                 dimensions_m=[round(v,4) for v in obj.dimensions],
                 provenance='Original project geometry; Tools/Create-CityBuildingRefinement.py (D79)',
                 building_refinement_task='D79')
    set_capability(entry, 'building_refinement')
    updated.append(entry)
    measurements.append(dict(asset_id=asset_id,triangles_before=before_geometry[asset_id],
        triangles=triangles,slots=len(obj.data.materials),boundary_edges=boundaries,
        dimensions_m=entry['dimensions_m']))
    export_one(obj, export_dir=EXPORT_DIR)
    obj.hide_render=False

# Check the real facade slabs, rather than only checking their authored numbers.
high=assets['building.residential.high.a']
shade_bottoms=sorted(set(round(v.co.z,3) for v in high.data.vertices
    if v.co.y < -6.6 and v.co.z>7 and abs(v.co.x)>2))
for center, half_height, window_center in ((9.2,.175,8),(17.2,.175,16),
    (25.2,.175,24),(33.2,.175,32),(14.0,.25,12),(30.0,.25,28)):
    assert round(center-half_height,3) in shade_bottoms
    assert center-half_height > window_center+.9
for band_center, lower_window in ((14.0,12),(30.0,28)):
    assert band_center+.25 < lower_window+4-.9, 'Wide band hits the next window row'
assert 30.0-.25 > 25.2+.175, 'Upper wide band overlaps the small shades'

# Layout the editable source as a gallery; export ground-centered pivots first.
for i,obj in enumerate(assets.values()): obj.location=(i%4*50,i//4*50,0)
bpy.ops.wm.save_as_mainfile(filepath=str(source))
draft=dict(task='D79',assets=updated,measurements=measurements,
    source_sha256=digest(source),manifest_sha256=digest(manifest_path),
    original_export_hashes={e['asset_id']:digest(root/e['export']) for e in selected},
    export_hashes={e['asset_id']:digest(stage/'Exports'/Path(e['export']).name) for e in updated},
    high_window_clearance_m=.125,wide_band_clearance_m=.85,
    wide_band_small_shade_clearance_m=4.375,
    unreal_imported=False,packaged=False)
(stage/'draft.json').write_text(json.dumps(draft,indent=2)+'\n',encoding='utf-8')
print('CITYSIM_D79_DRAFT_VALIDATED',len(updated))
