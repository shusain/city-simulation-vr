"""CI compatibility entry point for the explicit semantic asset migration."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).resolve().parent / 'Assets/Unreal/Organize-CityAssets.py'), run_name='__main__')
