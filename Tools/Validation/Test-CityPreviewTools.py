"""Offline preview API/CLI checks with synthetic PNGs; no Blender/Unreal runs."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from PIL import Image

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
from Assets.Preview.CityPreviewSheets import compose


class PreviewTools(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='preview fixtures ')
        self.root = Path(self.temporary.name)
        self.manifest = self.root / 'PreviewManifest.json'
        self.output = self.root / 'sheets'
        self.doc = dict(schema_version=1, source='fixture.blend', source_sha256='fixture-hash',
                        source_unchanged=True, resolution=64, scene='Synthetic test scene', frame=0,
                        engine='workbench', color='material', views=[])
        for i in range(5):
            path = self.root / f'fixture {i}.png'
            Image.new('RGB', (64, 64), (i * 40, 70, 100)).save(path)
            self.doc['views'].append(dict(image=path.name, sha256=self.digest(path), object=f'Mesh {i}',
                                          view='iso', dimensions_m=[1, 2, 3], triangles=12))
        self.write_manifest()

    def tearDown(self):
        self.temporary.cleanup()

    @staticmethod
    def digest(path):
        return hashlib.sha256(path.read_bytes()).hexdigest()

    def write_manifest(self):
        self.manifest.write_text(json.dumps(self.doc), encoding='utf-8')

    def test_paginated_references_preserve_input_and_full_resolution_bytes(self):
        before = self.manifest.read_bytes()
        report = compose(self.manifest, self.output, columns=2, rows=1, tile_size=64)
        self.assertEqual([p['tiles'] for p in report['pages']], [2, 2, 1])
        self.assertEqual(list(report['references']), ['P01-A1', 'P01-B1', 'P02-A1', 'P02-B1', 'P03-A1'])
        for key, entry in report['references'].items():
            self.assertEqual((self.output / entry['image']).read_bytes(), Path(entry['source_image']).read_bytes())
            self.assertEqual(self.digest(self.output / entry['image']), entry['sha256'])
        for page in report['pages']:
            self.assertEqual(self.digest(self.output / page['file']), page['sha256'])
        self.assertEqual(self.manifest.read_bytes(), before)
        self.assertEqual(json.loads((self.output / 'PreviewSheets.json').read_text())['render_count'], 5)

    def test_invalid_last_image_rejects_entire_set_before_output(self):
        (self.root / self.doc['views'][-1]['image']).write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'Preview image changed'):
            compose(self.manifest, self.output)
        self.assertFalse(self.output.exists())

    def test_wrong_image_dimensions_rejected_before_output(self):
        path = self.root / self.doc['views'][-1]['image']
        Image.new('RGB', (32, 64)).save(path)
        self.doc['views'][-1]['sha256'] = self.digest(path)
        self.write_manifest()
        with self.assertRaisesRegex(ValueError, 'dimensions'):
            compose(self.manifest, self.output)
        self.assertFalse(self.output.exists())

    def test_unsuccessful_manifest_and_invalid_layout_rejected(self):
        for key, value in (('schema_version', 2), ('source_unchanged', False), ('views', [])):
            prior = self.doc[key]
            self.doc[key] = value
            self.write_manifest()
            with self.subTest(key=key), self.assertRaises(ValueError):
                compose(self.manifest, self.output)
            self.doc[key] = prior
        self.write_manifest()
        with self.assertRaises(ValueError):
            compose(self.manifest, self.output, columns=0)
        self.assertFalse(self.output.exists())

    def test_image_traversal_rejected_before_output(self):
        self.doc['views'][0]['image'] = '../outside.png'
        self.write_manifest()
        with self.assertRaisesRegex(ValueError, 'plain filenames'):
            compose(self.manifest, self.output)
        self.assertFalse(self.output.exists())

    def test_legacy_module_exports_the_canonical_compose_api(self):
        spec = importlib.util.spec_from_file_location('legacy_preview', TOOLS / 'Render-CityPreviewSheet.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        self.assertIs(module.compose, compose)
        self.assertFalse(self.output.exists())

    def test_legacy_and_canonical_cli_compose_from_paths_with_spaces(self):
        for script in (TOOLS / 'Render-CityPreviewSheet.py', TOOLS / 'Assets/Preview/CityPreviewSheets.py'):
            destination = self.root / script.stem
            result = subprocess.run([sys.executable, str(script), '--manifest', str(self.manifest),
                                     '--output', str(destination), '--columns', '2', '--rows', '1',
                                     '--tile-size', '64', '--title', 'Fixture "quoted" title'],
                                    capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('CITYSIM_PREVIEW_SHEETS_PASS 3 pages; 5 renders', result.stdout)
            self.assertEqual(len(json.loads((destination / 'PreviewSheets.json').read_text())['references']), 5)


if __name__ == '__main__':
    unittest.main(verbosity=2)
