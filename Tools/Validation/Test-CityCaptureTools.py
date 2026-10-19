"""Offline capture lifecycle/driver contracts; synthetic outputs are not render evidence."""
import importlib.util
import json
from pathlib import Path
import re
import runpy
import sys
import tempfile
import types
import unittest
from unittest.mock import Mock, patch

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
from Capture.CityCaptureSession import CaptureSession, capture_paths

SUBJECTS = ('Damage', 'Hospital', 'Park', 'School', 'SportsPark', 'Station', 'Waste')


class Vector(tuple):
    def __new__(cls, x, y, z):
        return super().__new__(cls, (x, y, z))

    x, y, z = (property(lambda self, index=index: self[index]) for index in range(3))

    def __sub__(self, other):
        return Vector(*(a - b for a, b in zip(self, other)))


def fake_unreal(active=False):
    api = types.ModuleType('unreal')
    api.LevelEditorSubsystem, api.UnrealEditorSubsystem, api.EditorActorSubsystem = 'level', 'editor', 'actors'
    api.StaticMeshActor, api.Actor, api.PrimitiveComponent, api.CitySimulationSubsystem = 'mesh', 'actor', 'primitive', 'simulation'
    api.Vector = Vector
    api.CityLightingPreset = types.SimpleNamespace(DAY='day')
    state = types.SimpleNamespace(active=active, spawned=[], callbacks={}, cameras=[], commands=[])
    world = object()
    level, editor, actors = Mock(), Mock(), Mock()
    level.is_in_play_in_editor.side_effect = lambda: state.active
    level.editor_request_begin_play.side_effect = lambda: setattr(state, 'active', True)
    level.editor_request_end_play.side_effect = lambda: setattr(state, 'active', False)
    editor.get_game_world.return_value = world
    subsystems = {'level': level, 'editor': editor, 'actors': actors}
    api.get_editor_subsystem = Mock(side_effect=lambda kind: subsystems[kind])

    def actor(name='StaticMeshActor'):
        result = Mock()
        result.get_class.return_value.get_name.return_value = name
        result.get_actor_label.return_value = name
        result.set_actor_label.side_effect = lambda label: setattr(result.get_actor_label, 'return_value', label)
        result.get_components_by_class.return_value = [Mock()]
        return result

    def spawn(*args):
        result = actor()
        state.spawned.append(result)
        return result
    actors.spawn_actor_from_class.side_effect = spawn
    board, ui = actor('CityBoardActor'), actor('CityVRToolController')
    api.GameplayStatics = Mock()
    api.GameplayStatics.get_all_actors_of_class.side_effect = lambda *args: [board, ui, *state.spawned]
    pawn, controller = Mock(), Mock()
    pawn.get_editor_property.return_value = 160
    api.GameplayStatics.get_player_pawn.return_value = pawn
    api.GameplayStatics.get_player_controller.return_value = controller
    api.MathLibrary = Mock()
    api.MathLibrary.find_look_at_rotation.side_effect = lambda location, aim: state.cameras.append((location, aim))
    simulation = Mock()
    simulation.get_outer.return_value = world
    api.ObjectIterator = lambda kind: [simulation]
    api.load_asset = Mock(side_effect=lambda path: types.SimpleNamespace(get_path_name=lambda: path))
    api.register_slate_post_tick_callback = Mock(side_effect=lambda tick: state.callbacks.setdefault('tick', tick) and 'tick')
    api.unregister_slate_post_tick_callback = Mock(side_effect=lambda handle: state.callbacks.pop(handle, None))
    api.log_error = Mock()
    api.SystemLibrary = Mock()

    def console(world, command):
        state.commands.append(command)
        if command.startswith('HighResShot'):
            # This only exercises file-completion logic in a temporary test root.
            Path(re.search(r'filename="([^"]+)"', command)[1]).write_bytes(b'synthetic capture fixture')
    api.SystemLibrary.execute_console_command.side_effect = console
    return api, state, level, actors


