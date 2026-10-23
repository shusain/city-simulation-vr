"""Reconstruct #285's isolated regression input; never overwrite the capture.

Run with --source <original ReportedCity.json> --output <new directory>.
Only five civic/utility reservations change. Native tests validate current
catalog loading, actual shopping/medical journeys and current save/reload.
"""
import argparse
import hashlib
import json
from pathlib import Path

SOURCE_SHA = '72c2d4a0909a23135251a25953c8f1203eb297adadc39ac2126e46753481b64f'
ROOT = Path(__file__).resolve().parents[2]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    original = args.source.read_bytes()
    assert sha(original) == SOURCE_SHA, 'Original capture hash mismatch'
    target = args.output / 'CurrentCity.json'
    assert target.resolve() != args.source.resolve(), 'Never overwrite source'
    city = json.loads(original.decode('utf-8-sig'))
    manifests = {}
    sizes = {}
    for filename, keys in [
        ('city_utility_integration.json', {'power_base': 'Power', 'water_base': 'Service'}),
        ('city_civic_batch.json', {'police_base': 'Police', 'primary': 'PrimarySchool'}),
    ]:
        path = ROOT / 'ArtSource/Manifests' / filename
        raw = path.read_bytes()
        manifests[filename] = sha(raw)
        for asset in json.loads(raw)['assets']:
            if asset['key'] in keys:
                sizes[keys[asset['key']]] = tuple(asset['site_cells'])
    assert len(sizes) == 4
    roads = {(r['x'], r['y']) for r in city['roads']}
    occupied = set(roads)
    def cells(x, y, w, h):
        return {(a, b) for a in range(x, x+w) for b in range(y, y+h)}
    for building in city['buildings']:
        if building['definitionId'] not in sizes:
            occupied |= cells(building['x'], building['y'], building['width'], building['height'])
    changes = []
    for building in city['buildings']:
        if building['definitionId'] not in sizes:
            continue
        before = dict(building)
        w, h = sizes[building['definitionId']]
        if building['quarterTurns'] % 2:
            w, h = h, w
        candidates = []
        for y in range(city['gridHeight']-h+1):
            for x in range(city['gridWidth']-w+1):
                site = cells(x, y, w, h)
                if site & occupied:
                    continue
                if any((a+dx, b+dy) in roads for a, b in site
                       for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                    candidates.append((abs(x-before['x'])+abs(y-before['y']), y, x))
        assert candidates, f'No independent reservation for {building["entityId"]}'
        _, y, x = min(candidates)
        building.update(x=x, y=y, width=w, height=h)
        occupied |= cells(x, y, w, h)
        changes.append(dict(before=before, after=dict(building)))
    assert len(changes) == 5
    args.output.mkdir(parents=True, exist_ok=True)
    data = (json.dumps(city, indent=2) + '\n').encode()
    if target.exists():
        assert target.read_bytes() == data, 'Refuse to replace different fixture'
    else:
        target.write_bytes(data)
    receipt = dict(issue=285, source_sha256=SOURCE_SHA, output_sha256=sha(data),
                   catalog_sha256=sha((ROOT/'CitySimVR/Content/CitySim/Data/DA_CityBuildingCatalog.uasset').read_bytes()),
                   manifests=manifests, changes=changes,
                   preserved='All other JSON values, entity IDs, roads, residents, shopping and simulation state',
                   native_validation='Required separately; this reconstruction is not a native pass')
    (args.output/'Reconstruction.json').write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf-8')
    assert args.source.read_bytes() == original
    print(json.dumps(dict(output=str(target), sha256=sha(data), changed_reservations=len(changes))))


if __name__ == '__main__':
    main()
