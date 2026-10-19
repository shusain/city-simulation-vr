"""Create the standard UI icon bank, or update its condition-icon subset."""
from pathlib import Path
import sys
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from CityToolPaths import repository_root
from Assets.Unreal.CityUiIcons import (
    MASTER_PATH, ASSET_PATH, DEFAULT_ICON_NAMES, CONDITION_ICON_NAMES,
    import_textures, ensure_master, ensure_instances,
)


def main():
    condition_only = '-CityConditionIconsOnly' in unreal.SystemLibrary.get_command_line()
    names = CONDITION_ICON_NAMES if condition_only else DEFAULT_ICON_NAMES
    source = repository_root(__file__) / "ThirdParty/MaterialSymbols/Derived"
    import_textures(str(source), names)
    master = unreal.load_asset(MASTER_PATH) if condition_only else ensure_master()
    if not master:
        raise RuntimeError('Run the full UI icon import before the focused condition import')
    ensure_instances(master, names)
    unreal.log(f"CITYSIM_UI_ICONS_READY Count={len(names)} Path={ASSET_PATH}")


if __name__ == "__main__":
    main()
