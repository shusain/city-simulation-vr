"""Offline publication regressions: config defaults and coordinated interfaces."""
import configparser
import json
from pathlib import Path
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]


def text(path):
    return (ROOT / path).read_text(encoding='utf-8-sig')


class PublicSourceTests(unittest.TestCase):
    def test_local_override_is_ignored_and_not_already_tracked(self):
        override = 'CitySimVR/Config/UserEngine.ini'
        self.assertEqual(subprocess.check_output(['git', 'ls-files', '--', override], cwd=ROOT), b'')
        self.assertEqual(subprocess.run(['git', 'check-ignore', '--quiet', override], cwd=ROOT).returncode, 0)
        self.assertTrue(subprocess.check_output(['git', 'ls-files', '--', 'CitySimVR/Config/Android/AndroidEngine.ini'], cwd=ROOT))

    def test_android_file_server_is_disabled_without_shared_credential(self):
        config = configparser.RawConfigParser(strict=False)
        config.read_string(text('CitySimVR/Config/DefaultEngine.ini'))
        afs = config['/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings']
        for key in ('bEnablePlugin', 'bAllowNetworkConnection', 'bIncludeInShipping', 'bAllowExternalStartInShipping'):
            self.assertEqual(afs[key].lower(), 'false', key)
        self.assertFalse(afs.get('SecurityToken', '').strip())

    def test_native_source_has_semantic_interfaces_and_comments(self):
        numbered = re.compile(r'(?<![A-Za-z0-9])(?:D\d{2,3}|B\d{3})(?!\d)|CityD\d+Preview')
        for path in (ROOT / 'CitySimVR/Source').rglob('*'):
            if path.suffix in ('.h', '.cpp'):
                self.assertIsNone(numbered.search(path.read_text(encoding='utf-8-sig')), str(path))

    def test_numbered_wrappers_retired_but_canonical_tools_remain(self):
        plan = json.loads(text('Tools/Assets/Manifests/CityAssetOrganization.json'))
        for old, new in plan['tools'].items():
            self.assertTrue((ROOT / 'Tools' / new).is_file(), new)
            if re.search(r'D\d+', old):
                self.assertFalse((ROOT / 'Tools' / old).exists(), old)
        self.assertFalse((ROOT / 'Tools/Import-D116ConditionIcons.py').exists())
        self.assertTrue((ROOT / 'Tools/Import-CityConditionIcons.py').is_file())

    def test_ci_and_fixture_consumers_use_same_flags(self):
        ci = text('Tools/CI/Invoke-CityCI.ps1')
        for flag, filename in (
            ('ShoppingAllocationSave', 'CityShoppingAllocationTests.cpp'),
            ('ShoppingAllocationOutput', 'CityShoppingAllocationTests.cpp'),
            ('OutpatientFixtureRoot', 'CityOutpatientVisualTests.cpp'),
        ):
            self.assertIn('-' + flag + '=', ci)
            self.assertIn('TEXT("' + flag + '=")', text('CitySimVR/Source/CitySimVR/Private/Tests/' + filename))
        self.assertIn('HomeInspection-Outpatient-', ci)

    def test_packaged_smoke_wrapper_matches_isolation_guard(self):
        for path in ('Tools/Test-DesktopSandbox.ps1', 'Tools/Test-TesterDistribution.ps1',
                     'CitySimVR/Source/CitySimVR/Private/Tests/CityDesktopPackagedSmoke.cpp'):
            self.assertIn('DesktopSmoke-Automation', text(path))

    def test_capture_inputs_match_native_fixture_export_names(self):
        for basename, producer, consumer in (
            ('ConditionClusters-City.json', 'CityConditionClusterTests.cpp', 'Capture-CityConditionClusters.py'),
            ('ConditionIndicators-Indicator-City.json', 'CityConditionIndicatorTests.cpp', 'Capture-CityConditionPreview.py'),
            ('Parks-Sports-City.json', 'CityParkTests.cpp', 'Capture-CityParkActivityPreview.py'),
        ):
            self.assertIn(basename, text('CitySimVR/Source/CitySimVR/Private/Tests/' + producer))
            self.assertIn(basename, text('Tools/' + consumer))

    def test_new_docs_link_actual_license_and_dependencies(self):
        self.assertIn('[City Sim VR Personal Use Source License](LICENSE)', text('README.md'))
        self.assertIn('Pillow', text('Tools/requirements.txt'))
        self.assertIn('CITYSIM_TOOL_PYTHON', text('CONTRIBUTING.md'))
        self.assertIn('Git history still contains', text('SECURITY.md'))


if __name__ == '__main__':
    unittest.main()
