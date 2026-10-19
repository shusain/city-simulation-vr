"""Compatibility entry point; maintained implementation lives under Tools/Assets."""
from pathlib import Path
import runpy

if __name__ == "__main__":
    runpy.run_path(str(Path(__file__).resolve().parent / "Assets/Blender/Render-CityBlendPreviews.py"), run_name="__main__")
