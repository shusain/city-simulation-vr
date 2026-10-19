"""Compatibility entry point; maintained implementation lives under Tools/Assets."""
from pathlib import Path
import sys
sys.path.insert(0, str(next(p for p in Path(__file__).resolve().parents if (p / "CityToolPaths.py").is_file())))
from Assets.Preview.CityPreviewSheets import compose, font, fitted_text, main

if __name__ == "__main__":
    main()
