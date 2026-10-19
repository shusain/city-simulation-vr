"""Original Blender civic meshes; reuse the starter kit's export/slot contract."""
from pathlib import Path
import bpy
import json
import sys
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'Tools'))
from CityArtKit import export_one
from CityCivicMeshes import station, emergency_vehicle, initialize_civic_materials


def main():
    initialize_civic_materials()
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    bpy.context.scene.unit_settings.system = 'METRIC'
    bpy.context.scene.unit_settings.scale_length = 1.0
    assets = [station('SM_Building_Police_A','Police'), station('SM_Building_Fire_A','Fire',True),
              emergency_vehicle('SM_Traffic_PoliceCar_A','Police'), emergency_vehicle('SM_Traffic_FireEngine_A','Fire',True)]
    out = root / 'ArtSource/Blender/CityCivicKit.blend'
    bpy.ops.wm.save_as_mainfile(filepath=str(out))
    entries = []
    for obj in assets:
        obj.data.calc_loop_triangles()
        count = len(obj.data.loop_triangles)
        budget = 800 if 'Building' in obj.name else 500
        if count > budget or len(obj.data.materials) > 2: raise RuntimeError(f'Civic budget exceeded: {obj.name}')
        export_one(obj)
        entries.append({'name':obj.name,'triangles':count,'dimensions_m':[round(v,4) for v in obj.dimensions],
                        'slots':[m.name for m in obj.data.materials]})
    report = root / 'Artifacts/AssetImport/city_civic_geometry.json'
    report.parent.mkdir(parents=True,exist_ok=True)
    report.write_text(json.dumps(entries,indent=2),encoding='utf-8')
    manifest_path = root / 'ArtSource/Manifests/city_starter_kit.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    by_name = {e['name']:e for e in entries}
    for asset in manifest['assets']:
        name = asset['mesh_destination'].rsplit('/',1)[-1]
        if name in by_name: asset['dimensions_m'] = by_name[name]['dimensions_m']
    manifest_path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print('CITYSIM_CIVIC_ART_SAVED',out)


if __name__ == '__main__':
    main()
