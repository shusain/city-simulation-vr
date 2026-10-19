"""Check delivered D104 source/native/paired-sheet evidence and protected inputs."""
from pathlib import Path
import argparse
import ast
import json
from PIL import Image, ImageStat
from CityUnrealPreview import ROOT, load_batch, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--batch', type=Path, required=True)
    args = parser.parse_args(); batch = load_batch(args.batch); out = ROOT/batch['output']
    source = json.loads((out/'SourceVerification.json').read_text())
    native = json.loads((out/'Import.json').read_text())
    delivery = json.loads((out/'Delivery.json').read_text())
    assert source['source_sha256'] == native['source_sha256'] == delivery['source_sha256'] == batch['source_sha256']
    assert source['geometry_uv_rgba_fixed'] and source['only_main_wall_bindings_changed'] and source['packed_images'] == 34
    assert len(native['meshes']) == len(batch['meshes']) == 12
    assert len(native['shader_statistics']) == len(batch['materials']) == 9
    for mesh in batch['meshes']:
        imported = next(e for e in native['meshes'] if e['name'] == mesh['name'])
        assert imported['triangles'] == mesh['triangles'] and imported['sections'] == mesh['sections']
        assert imported['slots'] == mesh['slots']
        assert digest(ROOT/mesh['export']) == mesh['export_sha256']
    assert delivery['paired_views'] == 68 and len(delivery['sheets']) == 12
    captures = 0; indexed = 0
    for renderer in ('Blender', 'Unreal'):
        report = json.loads((out/renderer/'Report.json').read_text())
        assert len(report['views']) == 68 and report['source_sha256'] == batch['source_sha256']
        for entry in report['views']:
            path = out/renderer/entry['group']/entry['view']/entry['image']
            assert digest(path) == entry['sha256']
            with Image.open(path) as image:
                assert image.size == (800, 800)
                assert max(ImageStat.Stat(image.convert('RGB')).stddev) > 8, 'Empty/flat capture'
                if renderer == 'Unreal':
                    # Neutral studio ground must fill the frame; reject the
                    # initial orthographic near-plane/finite-floor black bars.
                    for edge in (image.crop((0, 0, 800, 8)), image.crop((0, 792, 800, 800))):
                        assert max(ImageStat.Stat(edge.convert('RGB')).mean) > 20, 'Clipped studio background'
            if renderer == 'Unreal':
                assert abs(entry['ruler_height_cm']-143.175) < .2
                assert abs(entry['ruler_feet_cm']-entry['ruler_feet_m']*100) < .2
                fit = entry['camera']; scale = entry['camera_axis_recede_factor']
                expected = [a+(p-a)*scale for a, p in zip(fit['target_m'], fit['position_m'])]
                actual = [entry['camera_cm'][0]/100, -entry['camera_cm'][1]/100, entry['camera_cm'][2]/100]
                assert all(abs(a-b) < .0001 for a, b in zip(actual, expected)), 'Camera recede changed projection axis'
                assert entry['reflection_cubemap'] == batch['preview_environment']['native']
                assert entry['reflection_lightmap_mixing'] == 0
            captures += 1
        for group, view in sorted({(e['group'], e['view']) for e in batch['renders']}):
            directory = out/renderer/group/view
            index = json.loads((directory/'PreviewSheets.json').read_text())
            for record in index['references'].values():
                assert digest(directory/record['image']) == record['sha256']; indexed += 1
    protected = json.loads((out/'ProtectedInputs.json').read_text(encoding='utf-8-sig'))
    assert all(digest(path).upper() == value for path, value in protected.items())
    assert digest(ROOT/batch['approved_source']) == batch['approved_source_sha256']
    assert json.loads((out/'Compatibility/Verification.json').read_text())['d103_sheet_byte_identical']
    winding_fixes = {m['name']: m.get('native_winding_corrected_faces', 0) for m in batch['meshes'] if m.get('native_winding_corrected_faces')}
    assert winding_fixes == {'Home_D_Veranda': 6}
    winding = json.loads((out/'VerandaRepair/Source.json').read_text())
    assert winding['source_unchanged'] and winding['positions_uv_rgba_slots_preserved']
    names = ['CityUnrealPreview', 'Create-CityMaterialTextures', 'Create-CityMaterialStudy', 'Import-CityUnrealPreview',
        'Render-CityUnrealPreviews', 'Render-CityMaterialBlenderPreviews', 'Compose-CityMaterialComparison',
        'Validate-CityMaterialStudy', 'Verify-CityMaterialComparison', 'Render-CityPreviewSheet']
    for name in names: ast.parse((ROOT/'Tools'/(name+'.py')).read_text())
    result = dict(task='D104', status='Paired exploratory asset review verified', source=batch['source'],
        source_sha256=batch['source_sha256'], native_meshes=12, compiled_material_options=9,
        paired_views=68, verified_images=captures, verified_full_resolution_references=indexed, sheets=12,
        packed_source_images=34, protected_native_inputs=len(protected), approved_d103_preserved=True,
        geometry_uv_rgba_fixed=True, blender_composer_compatibility=True, python_sources_parsed=len(names),
        preview_bridge_winding_fixes=winding_fixes,
        production_changed=False, game_tested=False, physical_device_verified=False,
        limitations=batch['limitations'], next='User surface/lighting review; no automatic production promotion')
    (out/'Verification.json').write_text(json.dumps(result, indent=2)+'\n')
    print('CITYSIM_D104_DELIVERY_PASS 12 native meshes; 9 options; 136 captures; 12 sheets; protected inputs unchanged')


if __name__ == '__main__': main()
