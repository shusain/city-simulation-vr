"""Import the explicitly approved v1 symbol unit through the existing icon contract."""
import hashlib
import json
from pathlib import Path
import sys
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from CityToolPaths import repository_root
from Assets.Unreal.CityUiIcons import MASTER_PATH, import_textures, ensure_instances

def main():
    root = repository_root(__file__)
    source = root / "ThirdParty/MaterialSymbols"
    approved_sha = "0aeb92d0f6cd97298ec42fd5390bd2c7ef8cb6139dda75d45d42dc5b34bba057"
    if hashlib.sha256((source / "D116-Sources-v1.json").read_bytes()).hexdigest() != approved_sha:
        raise RuntimeError("D116 source manifest differs from the user-approved version")
    approval = json.loads((source / "D116-Approval-v1.json").read_text())
    if approval["approved_manifest_sha256"] != approved_sha or approval["version"] != "v1":
        raise RuntimeError("D116 approval does not identify the selected source version")
    for entry in approval["artifacts"]:
        png = source / "Derived" / (entry["id"] + ".png")
        if hashlib.sha256(png.read_bytes()).hexdigest() != entry["png_sha256"]:
            raise RuntimeError("Reviewed derivative changed: " + entry["id"])
    manifest = json.loads((source / "D116-Sources-v1.json").read_text())
    for entry in manifest["artifacts"]:
        path = source / "svg" / (entry["id"] + ".svg")
        if hashlib.sha256(path.read_bytes()).hexdigest() != entry["source_sha256"]:
            raise RuntimeError("Approved SVG changed: " + entry["id"])

    master = unreal.load_asset(MASTER_PATH)
    if not master:
        raise RuntimeError("Existing M_CityUiIcon required")
    names = tuple(e["id"] for e in manifest["artifacts"])
    import_textures(str(source / "Derived"), names)
    ensure_instances(master, names)
    unreal.log("CITYSIM_CONDITION_ICONS_IMPORTED Count=13 ApprovedSourcesSHA256=" + approved_sha)


if __name__ == "__main__":
    main()
