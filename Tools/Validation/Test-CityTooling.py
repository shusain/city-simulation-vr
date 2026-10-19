"""Offline path and icon-contract tests. No Unreal/Blender process is launched."""
import importlib
import importlib.util
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import Mock, patch

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
from CityToolPaths import repository_root


class RepositoryPaths(unittest.TestCase):
    def test_real_checkout_from_nested_file(self):
        self.assertEqual(repository_root(__file__), TOOLS.parent)

    def test_relocated_nested_script_and_outside_directory(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / 'checkout with spaces'
            nested = root / 'Tools/Assets/Unreal/script.py'
            nested.parent.mkdir(parents=True)
            nested.touch()
            (root / 'CitySimVR').mkdir()
            (root / 'CitySimVR/CitySimVR.uproject').write_text('{}')
            self.assertEqual(repository_root(nested), root.resolve())
            self.assertEqual(repository_root(nested.parent), root.resolve())
            # CI may put TEMP inside its checkout; simulate absence of project
            # markers instead of depending on the host's temporary-directory policy.
            with patch.object(Path, 'is_file', return_value=False):
                with self.assertRaisesRegex(FileNotFoundError, 'No CitySim checkout'):
                    repository_root(temporary)


class IconContract(unittest.TestCase):
    def setUp(self):
        self.api = types.ModuleType('unreal')
        self.api.SystemLibrary = Mock()
        self.api.AssetToolsHelpers = Mock()
        self.api.EditorAssetLibrary = Mock()
        self.api.MaterialEditingLibrary = Mock()
        self.api.load_asset = Mock()
        self.api.log = Mock()
        self.api.AssetImportTask = lambda: types.SimpleNamespace(
            properties={}, set_editor_property=lambda key, value: None)
        self.patch = patch.dict(sys.modules, {'unreal': self.api})
        self.patch.start()
        sys.modules.pop('Assets.Unreal.CityUiIcons', None)
        self.icons = importlib.import_module('Assets.Unreal.CityUiIcons')

    def tearDown(self):
        sys.modules.pop('Assets.Unreal.CityUiIcons', None)
        self.patch.stop()

    def entry(self, name):
        spec = importlib.util.spec_from_file_location('test_entry', TOOLS / name)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def test_importing_helpers_and_entry_points_does_not_write_assets(self):
        self.entry('Create-CityUiIcons.py')
        self.entry('Import-CityConditionIcons.py')
        self.api.SystemLibrary.get_command_line.assert_not_called()
        self.api.AssetToolsHelpers.get_asset_tools.assert_not_called()
        self.api.EditorAssetLibrary.save_loaded_asset.assert_not_called()

    def test_explicit_texture_selection_and_import_failure(self):
        tasks = []

        class Task:
            def __init__(self):
                self.properties = {}
                tasks.append(self)

            def set_editor_property(self, key, value):
                self.properties[key] = value

            def get_editor_property(self, key):
                return self.properties.get(key)

        self.api.AssetImportTask = Task
        def import_tasks(batch):
            for task in batch:
                task.properties['imported_object_paths'] = ['/Game/imported']
        importer = self.api.AssetToolsHelpers.get_asset_tools.return_value.import_asset_tasks
        importer.side_effect = import_tasks
        self.icons.import_textures('approved', ('build', 'local_police'))
        self.assertEqual([t.properties['destination_name'] for t in tasks], ['T_Icon_build', 'T_Icon_local_police'])
        self.assertTrue(all(t.properties['save'] and t.properties['replace_existing'] for t in tasks))
        importer.side_effect = None
        with self.assertRaisesRegex(RuntimeError, 'Icon import failed'):
            self.icons.import_textures('approved', ('missing',))

    def test_focused_command_never_creates_master_or_imports_full_bank(self):
        entry = self.entry('Create-CityUiIcons.py')
        self.api.SystemLibrary.get_command_line.return_value = '-CityConditionIconsOnly'
        with patch.object(entry, 'import_textures') as textures, patch.object(entry, 'ensure_instances') as instances, patch.object(entry, 'ensure_master') as master:
            entry.main()
            self.assertEqual(textures.call_args.args[1], ('build', 'construction', 'local_police'))
            self.assertEqual(instances.call_args.args[1], ('build', 'construction', 'local_police'))
            master.assert_not_called()

    def test_missing_existing_master_rejected(self):
        entry = self.entry('Create-CityUiIcons.py')
        self.api.SystemLibrary.get_command_line.return_value = '-CityConditionIconsOnly'
        self.api.load_asset.return_value = None
        with patch.object(entry, 'import_textures'), self.assertRaisesRegex(RuntimeError, 'full UI icon import'):
            entry.main()

    def test_full_command_selects_standard_bank(self):
        entry = self.entry('Create-CityUiIcons.py')
        self.api.SystemLibrary.get_command_line.return_value = ''
        with patch.object(entry, 'import_textures') as textures, patch.object(entry, 'ensure_instances') as instances, patch.object(entry, 'ensure_master') as master:
            entry.main()
            self.assertEqual(len(textures.call_args.args[1]), 11)
            master.assert_called_once_with()
            self.assertEqual(instances.call_args.args, (master.return_value, self.icons.DEFAULT_ICON_NAMES))

    def test_pinned_approved_inputs_select_all_condition_icons(self):
        entry = self.entry('Import-CityConditionIcons.py')
        with patch.object(entry, 'import_textures') as textures, patch.object(entry, 'ensure_instances') as instances:
            entry.main()
            self.assertEqual(len(textures.call_args.args[1]), 13)
            self.assertEqual(instances.call_args.args[1], textures.call_args.args[1])
            self.assertIn('account_balance_wallet', textures.call_args.args[1])

    def test_approved_manifest_guard_runs_before_any_native_import(self):
        entry = self.entry('Import-CityConditionIcons.py')
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'ThirdParty/MaterialSymbols'
            source.mkdir(parents=True)
            (source / 'D116-Sources-v1.json').write_text('{"tampered": true}')
            with patch.object(entry, 'repository_root', return_value=root), patch.object(entry, 'import_textures') as textures:
                with self.assertRaisesRegex(RuntimeError, 'user-approved version'):
                    entry.main()
                textures.assert_not_called()


if __name__ == '__main__':
    unittest.main(verbosity=2)
