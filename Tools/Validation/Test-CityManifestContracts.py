"""Exercise shared manifest APIs and importer dry runs with a stub Unreal API."""
import importlib.util
import json
from pathlib import Path
import runpy
import sys
import tempfile
import types
import unittest
from unittest.mock import Mock, patch

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parent
sys.path.insert(0, str(TOOLS))
from Assets.Manifests.CityArtManifest import (
    load_starter_manifest, load_seasonal_manifest, ManifestValidationError,
)
from Assets.Manifests.CityAssetCapabilities import has_capability, set_capability


class ManifestContracts(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.directory = self.root / 'ArtSource/Manifests'
        self.directory.mkdir(parents=True)
        self.docs = {}
        for name in ('city_starter_kit.json', 'city_seasonal_kit.json'):
            doc = json.loads((ROOT / 'ArtSource/Manifests' / name).read_text())
            # Reuse current source-file existence, without copying or altering
            # approved source bytes. Only the test manifests are mutable.
            for entry in doc['assets']:
                for key in ('source', 'export', 'native_lod_export'):
                    if key in entry:
                        entry[key] = str((ROOT / entry[key]).resolve())
            self.docs[name] = doc
            self.write(name)

    def tearDown(self):
        self.temporary.cleanup()

    def write(self, name):
        (self.directory / name).write_text(json.dumps(self.docs[name]), encoding='utf-8')

    def test_current_production_manifests(self):
        starter, planned = load_starter_manifest(ROOT)
        seasonal = load_seasonal_manifest(ROOT)
        self.assertGreater(len(planned), 0)
        self.assertEqual(len(planned), len(starter['assets']))
        self.assertEqual(len(seasonal['assets']), 14)

    def test_duplicate_json_keys_rejected_in_both_contracts(self):
        for name, loader in (('city_starter_kit.json', load_starter_manifest), ('city_seasonal_kit.json', load_seasonal_manifest)):
            (self.directory / name).write_text('{"schema_version":1,"schema_version":1}')
            with self.assertRaisesRegex(RuntimeError, 'Duplicate'):
                loader(self.root)

    def test_starter_reports_duplicate_destinations_and_missing_source(self):
        entries = self.docs['city_starter_kit.json']['assets']
        entries[0]['mesh_destination'] = entries[1]['mesh_destination']
        entries[0]['source'] = str(self.root / 'missing.blend')
        self.write('city_starter_kit.json')
        with self.assertRaises(ManifestValidationError) as failure:
            load_starter_manifest(self.root)
        self.assertTrue(any('duplicate mesh_destination' in message for message in failure.exception.errors))
        self.assertTrue(any('Missing Blender source' in message for message in failure.exception.errors))

    def test_starter_geometry_budget_rejected(self):
        entry = next(e for e in self.docs['city_starter_kit.json']['assets'] if has_capability(e, 'reviewed_building'))
        entry['triangles'] = 100000
        self.write('city_starter_kit.json')
        with self.assertRaises(ManifestValidationError) as failure:
            load_starter_manifest(self.root)
        self.assertTrue(any('reviewed geometry contract' in message for message in failure.exception.errors))

    def test_seasonal_pivot_and_missing_export_rejected(self):
        entry = self.docs['city_seasonal_kit.json']['assets'][0]
        pivot = entry['pivot_policy']
        entry['pivot_policy'] = 'unapproved pivot'
        self.write('city_seasonal_kit.json')
        with self.assertRaises(AssertionError):
            load_seasonal_manifest(self.root)
        entry['pivot_policy'] = pivot
        entry['export'] = str(self.root / 'missing.fbx')
        self.write('city_seasonal_kit.json')
        with self.assertRaises(AssertionError):
            load_seasonal_manifest(self.root)

    def test_seasonal_entry_set_rejected(self):
        self.docs['city_seasonal_kit.json']['assets'].pop()
        self.write('city_seasonal_kit.json')
        with self.assertRaises(AssertionError):
            load_seasonal_manifest(self.root)

    def test_native_importer_dry_runs_use_shared_contracts_without_asset_writes(self):
        unreal = types.ModuleType('unreal')
        unreal.SystemLibrary = Mock()
        unreal.SystemLibrary.get_command_line.return_value = '-DryRun'
        unreal.log = Mock()
        unreal.log_error = Mock()
        # No AssetTools/EditorAssetLibrary attributes: any attempted native
        # operation fails. Reports are written only inside this temporary root.
        with patch.dict(sys.modules, {'unreal': unreal}), patch('CityToolPaths.repository_root', return_value=self.root):
            runpy.run_path(str(TOOLS / 'Import-CityArtKit.py'), run_name='__main__')
            runpy.run_path(str(TOOLS / 'Import-CitySeasonalArt.py'), run_name='__main__')
        unreal.log_error.assert_not_called()
        output = self.root / 'Artifacts/AssetImport'
        starter = json.loads((output / 'city_starter_kit_dry_run.json').read_text())
        seasonal = json.loads((output / 'D66-SeasonalImport.json').read_text())
        self.assertTrue(starter['dry_run'] and seasonal['dry_run'])
        self.assertEqual(starter['errors'], [])
        self.assertEqual(len(starter['planned']), len(self.docs['city_starter_kit.json']['assets']))
        self.assertEqual(seasonal['assets'], 14)
        self.assertEqual(starter['changed'], [])
        self.assertEqual(seasonal['measurements'], [])

    def test_historical_adoption_validator_is_importable_without_evidence_or_unreal(self):
        spec = importlib.util.spec_from_file_location('adoption_check', TOOLS / 'Validate-CityBuildingAdoptionSources.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        self.assertTrue(callable(module.main))

    def remove_task_labels(self):
        def scrub(value):
            if isinstance(value, dict):
                for key in list(value):
                    if key == 'task' or key.endswith('_task'):
                        value.pop(key)
                    else:
                        scrub(value[key])
            elif isinstance(value, list):
                for item in value:
                    scrub(item)
        for name, doc in self.docs.items():
            scrub(doc)
            self.write(name)

    def test_task_provenance_does_not_control_manifest_validation(self):
        self.remove_task_labels()
        starter, planned = load_starter_manifest(self.root)
        self.assertEqual(len(planned), 156)
        self.assertEqual(len(load_seasonal_manifest(self.root)['assets']), 14)
        # Reviewed geometry protection still applies without provenance labels.
        next(e for e in starter['assets'] if has_capability(e, 'reviewed_building'))['triangles'] = 100000
        self.docs['city_starter_kit.json'] = starter
        self.write('city_starter_kit.json')
        with self.assertRaises(ManifestValidationError):
            load_starter_manifest(self.root)

    def test_capabilities_fail_closed_and_provenance_never_grants_them(self):
        entry = dict(asset_id='test', building_surface_task='D79', building_adoption_task='D91')
        self.assertFalse(has_capability(entry, 'building_surfaces'))
        self.assertFalse(has_capability(entry, 'reviewed_building'))
        set_capability(entry, 'building_surfaces')
        set_capability(entry, 'reviewed_building')
        set_capability(entry, 'building_surfaces', False)
        self.assertEqual(entry['capabilities'], ['reviewed_building'])
        for invalid in ('building_surfaces', None, ['unknown'], ['building_surfaces'] * 2, [{}]):
            entry['capabilities'] = invalid
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                has_capability(entry, 'building_surfaces')
        with self.assertRaises(ValueError):
            set_capability({}, 'typo')

    def test_both_manifest_loaders_reject_unknown_capabilities(self):
        for name, loader in (('city_starter_kit.json', load_starter_manifest), ('city_seasonal_kit.json', load_seasonal_manifest)):
            self.docs[name]['assets'][0]['capabilities'] = ['typo']
            self.write(name)
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'invalid asset capabilities'):
                loader(self.root)

    def test_paired_lod_contract_survives_removed_provenance(self):
        self.remove_task_labels()
        for name, loader in (('city_starter_kit.json', load_starter_manifest), ('city_seasonal_kit.json', load_seasonal_manifest)):
            entry = next(e for e in self.docs[name]['assets'] if has_capability(e, 'paired_park_lods'))
            entry['native_lod_export'] = str(self.root / 'missing-lod.fbx')
            self.write(name)
            with self.subTest(name=name), self.assertRaises((ManifestValidationError, AssertionError)):
                loader(self.root)

    def test_focused_import_selection_uses_capabilities_without_task_labels(self):
        self.remove_task_labels()
        unreal = types.ModuleType('unreal')
        unreal.SystemLibrary = Mock()
        unreal.log = Mock()
        unreal.log_error = Mock()
        for flag, capability, count in (('-BuildingRefinementOnly', 'building_refinement', 8),
                                        ('-BuildingAdoptionOnly', 'reviewed_building', 60)):
            unreal.SystemLibrary.get_command_line.return_value = '-DryRun ' + flag
            with self.subTest(flag=flag), patch.dict(sys.modules, {'unreal': unreal}), patch('CityToolPaths.repository_root', return_value=self.root):
                state = runpy.run_path(str(TOOLS / 'Import-CityArtKit.py'), run_name='__main__')
                self.assertEqual(len(state['planned']), count)
                self.assertEqual({e['asset_id'] for e in state['import_entries']},
                                 {e['asset_id'] for e in self.docs['city_starter_kit.json']['assets'] if has_capability(e, capability)})
        unreal.log_error.assert_not_called()

    def test_damage_selection_preserves_reviewed_inputs_without_task_labels(self):
        # Import the real pure selector; no Blender API is available to execute.
        with patch.dict(sys.modules, {'bpy': types.ModuleType('bpy'), 'bmesh': types.ModuleType('bmesh')}):
            module = runpy.run_path(str(TOOLS / 'CityDamageArt.py'))
        original = module['select_inputs'](self.docs['city_starter_kit.json'], 'all')
        self.remove_task_labels()
        for scope in ('all', 'school', 'parks', 'station-upgrades'):
            selected, accepted = module['select_inputs'](self.docs['city_starter_kit.json'], scope)
            if scope == 'all':
                self.assertEqual((selected, accepted), original)
            self.assertTrue(all(not has_capability(e, 'reviewed_building') for e in selected))
        # A provenance field on an otherwise ordinary source cannot protect it.
        plain = dict(asset_id='building.example', building_surface_task='D79')
        self.assertEqual(module['select_inputs']({'assets': [plain]}, 'all')[0], [plain])

    def test_storefront_selection_uses_surfaces_without_task_labels(self):
        unreal = types.ModuleType('unreal')
        unreal.load_asset = Mock(side_effect=lambda path: types.SimpleNamespace(get_path_name=lambda: path))
        unreal.CityCommercialPresentationVariant = Mock(side_effect=Mock)
        with patch.dict(sys.modules, {'unreal': unreal}):
            module = runpy.run_path(str(TOOLS / 'CityStorefrontCatalog.py'))
        module['configure'].__globals__['ROOT'] = self.root
        self.remove_task_labels()
        catalog = Mock()
        definitions = [Mock()]
        properties = {'definitions': definitions}
        catalog.get_editor_property.side_effect = properties.__getitem__
        catalog.set_editor_property.side_effect = properties.__setitem__
        self.assertEqual(len(module['configure'](catalog)), 8)
        self.assertEqual(unreal.load_asset.call_count, 48)
        self.assertIs(properties['definitions'], definitions)
        unreal.load_asset.reset_mock()
        for entry in self.docs['city_starter_kit.json']['assets']:
            set_capability(entry, 'building_surfaces', False)
            entry['building_surface_task'] = 'D79'
        self.write('city_starter_kit.json')
        self.assertEqual(module['configure'](catalog), [])
        unreal.load_asset.assert_not_called()


if __name__ == '__main__':
    unittest.main(verbosity=2)
