"""Bake the fixed cube in a fully initialized native Editor, then exit."""
import unreal
from pathlib import Path
import hashlib
import json

root=Path(__file__).resolve().parents[1]
report=root/'Artifacts/D79-MetalD71/reflection-bake.json'
report.unlink(missing_ok=True)
try:
    assert unreal.CityMetalReflectionAssetsCommandlet.bake_room_reflection() == 0
    paths=[root/'CitySimVR/Content/CitySim/Maps'/name for name in ('CitySandbox.umap','CitySandbox_BuiltData.uasset')]
    report.write_text(json.dumps(dict(saved=True,live_recapture=False,
        files=[dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),bytes=p.stat().st_size) for p in paths]),indent=2)+'\n')
finally:
    unreal.SystemLibrary.quit_editor()
