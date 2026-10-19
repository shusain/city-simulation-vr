"""Locate a CitySim checkout without assuming a script's directory depth."""
from pathlib import Path


def repository_root(start=__file__):
    """Find the nearest ancestor containing the project and Tools directory."""
    path = Path(start).resolve(strict=True)
    directory = path if path.is_dir() else path.parent
    for candidate in (directory, *directory.parents):
        if (candidate / "CitySimVR/CitySimVR.uproject").is_file() and (candidate / "Tools").is_dir():
            return candidate
    raise FileNotFoundError(f"No CitySim checkout above {path}")
