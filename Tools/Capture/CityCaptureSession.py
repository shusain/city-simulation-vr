"""Temporary PIE capture ownership, deadlines and cleanup; no import-time Unreal calls."""
from pathlib import Path
import os
import time
import traceback


def capture_paths(root, name, output_directory=None):
    """Keep each requested output fresh and inside this checkout's Artifacts."""
    artifacts = (Path(root) / 'Artifacts').resolve()
    requested = output_directory or os.environ.get('CITYSIM_CAPTURE_OUTPUT')
    requested_path = Path(requested) if requested else artifacts / 'Preview'
    output = (requested_path if requested_path.is_absolute() else Path(root) / requested_path).resolve()
    if not output.is_relative_to(artifacts):
        raise ValueError('Capture outputs must stay inside this checkout Artifacts directory')
    if Path(name).name != name or name in ('.', '..'):
        raise ValueError('Capture name must be a plain filename stem')
    outputs = [output / f'{name}-{angle}.png' for angle in ('Front', 'Back')]
    report = output / f'{name}.json'
    error = report.with_suffix('.error.txt')
    for path in (*outputs, report, error):
        if path.exists():
            raise FileExistsError(f'Preserve existing capture evidence: {path}; choose a fresh CITYSIM_CAPTURE_OUTPUT')
    output.mkdir(parents=True, exist_ok=True)
    return outputs, report, error


class CaptureSession:
    """Own one callback and explicit temporary actors, never save a map or city.

    The caller must coordinate the native window and launch an isolated Editor.
    A pre-existing PIE session is rejected before acquiring any capture state.
    Feature-specific camera, geometry, simulation and screenshot steps stay in
    their entry points. Closing is idempotent and attempts every cleanup step.
    """

    def __init__(self, api, error_path, timeout_seconds=120, timeout_message='Capture timed out'):
        self.api = api
        self.level = api.get_editor_subsystem(api.LevelEditorSubsystem)
        if self.level.is_in_play_in_editor():
            raise RuntimeError('Capture requires an isolated Editor without an active PIE session')
        self.editor = api.get_editor_subsystem(api.UnrealEditorSubsystem)
        self.actors = api.get_editor_subsystem(api.EditorActorSubsystem)
        self.owned_actors = []
        self.error_path = Path(error_path)
        self.timeout_seconds = timeout_seconds
        self.timeout_message = timeout_message
        self.started = time.monotonic()
        self.handle = None
        self.closed = False

    def start(self, callback):
        if self.closed or self.handle is not None:
            raise RuntimeError('Capture session can only start once')

        def tick(delta):
            if self.closed:
                return
            try:
                if time.monotonic() - self.started > self.timeout_seconds:
                    raise TimeoutError(self.timeout_message)
                callback(delta)
            except Exception:
                try:
                    self._record_error(traceback.format_exc())
                finally:
                    self.finish()

        try:
            self.handle = self.api.register_slate_post_tick_callback(tick)
        except Exception:
            try:
                self._record_error(traceback.format_exc())
            finally:
                self.finish()
            raise
        return self.handle

    def _record_error(self, message):
        self.api.log_error(message)
        try:
            with self.error_path.open('a', encoding='utf-8') as stream:
                stream.write(message + '\n')
        except OSError as error:
            self.api.log_error(f'Could not retain capture error at {self.error_path}: {error}')

    def finish(self):
        if self.closed:
            return
        self.closed = True
        actions = []
        if self.handle is not None:
            actions.append(lambda: self.api.unregister_slate_post_tick_callback(self.handle))
        actions.append(self.level.editor_request_end_play)
        actions.extend(lambda actor=actor: self.actors.destroy_actor(actor) for actor in self.owned_actors)
        self.owned_actors.clear()
        for action in actions:
            try:
                action()
            except Exception:
                self._record_error(traceback.format_exc())
        self.api.SystemLibrary.quit_editor()
