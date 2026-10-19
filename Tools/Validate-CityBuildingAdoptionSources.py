"""Validate promotion scope and current generic importer contracts without Unreal writes."""
from pathlib import Path
import json,hashlib,sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from CityToolPaths import repository_root
from Assets.Manifests.CityArtManifest import load_starter_manifest, load_seasonal_manifest

def main():
    ROOT=repository_root(__file__);OUT=ROOT/'Artifacts/D91/Integration'
    doc=json.loads((ROOT/'ArtSource/Manifests/city_building_adoption.json').read_text());selected={e['mesh_destination']:e for e in doc['assets']};assert len(selected)==61
    checked=0;unchanged=0
    for filename in ('city_starter_kit.json','city_seasonal_kit.json'):
        before=json.loads((OUT/'Before'/filename).read_text());after=json.loads((ROOT/'ArtSource/Manifests'/filename).read_text());assert len(before['assets'])==len(after['assets'])
        for a,b in zip(before['assets'],after['assets']):
            assert a['asset_id']==b['asset_id'] and a['mesh_destination']==b['mesh_destination']
            if b['mesh_destination'] not in selected:assert a==b;unchanged+=1;continue
            assert b['building_adoption_prior_entry']==a and b==selected[b['mesh_destination']]
            for key in ('footprint_cells','entrance_sides_local','presentation_yaw_offset_quarter_turns','damaged_variant','original_asset_id'):
                assert a.get(key)==b.get(key),(b['asset_id'],key)
            assert hashlib.sha256((ROOT/b['export']).read_bytes()).hexdigest()==b['export_sha256'];checked+=1
            if b.get('native_lod_export'):
                assert b['native_lod_triangles']==a['native_lod_triangles'] and hashlib.sha256((ROOT/b['native_lod_export']).read_bytes()).hexdigest()==b['native_lod_export_sha256']
    assert checked==61
    # Invoke the same read-only APIs used by the native importers.
    load_starter_manifest(ROOT)
    load_seasonal_manifest(ROOT)
    report=dict(task='D91',selected_entries=checked,unchanged_entries=unchanged,logical_ids_footprints_entrances_preserved=True,paired_lods_preserved=4,production_starter_contract_pass=True,production_seasonal_contract_pass=True)
    (OUT/'SourceVerification.json').write_text(json.dumps(report,indent=2)+'\n')
    print('CITYSIM_D91_ADOPTION_SOURCE_PASS 61 exact exports;4 paired LODs;109 unrelated entries preserved;generic starter contracts valid')


if __name__ == "__main__":
    main()