class CaptureContracts(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.error = self.root / 'capture.error.txt'
        self.api, self.state, self.level, self.actors = fake_unreal()

    def tearDown(self):
        self.temporary.cleanup()

    def test_successful_cleanup_is_once_and_only_destroys_owned_actors(self):
        session = CaptureSession(self.api, self.error)
        owned = [object(), object()]
        session.owned_actors.extend(owned)
        session.start(lambda delta: session.finish())
        callback = self.state.callbacks['tick']
        callback(0.1)
        session.finish()
        callback(0.1)
        self.assertEqual([call.args[0] for call in self.actors.destroy_actor.call_args_list], owned)
        self.api.unregister_slate_post_tick_callback.assert_called_once_with('tick')
        self.level.editor_request_end_play.assert_called_once_with()
        self.api.SystemLibrary.quit_editor.assert_called_once_with()
        self.assertFalse(self.error.exists())

    def test_active_PIE_is_rejected_without_interrupting_it(self):
        self.state.active = True
        with self.assertRaisesRegex(RuntimeError, 'without an active PIE'):
            CaptureSession(self.api, self.error)
        self.level.editor_request_end_play.assert_not_called()
        self.api.SystemLibrary.quit_editor.assert_not_called()
        self.api.register_slate_post_tick_callback.assert_not_called()

    def test_timeout_closes_even_when_capture_never_becomes_ready(self):
        with patch('time.monotonic', return_value=0):
            session = CaptureSession(self.api, self.error, timeout_seconds=120)
        callback = Mock()
        session.start(callback)
        with patch('time.monotonic', return_value=121):
            self.state.callbacks['tick'](0.1)
        callback.assert_not_called()
        self.assertIn('Capture timed out', self.error.read_text())
        self.api.SystemLibrary.quit_editor.assert_called_once_with()

    def test_callback_error_and_failed_actor_cleanup_retain_evidence_and_continue(self):
        session = CaptureSession(self.api, self.error)
        owned = [object(), object()]
        session.owned_actors.extend(owned)
        self.actors.destroy_actor.side_effect = [RuntimeError('cleanup failed'), None]
        session.start(Mock(side_effect=RuntimeError('capture failed')))
        self.state.callbacks['tick'](0.1)
        self.assertEqual(self.actors.destroy_actor.call_count, 2)
        self.assertIn('capture failed', self.error.read_text())
        self.assertIn('cleanup failed', self.error.read_text())
        self.api.SystemLibrary.quit_editor.assert_called_once_with()

    def test_unwritable_error_report_still_cleans_up(self):
        session = CaptureSession(self.api, self.root / 'missing parent/error.txt')
        session.start(Mock(side_effect=RuntimeError('capture failed')))
        self.state.callbacks['tick'](0.1)
        self.level.editor_request_end_play.assert_called_once_with()
        self.api.SystemLibrary.quit_editor.assert_called_once_with()
        self.assertTrue(any('Could not retain' in call.args[0] for call in self.api.log_error.call_args_list))

    def test_registration_failure_closes_and_cannot_restart(self):
        session = CaptureSession(self.api, self.error)
        self.api.register_slate_post_tick_callback.side_effect = RuntimeError('registration failed')
        with self.assertRaisesRegex(RuntimeError, 'registration failed'):
            session.start(Mock())
        self.api.SystemLibrary.quit_editor.assert_called_once_with()
        with self.assertRaisesRegex(RuntimeError, 'only start once'):
            session.start(Mock())

    def test_existing_outputs_are_preserved_and_outside_paths_rejected(self):
        with patch.dict('os.environ', {}, clear=True):
            outputs, report, error = capture_paths(self.root, 'Hospitals')
            outputs[0].write_bytes(b'original evidence')
            with self.assertRaisesRegex(FileExistsError, 'Preserve existing capture'):
                capture_paths(self.root, 'Hospitals')
            self.assertEqual(outputs[0].read_bytes(), b'original evidence')
            with self.assertRaisesRegex(ValueError, 'inside this checkout'):
                capture_paths(self.root, 'Hospitals', self.root / 'outside')
            with self.assertRaisesRegex(ValueError, 'plain filename'):
                capture_paths(self.root, '../escape')

    def test_explicit_and_environment_output_directories(self):
        output = self.root / 'Artifacts/Preview/another run'
        with patch.dict('os.environ', {'CITYSIM_CAPTURE_OUTPUT': str(output)}):
            outputs, _, _ = capture_paths(self.root, 'Hospitals')
        self.assertEqual(outputs[0].parent, output)
        explicit = self.root / 'Artifacts/Preview/explicit'
        self.assertEqual(capture_paths(self.root, 'Hospitals', explicit)[0][0].parent, explicit)
        self.assertEqual(capture_paths(self.root, 'Hospitals', 'Artifacts/Preview/relative')[0][0].parent,
                         self.root / 'Artifacts/Preview/relative')

    def test_importing_all_canonical_and_forwarding_entries_has_no_Unreal_side_effects(self):
        with patch.dict(sys.modules, {'unreal': self.api}):
            for subject in SUBJECTS:
                for folder in (TOOLS, TOOLS / 'Capture'):
                    spec = importlib.util.spec_from_file_location('preview_entry', folder / f'Capture-City{subject}Preview.py')
                    spec.loader.exec_module(importlib.util.module_from_spec(spec))
        self.api.get_editor_subsystem.assert_not_called()
        self.api.register_slate_post_tick_callback.assert_not_called()

    def test_all_seven_drivers_and_forwarders_complete_two_views_without_saving(self):
        manifest = json.loads((TOOLS.parent / 'ArtSource/Manifests/city_starter_kit.json').read_text(encoding='utf-8'))
        selected = {
            'Damage': lambda e: e.get('damaged_variant'),
            'Hospital': lambda e: e.get('hospital_asset'),
            'Park': lambda e: e.get('civic_service') == 'park',
            'School': lambda e: e.get('civic_service') == 'school',
            'SportsPark': lambda e: e.get('park_variant') in ('basketball', 'skate', 'baseball', 'soccer'),
            'Waste': lambda e: e.get('waste_asset'),
        }
        for subject in SUBJECTS:
            results = []
            for folder in (TOOLS / 'Capture', TOOLS):
                with self.subTest(subject=subject, folder=folder.name), tempfile.TemporaryDirectory() as temporary:
                    root = Path(temporary)
                    directory = root / 'ArtSource/Manifests'
                    directory.mkdir(parents=True)
                    (directory / 'city_starter_kit.json').write_text(json.dumps(manifest))
                    api, state, level, actors = fake_unreal()
                    clock = [0]
                    with patch.dict(sys.modules, {'unreal': api}), patch.dict('os.environ', {'CITYSIM_CAPTURE_OUTPUT': str(root / 'Artifacts/Preview')}), patch('CityToolPaths.repository_root', return_value=root), patch('time.monotonic', side_effect=lambda: clock[0]):
                        runpy.run_path(str(folder / f'Capture-City{subject}Preview.py'), run_name='__main__')
                        for second in range(1, 121):
                            if not state.callbacks:
                                break
                            clock[0] = second
                            state.callbacks['tick'](1)
                    self.assertFalse(state.callbacks, 'Capture session did not finish')
                    self.assertFalse(list(root.rglob('*.error.txt')))
                    reports = list((root / 'Artifacts/Preview').glob('*.json'))
                    self.assertEqual(len(reports), 1)
                    report = json.loads(reports[0].read_text())
                    self.assertFalse(report['saved_map'])
                    if subject != 'Station':
                        self.assertEqual([e['asset'] for e in report['assets']], [e['asset_id'] for e in manifest['assets'] if selected[subject](e)])
                    else:
                        self.assertEqual(len(report['assets']), 10)
                    self.assertEqual(len(list(root.rglob('*.png'))), 2)
                    self.assertEqual(len(state.cameras), 2)
                    self.assertEqual(actors.destroy_actor.call_count, len(state.spawned))
                    self.assertEqual([call.args[0] for call in actors.destroy_actor.call_args_list], state.spawned)
                    api.SystemLibrary.quit_editor.assert_called_once_with()
                    results.append((report, state.cameras))
            self.assertEqual(results[0], results[1], subject + ' forwarding changed behavior')


if __name__ == '__main__':
    unittest.main(verbosity=2)
