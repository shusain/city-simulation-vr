"""Deferred focused SoundWave import; run only with the exclusive D58 native window."""

import sys
from pathlib import Path
sys.path.insert(0, str(next(p for p in Path(__file__).resolve().parents if (p / "CityToolPaths.py").is_file())))
from CityToolPaths import repository_root
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
ROOT = repository_root(__file__)
STAGE = ROOT / "AudioSource/Soundscape/Unreal"
REPORT = ROOT / "Artifacts/D58/UnrealImport/ImportReport.json"
SOURCE_TAG = "CitySimD58SourceSHA256"


def main():
    tasks = (ROOT / "docs/DEVELOPMENT_TASKS.md").read_text(encoding="utf-8")
    owners = re.findall(r"\*\*Native window OWNED BY ([A-Z]+\d+)\b", tasks)
    if owners != ["D58"]:
        raise RuntimeError(f"D58 import requires its recorded native window; current owners: {owners}")
    spec = importlib.util.spec_from_file_location("d58_stage", ROOT / "Tools/Stage-CityAudioSources.py")
    stage = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(stage)
    manifest = stage.validate_stage()
    import unreal

    library = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    # Preflight every existing target before any asset import/settings mutation.
    existing = {}
    for entry in manifest["assets"]:
        package = entry["package"]
        if library.does_asset_exist(package):
            sound = library.load_asset(package)
            if not isinstance(sound, unreal.SoundWave) or library.get_metadata_tag(sound, SOURCE_TAG) != entry["sha256"]:
                raise RuntimeError(f"Refusing to overwrite an unowned or differently sourced asset: {package}")
            existing[package] = sound
    records = []
    for entry in manifest["assets"]:
        package = entry["package"]
        action = "Verified existing D58 wave" if package in existing else "Imported new D58 wave"
        sound = existing.get(package)
        if sound is None:
            task = unreal.AssetImportTask()
            for key, value in {
                "filename": str(stage.inside(STAGE, entry["file"])),
                "destination_path": package.rsplit("/", 1)[0], "destination_name": entry["assetName"],
                "automated": True, "replace_existing": False, "replace_existing_settings": False, "save": False,
            }.items():
                task.set_editor_property(key, value)
            factory = unreal.SoundFactory()
            factory.set_editor_property("auto_create_cue", False)
            task.set_editor_property("factory", factory)
            tools.import_asset_tasks([task])
            sound = library.load_asset(package)
        if not isinstance(sound, unreal.SoundWave):
            raise RuntimeError(f"Import did not create a SoundWave: {package}")
        if sound.get_editor_property("num_channels") != 1 or sound.get_editor_property("imported_sample_rate") != 48000:
            raise RuntimeError(f"Native import changed source format: {package}")
        duration = float(sound.get_editor_property("duration"))
        if abs(duration - entry["seconds"]) > 1 / 48000 + 1e-4:
            raise RuntimeError(f"Native import changed duration: {package} {duration}")
        settings = {"looping": entry["loop"], "volume": 1.0, "pitch": 1.0}
        if package in existing:
            for key, value in settings.items():
                if sound.get_editor_property(key) != value:
                    raise RuntimeError(f"Existing D58 settings differ; preserve and inspect: {package} {key}")
        else:
            if not unreal.CityEditorAssetLibrary.repoint_imported_source(sound, str(stage.inside(STAGE, entry["file"])), 0):
                raise RuntimeError(f"Could not store a portable source filename: {package}")
            for key, value in settings.items():
                sound.set_editor_property(key, value)
            library.set_metadata_tag(sound, SOURCE_TAG, entry["sha256"])
            library.set_metadata_tag(sound, "CitySimD58SourceID", entry["id"])
            if not library.save_loaded_asset(sound):
                raise RuntimeError(f"Could not save new D58 wave: {package}")
        native = ROOT / "CitySimVR/Content" / (package.removeprefix("/Game/") + ".uasset")
        if not native.is_file():
            raise RuntimeError(f"Saved native package missing: {package}")
        records.append({"id": entry["id"], "package": package, "action": action,
                        "sourceSha256": entry["sha256"], "nativeSha256": hashlib.sha256(native.read_bytes()).hexdigest(),
                        "seconds": duration, "looping": sound.get_editor_property("looping")})
        unreal.log(f"D58_SOUNDWAVE_IMPORT_PASS {entry['id']}")
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps({
        "status": "Pass", "task": "D58", "createdUTC": datetime.now(timezone.utc).isoformat(), "assets": records,
        "scope": "SoundWave import only; no game-state/event/mix/MetaSound/map/C++/cook-reference integration",
        "pending": ["Native compressed loop/phase/playback quality", "Runtime event/spatial/lifecycle/mix/saved controls", "Windows/Quest packaging and device/cost gates"],
    }, indent=2), encoding="utf-8")
    unreal.log("D58_SOUNDWAVE_IMPORT_COMPLETE assets=50")


if __name__ == "__main__":
    main()
