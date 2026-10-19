"""Shared Blender damage generation; importing performs no scene/file changes."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from Assets.Manifests.CityAssetCapabilities import has_capability
import bpy
import bmesh
import json
import math
import os

# Scope owns selection and replacement together; output paths remain explicit.
SCOPE_OUTPUTS = {
    'all': ('CityDamageKit.blend', 'city_damage_geometry.json'),
    'school': ('CitySchoolDamageKit.blend', 'D46-school-damage-geometry.json'),
    'parks': ('CityParkDamageKit.blend', 'D47-park-damage-geometry.json'),
    'station-upgrades': ('CityStationDamageKit.blend', 'D48-station-damage-geometry.json'),
}


def in_scope(entry, scope):
    if scope == 'station-upgrades':
        return bool(entry.get('station_upgrade'))
    if scope in ('school', 'parks'):
        return entry.get('civic_service') == ('school' if scope == 'school' else 'park')
    if scope == 'all':
        return True
    raise ValueError('Unknown damage scope: ' + scope)


def job_path(root, path):
    path = Path(path)
    return Path(os.path.abspath(path if path.is_absolute() else root/path))


def select_inputs(manifest, scope):
    if scope not in SCOPE_OUTPUTS:
        raise ValueError('Unknown damage scope: ' + scope)
    reviewed_ids = {e['asset_id'] for e in manifest['assets'] if has_capability(e, 'reviewed_building')}
    accepted = {e['asset_id'] for e in manifest['assets'] if e.get('damaged_variant') and
                (e.get('original_asset_id') in reviewed_ids or has_capability(e, 'reviewed_building')
                 or has_capability(e, 'building_surfaces') or (has_capability(e, 'building_refinement') or has_capability(e, 'paired_park_lods')))}
    originals = [e for e in manifest['assets'] if e['asset_id'].startswith('building.') and not e.get('damaged_variant')
                 and not (has_capability(e, 'reviewed_building') or has_capability(e, 'building_surfaces')
                          or (has_capability(e, 'building_refinement') or has_capability(e, 'paired_park_lods')))
                 and e['asset_id'] + '.damaged' not in accepted and in_scope(e, scope)]
    return originals, accepted


def generate_damage(root, *, scope='all', manifest_path=None, source_path=None, export_directory=None, report_path=None):
    """Generate eligible derivatives and replace only unreviewed scope entries.

    Paths default to the existing source/export/report contracts. Override them
    for staged work; outputs must be beneath root to retain relative asset paths.
    No eligible originals means no scene reset, directory creation or file writes.
    """
    root = Path(os.path.abspath(root))
    if scope not in SCOPE_OUTPUTS:
        raise ValueError('Unknown damage scope: ' + scope)
    source_name, report_name = SCOPE_OUTPUTS[scope]
    manifest_path = job_path(root, manifest_path if manifest_path is not None else 'ArtSource/Manifests/city_starter_kit.json')
    source_path = job_path(root, source_path if source_path is not None else Path('ArtSource/Blender')/source_name)
    export_directory = job_path(root, export_directory if export_directory is not None else 'ArtSource/Exports')
    report_path = job_path(root, report_path if report_path is not None else Path('Artifacts/AssetImport')/report_name)
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    originals, accepted = select_inputs(manifest, scope)
    if not originals:
        print('CITYSIM_DAMAGE_ART_SKIPPED', scope, 'no eligible originals; reviewed assets preserved')
        return dict(scope=scope, generated=0, entries=[], measurements=[], source_unchanged=True, accepted_ids=sorted(accepted))
    # Check relative output references before modifying a scene or writing files.
    source_path.relative_to(root)
    export_directory.relative_to(root)
    report_path.relative_to(root)
    manifest_path.relative_to(root)
    for folder in (source_path.parent, export_directory, report_path.parent):
        folder.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
    bpy.context.scene.unit_settings.system='METRIC'; bpy.context.scene.unit_settings.scale_length=1
    char = bpy.data.materials.new('Charred'); char.diffuse_color=(.19,.15,.13,1)
    trim = bpy.data.materials.new('CharredTrim'); trim.diffuse_color=(.065,.06,.055,1)
    entries=[]; measurements=[]
    for i, entry in enumerate(originals):
        before=set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(root / entry['export']))
        meshes=[o for o in set(bpy.data.objects)-before if o.type=='MESH']
        if len(meshes)!=1: raise RuntimeError('Expected one imported mesh: '+entry['asset_id'])
        obj=meshes[0]; bpy.context.view_layer.objects.active=obj; obj.select_set(True)
        bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
        name=Path(entry['export']).stem+'_Damaged'; obj.name=name
        height=max(v.co.z for v in obj.data.vertices)
        # Keep the source shell closed. Removing top panels exposed the board through
        # hollow buildings; damage is conveyed by sagging geometry and charred tint.
        source_triangles = len(obj.data.polygons)
        bm=bmesh.new(); bm.from_mesh(obj.data)
        source_boundary_edges=sum(e.is_boundary for e in bm.edges)
        bm.free()
        for v in obj.data.vertices:
            if v.co.z>height*.45:
                v.co.z*=.72+.14*math.sin(v.co.x*.55+v.co.y*.43+i)
        bm=bmesh.new(); bm.from_mesh(obj.data)
        bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
        boundary_edges=sum(e.is_boundary for e in bm.edges)
        if boundary_edges != source_boundary_edges:
            raise RuntimeError('Damage introduced open boundaries: '+name)
        if any(f.calc_area()<1e-8 for f in bm.faces):
            raise RuntimeError('Degenerate damage face: '+name)
        bm.to_mesh(obj.data); bm.free(); obj.data.update()
        indices=[0 if poly.material_index==0 else 1 for poly in obj.data.polygons]
        obj.data.materials.clear(); obj.data.materials.append(char); obj.data.materials.append(trim)
        for poly,index in zip(obj.data.polygons,indices): poly.material_index=index
        for colors in obj.data.color_attributes:
            for c in colors.data: c.color=(1,1,1,1)
        bpy.context.view_layer.update(); obj.data.calc_loop_triangles()
        count=len(obj.data.loop_triangles)
        if count>1500: raise RuntimeError('Damage mesh exceeds shared building budget')
        bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True)
        export=export_directory / (name+'.fbx')
        bpy.ops.export_scene.fbx(filepath=str(export),use_selection=True,object_types={'MESH'},apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',mesh_smooth_type='FACE',axis_forward='-Y',axis_up='Z',add_leaf_bones=False)
        damaged={k:v for k,v in entry.items() if k not in ('progression_family','progression_stage','animation_role','roof_light_policy','window_policy','parked_vehicle_asset_id')}
        damaged.update(asset_id=entry['asset_id']+'.damaged',damaged_variant=True,original_asset_id=entry['asset_id'],source=source_path.relative_to(root).as_posix(),export=export.relative_to(root).as_posix(),mesh_destination=entry['mesh_destination']+'_Damaged',dimensions_m=[round(x,4) for x in obj.dimensions],material_slots={slot:'/Game/CitySim/Art/Materials/MI_City'+slot for index,slot in enumerate(('Charred','CharredTrim')) if index in set(indices)},provenance='Original authored building derivative; Create-CityDamageArt.py',lod_policy='shared damaged mesh; no more triangles than source',pivot_policy='ground center preserved from source')
        entries.append(damaged); measurements.append(dict(name=name,triangles=count,dimensions_m=damaged['dimensions_m'],source_boundary_edges=source_boundary_edges,boundary_edges=boundary_edges,source_faces=source_triangles,faces=len(obj.data.polygons)))
        obj.location.x=i*50 # Source gallery only; exported pivots remain ground center.
    bpy.ops.wm.save_as_mainfile(filepath=str(source_path))
    manifest['assets'] = [e for e in manifest['assets'] if not e.get('damaged_variant')
                          or e['asset_id'] in accepted or not in_scope(e, scope)] + entries
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    report_path.write_text(json.dumps(measurements, indent=2), encoding='utf-8')
    print('CITYSIM_DAMAGE_ART_SAVED', len(entries), scope)
    return dict(scope=scope, generated=len(entries), entries=entries, measurements=measurements,
                source_unchanged=False, accepted_ids=sorted(accepted))
