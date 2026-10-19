"""Import the approved D82 bank, preserving all previous draft assets."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import wave

ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / "AudioSource/Music/CitySoundtrack/UnrealApproved"

def main():
    import unreal
    manifest = json.loads((STAGE/"Manifest.json").read_text())
    library = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    records = []
    if len(manifest["assets"]) != 9:
        raise RuntimeError("Expected the approved nine-track bank")
    # Preflight the complete bank before saving any asset.
    for entry in manifest["assets"]:
        source = (STAGE / entry["file"]).resolve()
        if source.parent != STAGE.resolve() or hashlib.sha256(source.read_bytes()).hexdigest() != entry["sha256"]:
            raise RuntimeError("Music source path/hash changed")
        if entry["package"] != f'/Game/CitySim/Audio/Music/Approved/SW_Music_{entry["id"]}':
            raise RuntimeError("Unexpected approved bank package")
        if library.does_asset_exist(entry["package"]):
            sound = library.load_asset(entry["package"])
            if not isinstance(sound, unreal.SoundWave) or library.get_metadata_tag(sound,"CitySimD82SHA256") != entry["sha256"]:
                raise RuntimeError("Refusing to overwrite unowned/different approved music")
    for entry in manifest["assets"]:
        source = STAGE / entry["file"]
        if source.parent != STAGE or hashlib.sha256(source.read_bytes()).hexdigest() != entry["sha256"]:
            raise RuntimeError("Music source path/hash changed")
        with wave.open(str(source)) as wav:
            if (wav.getnchannels(),wav.getframerate(),wav.getsampwidth(),wav.getnframes()) != (2,48000,2,entry["frames"]):
                raise RuntimeError("Music source format changed")
        package = entry["package"]
        existing = library.does_asset_exist(package)
        sound = library.load_asset(package) if existing else None
        if existing and (not isinstance(sound,unreal.SoundWave) or library.get_metadata_tag(sound,"CitySimD82SHA256") != entry["sha256"]):
            raise RuntimeError(f"Refusing to overwrite unowned/different music: {package}")
        if not existing:
            task = unreal.AssetImportTask()
            for key,value in {"filename":str(source),"destination_path":package.rsplit("/",1)[0],
                              "destination_name":package.rsplit("/",1)[1],"automated":True,"replace_existing":False,"save":False}.items():
                task.set_editor_property(key,value)
            factory = unreal.SoundFactory()
            factory.set_editor_property("auto_create_cue",False)
            task.set_editor_property("factory",factory)
            tools.import_asset_tasks([task])
            sound = library.load_asset(package)
            if not isinstance(sound,unreal.SoundWave):
                raise RuntimeError("Native music import failed")
            library.set_metadata_tag(sound,"CitySimD82SHA256",entry["sha256"])
            library.set_metadata_tag(sound,"CitySimD82ID",entry["id"])
        settings = {"looping":False,"volume":1.0,"pitch":1.0,"loading_behavior":unreal.SoundWaveLoadingBehavior.PRIME_ON_LOAD,
                    "sound_asset_compression_type":unreal.SoundAssetCompressionType.BINK_AUDIO,
                    "virtualization_mode":unreal.VirtualizationMode.PLAY_WHEN_SILENT}
        changed = not existing
        for key,value in settings.items():
            if sound.get_editor_property(key) != value:
                sound.set_editor_property(key,value)
                changed = True
        if changed:
            if not library.save_loaded_asset(sound):
                raise RuntimeError("Music asset save failed")
        if sound.get_editor_property("num_channels") != 2 or abs(sound.get_editor_property("duration")-entry["seconds"]) > 0.001:
            raise RuntimeError("Native music format/duration changed")
        native = ROOT/"CitySimVR/Content"/(package.removeprefix("/Game/")+".uasset")
        records.append({"id":entry["id"],"package":package,"nativeSha256":hashlib.sha256(native.read_bytes()).hexdigest(),
                        "seconds":entry["seconds"],"sourceSha256":entry["sha256"]})
        unreal.log(f'D82_MUSIC_IMPORT_PASS {entry["id"]}')
    report = ROOT/"Artifacts/D82/ApprovedIntegration/ImportReport.json"
    report.write_text(json.dumps({"status":"Pass","createdUTC":datetime.now(timezone.utc).isoformat(),"assets":records},indent=2))

if __name__ == "__main__":
    main()
