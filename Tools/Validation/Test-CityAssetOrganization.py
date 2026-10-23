"""Offline source identity, approval and maintained-entrypoint checks."""
from pathlib import Path
import hashlib
import importlib.util
import json
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools'))
from Assets.Manifests.CityApprovedSources import verified_export
from Assets.Manifests.CityEditorialArt import editorial_entries
PLAN = json.loads((ROOT / 'Tools/Assets/Manifests/CityAssetOrganization.json').read_text())
RETIRED_AUDIO = {
    '/Game/CitySim/Audio/Soundscape/Ambience/City/SW_Car_Pass',
    '/Game/CitySim/Audio/Soundscape/Ambience/City/SW_Truck_Pass',
    '/Game/CitySim/Audio/Soundscape/Graphs/MS_Car_Pass',
    '/Game/CitySim/Audio/Soundscape/Graphs/MS_Truck_Pass',
}
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()


class OrganizationTests(unittest.TestCase):
    def test_editorial_roles_are_independent_of_issue_numbers(self):
        doc = json.loads((ROOT/'ArtSource/Derived/EditorialArt/manifest.json').read_text())
        entries = editorial_entries(doc)
        self.assertEqual(entries['housing']['usage'], 'newspaper_illustration')
        self.assertEqual(entries['repairs']['usage'], 'repair_display')
        for row in doc['assets']:
            row['issue'] = 999
        self.assertEqual(editorial_entries(doc)['housing']['usage'], 'newspaper_illustration')

    def test_editorial_roles_reject_task_only_or_mismatched_metadata(self):
        doc = json.loads((ROOT/'ArtSource/Derived/EditorialArt/manifest.json').read_text())
        del doc['assets'][0]['usage']
        with self.assertRaisesRegex(ValueError, 'usage'):
            editorial_entries(doc)
        doc['assets'][0]['usage'] = 'repair_display'
        with self.assertRaisesRegex(ValueError, 'usage'):
            editorial_entries(doc)

    def test_editorial_roles_reject_duplicate_keys(self):
        doc = json.loads((ROOT/'ArtSource/Derived/EditorialArt/manifest.json').read_text())
        doc['assets'][1]['key'] = doc['assets'][0]['key']
        with self.assertRaisesRegex(ValueError, 'uniquely'):
            editorial_entries(doc)

    def test_unique_semantic_destinations(self):
        rows = PLAN['native']
        self.assertEqual(len(rows), 247)
        self.assertEqual(len({r['new'] for r in rows}), len(rows))
        for row in rows:
            self.assertRegex(row['new'], r'^/Game/CitySim/[A-Za-z0-9_/]+$')
            self.assertNotRegex(row['new'], r'(?:/|_)D\d+(?:/|_|$)')
            old = ROOT / ('CitySimVR/Content/' + row['old'].removeprefix('/Game/') + '.uasset')
            new = ROOT / ('CitySimVR/Content/' + row['new'].removeprefix('/Game/') + '.uasset')
            if row['new'] in RETIRED_AUDIO:
                self.assertFalse(old.exists())
                self.assertFalse(new.exists(), 'Unused legacy pass-by retired in #55')
                continue
            self.assertNotEqual(old.exists(), new.exists(), row['new'])
            if old.exists():
                self.assertEqual(sha(old), row['sha256'])

    def test_relocated_binary_and_editable_payloads(self):
        recorded = verified_export(ROOT / 'AudioSource/Generated/recordings/issue-54/v1', 54, 1,
                                   'fd6977b44e1b325a81d055147a7e1c9d203a9799609b739ad5e32742bdc86f24', kind='audio')
        replacements = {e['runtimeFile']: e for e in recorded['assets']}
        stage = ROOT / 'AudioSource/Soundscape/Unreal'
        retired = {('AudioSource/Soundscape/Unreal/' + e['file']): e['sha256'] for e in
                   json.loads((stage / 'Manifest.json').read_text())['legacyTrafficRemoval']['removedSources']}
        for row in PLAN['sources']:
            old, new = ROOT / row['old'], ROOT / row['new']
            self.assertFalse(old.exists(), row['old'])
            if row['new'] in retired:
                self.assertEqual(retired[row['new']], row['sha256'], 'Historical retired source identity retained')
                self.assertFalse(new.exists(), row['new'])
                continue
            self.assertTrue(new.is_file(), row['new'])
            if new.suffix.lower() in ('.wav', '.mp3', '.fbx', '.blend', '.png', '.scd') or new.name in ('ApprovedProvenance.json', 'IntegrationReceipt.json', 'Handoff.json') or 'Acceptance' in new.parts:
                replacement = replacements.get(row['new'])
                if replacement:
                    self.assertEqual(replacement['oldSha256'], row['sha256'], 'Historical relocation identity retained')
                    self.assertEqual(sha(new), replacement['sha256'], row['new'])
                else:
                    self.assertEqual(sha(new), row['sha256'], row['new'])

    def test_no_numbered_runtime_asset_literals(self):
        if (ROOT / "Tools/CI/AssetPreparation.txt").exists():
            self.skipTest("Native preparation retains original constructor paths until prepared packages are committed")
        import re
        numbered = re.compile(r'"[^"\n]*(?:/Game/CitySim/|AS_Park_)[^"\n]*(?:/D\d+/|_D\d+)[^"\n]*"')
        for path in (ROOT / 'CitySimVR/Source').rglob('*'):
            if path.suffix in ('.cpp', '.h'):
                self.assertIsNone(numbered.search(path.read_text(encoding='utf-8-sig')), str(path))

    def test_accepted_soundscape_staging(self):
        path = ROOT / 'Tools/Audio/Soundscape/Stage-CityAudioSources.py'
        spec = importlib.util.spec_from_file_location('soundscape_stage', path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        manifest = module.validate_stage()
        self.assertEqual(len(manifest['assets']), 53)
        self.assertTrue(all(e['package'].startswith('/Game/CitySim/Audio/Soundscape/') for e in manifest['assets']))

    def test_texture_producers_import_without_writes_or_arguments(self):
        for name in ('Prepare-CityEditorialArt.py', 'Prepare-CityStorefrontRooms.py'):
            path = ROOT / 'Tools/Assets/Textures' / name
            spec = importlib.util.spec_from_file_location('texture_' + name, path)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            self.assertTrue(callable(module.main))

    def test_approval_contract(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            payload = directory / 'asset.png'
            payload.write_bytes(b'fixture')
            row = dict(name='asset.png', bytes=payload.stat().st_size, sha256=sha(payload))
            approval = dict(repository='CitySimVR/citysim', issue=33, version=1, kind='comfy', artifacts=[row])
            (directory / 'approval.json').write_text(json.dumps(approval))
            pinned = sha(directory / 'approval.json')
            handoff = dict(repository='CitySimVR/citysim', asset_issue=33, approved_version=1, kind='comfy', approval_sha256=pinned, artifacts=[row])
            (directory / 'unreal-handoff.json').write_text(json.dumps(handoff))
            self.assertEqual(verified_export(directory, 33, 1, pinned)['asset_issue'], 33)
            with self.assertRaisesRegex(ValueError, 'hash'):
                verified_export(directory, 33, 1, '0' * 64)
            with self.assertRaisesRegex(ValueError, 'identity'):
                verified_export(directory, 34, 1, pinned)
            payload.write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError, 'artifact'):
                verified_export(directory, 33, 1, pinned)

    def test_approval_rejects_traversal(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            row = dict(name='../outside.png', bytes=0, sha256='0' * 64)
            approval = dict(repository='CitySimVR/citysim', issue=33, version=1, kind='comfy', artifacts=[row])
            (directory / 'approval.json').write_text(json.dumps(approval))
            pinned = sha(directory / 'approval.json')
            handoff = dict(repository='CitySimVR/citysim', asset_issue=33, approved_version=1, kind='comfy', approval_sha256=pinned, artifacts=[row])
            (directory / 'unreal-handoff.json').write_text(json.dumps(handoff))
            with self.assertRaisesRegex(ValueError, 'escapes'):
                verified_export(directory, 33, 1, pinned)


if __name__ == '__main__':
    unittest.main()
