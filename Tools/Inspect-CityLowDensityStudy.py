"""Inspect the accepted low-density inputs without modifying their source."""
from pathlib import Path
import sys, json
import bpy
sys.path.insert(0, str(Path(__file__).resolve().parent))
from CityBuildingStudyIO import load_object
from CityParkGeometry import components

root = Path(__file__).resolve().parents[1]
manifest = json.loads((root/'ArtSource/Manifests/city_starter_kit.json').read_text())
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
report = []
for entry in manifest['assets']:
    if entry['asset_id'] not in ('building.residential.a', 'building.shop.a', 'building.factory.a'):
        continue
    obj = load_object(root/entry['source'], entry['mesh_destination'].rsplit('/',1)[1])
    pieces = []
    for indices in components(obj.data):
        coords = [obj.data.vertices[i].co for i in indices]
        low = [min(c[i] for c in coords) for i in range(3)]
        high = [max(c[i] for c in coords) for i in range(3)]
        faces = [p for p in obj.data.polygons if set(p.vertices).issubset(indices)]
        pieces.append(dict(vertices=sorted(indices), faces=[p.index for p in faces], low=low, high=high,
                           materials=sorted(set(obj.data.materials[p.material_index].name for p in faces))))
    report.append(dict(asset_id=entry['asset_id'], name=obj.name, pieces=pieces))
out = root/'Artifacts/D103'
out.mkdir(parents=True, exist_ok=True)
(out/'InputGeometry.json').write_text(json.dumps(report, indent=2)+'\n')
print('CITYSIM_D103_INPUT_INSPECTION_PASS', len(report))
