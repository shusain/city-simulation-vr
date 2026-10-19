"""Compatibility entry point; maintained implementation lives under Tools/Assets."""
from pathlib import Path
import sys
sys.path.insert(0, str(next(p for p in Path(__file__).resolve().parents if (p / "CityToolPaths.py").is_file())))
from Assets.Blender.CityAssetPreview import (
    VIEWS, digest, preview_scene, evaluated_copy, fit_camera, studio_lights, render_asset,
)
