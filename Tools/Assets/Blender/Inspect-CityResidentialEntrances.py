"""Read-only #125 inventory of existing residential source components.

Blender --background --factory-startup --threads 2 --python this-file -- INPUTS OUTPUT
No source, native asset, catalog or player file is saved.
"""
from pathlib import Path
import hashlib
import json
import sys
import bpy

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'Tools'))
from CityBuildingStudyIO import load_object

inputs, output = map(Path, sys.argv[sys.argv.index('--') + 1:])
records = []
for entry in json.loads(inputs.read_text(encoding='utf-8')):
    source = ROOT / entry['source']
    assert hashlib.sha256(source.read_bytes()).hexdigest() == entry['source_sha256']
    obj = load_object(source, entry['object'])
    mesh = obj.data
    adjacency = [set() for _ in mesh.vertices]
    for edge in mesh.edges:
        a, b = edge.vertices
        adjacency[a].add(b)
        adjacency[b].add(a)
    components, unseen = [], set(range(len(mesh.vertices)))
    while unseen:
        stack = [unseen.pop()]
        vertices = set(stack)
        while stack:
            for other in adjacency[stack.pop()] & unseen:
                unseen.remove(other)
                vertices.add(other)
                stack.append(other)
        points = [mesh.vertices[i].co for i in vertices]
        low = [min(v[a] for v in points) for a in range(3)]
        high = [max(v[a] for v in points) for a in range(3)]
        faces = [p for p in mesh.polygons if set(p.vertices) <= vertices]
        components.append(dict(min=low, max=high, faces=[p.index for p in faces],
                               slots=sorted({mesh.materials[p.material_index].name.split('.')[0] for p in faces})))
    mesh.calc_loop_triangles()
    record = dict(entry, triangles=len(mesh.loop_triangles), components=components,
                  dimensions=list(obj.dimensions), materials=[m.name.split('.')[0] for m in mesh.materials])
    records.append(record)
    print('RESIDENTIAL_INVENTORY', entry['name'], len(components), 'parts;',
          'low parts:', [(c['min'], c['max'], c['slots']) for c in components if c['max'][2] < 2])
    bpy.data.objects.remove(obj, do_unlink=True)
    assert hashlib.sha256(source.read_bytes()).hexdigest() == entry['source_sha256']
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(records, indent=2), encoding='utf-8')
print('RESIDENTIAL_INVENTORY_PASS', len(records))
